#if !defined(USB_ONLY)
#include "modes.h"
#include "../net/sockets.h"
#include "../capture.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <switch/services/sfdnsres.h>

#define SHIS_CONFIG_PATH "/config/sysdvr/shis.ini"
#define SHIS_CONFIG_MAX 768
#define SHIS_DNS_MAX 0x400
#define SHIS_AUTH_MAX 320
#define SHIS_RELAY_HEADER_SIZE 20
#define SHIS_RETRY_NS 2000000000ULL
#define SHIS_AUDIO_WAIT_NS 100000000ULL
#define SHIS_QUEUE_WAIT_NS 1000000ULL

#define SHIS_KIND_VIDEO 1u
#define SHIS_KIND_AUDIO 2u

// Two full-size H.264 slots are enough to let capture continue while the
// network thread is sending the previous frame. If both slots fill we do not
// block GRC capture: queued P-frames are discarded and streaming resumes from
// the next IDR, avoiding reference-chain corruption.
#define SHIS_VIDEO_QUEUE_SLOTS 2
#define SHIS_SLOT_EMPTY 0u
#define SHIS_SLOT_QUEUED 1u
#define SHIS_SLOT_SENDING 2u

typedef struct {
    u8 state;
    u8 flags;
    u16 reserved;
    u32 size;
    u64 timestamp;
    u64 sequence;
    u8 data[VbufSz];
} ShisVideoSlot;

static char ConfigBuffer[SHIS_CONFIG_MAX];
static u8 DnsBuffer[SHIS_DNS_MAX];
static Mutex RelayMutex;
static int RelaySocket = SOCKET_INVALID;
static u32 RelayGeneration = 0;

static Mutex VideoQueueMutex;
static ShisVideoSlot VideoQueue[SHIS_VIDEO_QUEUE_SLOTS];
static u64 VideoQueueSequence = 0;
static bool VideoNeedsIDR = true;

static Thread SenderThread;
static u8 alignas(0x1000) SenderThreadStack[0x2000 + LOGGING_STACK_BOOST];

typedef struct {
    char host[128];
    u16 port;
    char stream[64];
    char key[160];
} ShisConfig;

static size_t BoundedStrlen(const char* s, size_t max)
{
    size_t i = 0;
    while (i < max && s[i]) i++;
    return i;
}

static char* Trim(char* s)
{
    while (*s == ' ' || *s == '\t' || *s == '\r') s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    *end = 0;
    return s;
}

static bool CopyValue(char* out, size_t outSize, const char* value)
{
    size_t len = strlen(value);
    if (!len || len >= outSize) return false;
    memcpy(out, value, len + 1);
    return true;
}

static bool ParsePort(const char* text, u16* out)
{
    u32 value = 0;
    if (!*text) return false;
    while (*text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10u + (u32)(*text - '0');
        if (value > 65535u) return false;
        text++;
    }
    if (!value) return false;
    *out = (u16)value;
    return true;
}

static bool LoadShisConfig(ShisConfig* cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    FsFileSystem fs;
    Result rc = fsOpenSdCardFileSystem(&fs);
    if (R_FAILED(rc)) return false;
    FsFile file;
    rc = fsFsOpenFile(&fs, SHIS_CONFIG_PATH, FsOpenMode_Read, &file);
    if (R_FAILED(rc)) { fsFsClose(&fs); return false; }
    s64 fileSize = 0;
    rc = fsFileGetSize(&file, &fileSize);
    if (R_FAILED(rc) || fileSize <= 0 || fileSize >= (s64)sizeof(ConfigBuffer)) {
        fsFileClose(&file); fsFsClose(&fs); return false;
    }
    u64 bytesRead = 0;
    rc = fsFileRead(&file, 0, ConfigBuffer, (u64)fileSize, 0, &bytesRead);
    fsFileClose(&file); fsFsClose(&fs);
    if (R_FAILED(rc) || bytesRead != (u64)fileSize) return false;
    ConfigBuffer[fileSize] = 0;

    char* cursor = ConfigBuffer;
    while (*cursor) {
        char* line = cursor;
        char* nl = strchr(cursor, '\n');
        if (nl) { *nl = 0; cursor = nl + 1; }
        else cursor += strlen(cursor);
        line = Trim(line);
        if (!*line || *line == '#' || *line == ';') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char* key = Trim(line);
        char* value = Trim(eq + 1);
        if (!strcmp(key, "relay_host")) {
            if (!CopyValue(cfg->host, sizeof(cfg->host), value)) return false;
        } else if (!strcmp(key, "relay_port")) {
            if (!ParsePort(value, &cfg->port)) return false;
        } else if (!strcmp(key, "stream")) {
            if (!CopyValue(cfg->stream, sizeof(cfg->stream), value)) return false;
        } else if (!strcmp(key, "stream_key")) {
            if (!CopyValue(cfg->key, sizeof(cfg->key), value)) return false;
        }
    }
    return cfg->host[0] && cfg->port && cfg->stream[0] && cfg->key[0];
}

static u16 ReadBE16(const u8* p) { return ((u16)p[0] << 8) | p[1]; }
static u32 ReadBE32(const u8* p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3]; }

static bool ResolveIPv4(const char* host, u32* outAddr)
{
    u32 hErr = 0, nErr = 0, serialized = 0;
    memset(DnsBuffer, 0, sizeof(DnsBuffer));
    Result rc = sfdnsresGetHostByNameRequest(0, false, host, &hErr, &nErr,
        DnsBuffer, sizeof(DnsBuffer), &serialized);
    if (R_FAILED(rc) || hErr != 0 || serialized < 16 || serialized > sizeof(DnsBuffer)) return false;
    const u8* p = DnsBuffer;
    const u8* end = DnsBuffer + serialized;
    size_t nameLen = BoundedStrlen((const char*)p, (size_t)(end - p));
    if (p + nameLen + 1 > end) return false;
    p += nameLen + 1;
    if (p + 4 > end) return false;
    u32 aliases = ReadBE32(p); p += 4;
    for (u32 i = 0; i < aliases; ++i) {
        size_t len = BoundedStrlen((const char*)p, (size_t)(end - p));
        if (p + len + 1 > end) return false;
        p += len + 1;
    }
    if (p + 8 > end) return false;
    u16 addrType = ReadBE16(p); p += 2;
    u16 addrLen = ReadBE16(p); p += 2;
    u32 count = ReadBE32(p); p += 4;
    if (addrType != AF_INET || addrLen != 4 || count == 0 || p + 4 > end) return false;
    u32 raw = 0;
    memcpy(&raw, p, sizeof(raw));
    *outAddr = ntohl(raw);
    return true;
}

static int ConnectRelay(const ShisConfig* cfg)
{
    u32 address = 0;
    if (!ResolveIPv4(cfg->host, &address)) return SOCKET_INVALID;
    int sock = bsdSocket(AF_INET, SOCK_STREAM, 0);
    if (sock == SOCKET_INVALID) return SOCKET_INVALID;
    int one = 1;
    (void)bsdSetSockOpt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->port);
    addr.sin_addr.s_addr = address;
    if (bsdConnect(sock, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        SocketClose(&sock); return SOCKET_INVALID;
    }

    char auth[SHIS_AUTH_MAX];
    const char prefix[] = "SHIS/1 ";
    size_t used = 0;
    size_t n = sizeof(prefix) - 1;
    memcpy(auth + used, prefix, n); used += n;
    n = strlen(cfg->stream);
    if (used + n + 1 >= sizeof(auth)) { SocketClose(&sock); return SOCKET_INVALID; }
    memcpy(auth + used, cfg->stream, n); used += n;
    auth[used++] = ' ';
    n = strlen(cfg->key);
    if (used + n + 1 >= sizeof(auth)) { SocketClose(&sock); return SOCKET_INVALID; }
    memcpy(auth + used, cfg->key, n); used += n;
    auth[used++] = '\n';
    if (!SocketSendAll(sock, auth, (u32)used)) { SocketClose(&sock); return SOCKET_INVALID; }
    char response[3] = {0};
    if (!SocketRecevExact(sock, response, sizeof(response)) || memcmp(response, "OK\n", 3) != 0) {
        SocketClose(&sock); return SOCKET_INVALID;
    }

    // Network backpressure must never stall the GRC capture thread. The sender
    // runs separately and uses SysDVR's poll-aware SocketSendAll on EAGAIN.
    if (!SocketMakeNonBlocking(sock)) {
        SocketClose(&sock); return SOCKET_INVALID;
    }
    return sock;
}

static void WriteLE32(u8* p, u32 v)
{
    p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

static void WriteLE64(u8* p, u64 v)
{
    for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (i * 8));
}

static bool HasIDR(const u8* data, u32 size)
{
    for (u32 i = 0; i + 5 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0) {
            if (i + 4 < size && data[i + 2] == 0 && data[i + 3] == 1 && (data[i + 4] & 0x1F) == 5) return true;
            if (i + 3 < size && data[i + 2] == 1 && (data[i + 3] & 0x1F) == 5) return true;
        }
    }
    return false;
}

static void InstallRelay(int sock)
{
    mutexLock(&RelayMutex);
    if (RelaySocket != SOCKET_INVALID) SocketClose(&RelaySocket);
    RelaySocket = sock;
    RelayGeneration++;
    mutexUnlock(&RelayMutex);
}

static void DropRelay(void)
{
    mutexLock(&RelayMutex);
    if (RelaySocket != SOCKET_INVALID) SocketClose(&RelaySocket);
    RelaySocket = SOCKET_INVALID;
    RelayGeneration++;
    mutexUnlock(&RelayMutex);
}

static bool GetRelayGeneration(u32* generation)
{
    bool connected;
    mutexLock(&RelayMutex);
    connected = RelaySocket != SOCKET_INVALID;
    *generation = RelayGeneration;
    mutexUnlock(&RelayMutex);
    return connected;
}

static bool SendFrame(u8 kind, u8 flags, u64 timestamp, const u8* data, u32 size)
{
    u8 header[SHIS_RELAY_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, "SHFR", 4);
    header[4] = kind;
    header[5] = flags;
    WriteLE64(header + 8, timestamp);
    WriteLE32(header + 16, size);

    bool ok = false;
    mutexLock(&RelayMutex);
    if (RelaySocket != SOCKET_INVALID) {
        ok = SocketSendAll(RelaySocket, header, sizeof(header)) && SocketSendAll(RelaySocket, data, size);
        if (!ok) {
            SocketClose(&RelaySocket);
            RelaySocket = SOCKET_INVALID;
            RelayGeneration++;
        }
    }
    mutexUnlock(&RelayMutex);
    return ok;
}

static void MarkVideoDiscontinuity(void)
{
    mutexLock(&VideoQueueMutex);
    VideoNeedsIDR = true;
    for (u32 i = 0; i < SHIS_VIDEO_QUEUE_SLOTS; ++i) {
        if (VideoQueue[i].state == SHIS_SLOT_QUEUED)
            VideoQueue[i].state = SHIS_SLOT_EMPTY;
    }
    mutexUnlock(&VideoQueueMutex);
}

static bool QueueVideoPacket(void)
{
    if ((VPkt.Header.MetaData & PacketMeta_Content_Data) == 0 || VPkt.Header.DataSize == 0)
        return true;

    const bool isIDR = HasIDR(VPkt.Data, VPkt.Header.DataSize);

    mutexLock(&VideoQueueMutex);

    // Once any frame is lost, dependent P-frames cannot be decoded reliably.
    // Wait for a clean random-access point instead of forwarding corruption.
    if (VideoNeedsIDR && !isIDR) {
        mutexUnlock(&VideoQueueMutex);
        return true;
    }

    if (VideoNeedsIDR && isIDR)
        VideoNeedsIDR = false;

    int empty = -1;
    for (u32 i = 0; i < SHIS_VIDEO_QUEUE_SLOTS; ++i) {
        if (VideoQueue[i].state == SHIS_SLOT_EMPTY) {
            empty = (int)i;
            break;
        }
    }

    if (empty < 0) {
        // Network sender fell behind. Never block CaptureReadVideo: discard any
        // frame that has not started sending and restart the dependency chain.
        for (u32 i = 0; i < SHIS_VIDEO_QUEUE_SLOTS; ++i) {
            if (VideoQueue[i].state == SHIS_SLOT_QUEUED)
                VideoQueue[i].state = SHIS_SLOT_EMPTY;
        }
        VideoNeedsIDR = true;

        for (u32 i = 0; i < SHIS_VIDEO_QUEUE_SLOTS; ++i) {
            if (VideoQueue[i].state == SHIS_SLOT_EMPTY) {
                empty = (int)i;
                break;
            }
        }

        if (!isIDR || empty < 0) {
            mutexUnlock(&VideoQueueMutex);
            return true;
        }
        VideoNeedsIDR = false;
    }

    ShisVideoSlot* slot = &VideoQueue[empty];
    slot->flags = isIDR ? 1u : 0u;
    slot->size = VPkt.Header.DataSize;
    slot->timestamp = VPkt.Header.Timestamp;
    slot->sequence = ++VideoQueueSequence;
    memcpy(slot->data, VPkt.Data, slot->size);
    slot->state = SHIS_SLOT_QUEUED;

    mutexUnlock(&VideoQueueMutex);
    return true;
}

static int AcquireQueuedVideo(void)
{
    int selected = -1;
    u64 lowestSequence = UINT64_MAX;

    mutexLock(&VideoQueueMutex);
    for (u32 i = 0; i < SHIS_VIDEO_QUEUE_SLOTS; ++i) {
        if (VideoQueue[i].state == SHIS_SLOT_QUEUED && VideoQueue[i].sequence < lowestSequence) {
            selected = (int)i;
            lowestSequence = VideoQueue[i].sequence;
        }
    }
    if (selected >= 0)
        VideoQueue[selected].state = SHIS_SLOT_SENDING;
    mutexUnlock(&VideoQueueMutex);

    return selected;
}

static void ReleaseVideoSlot(int index)
{
    if (index < 0 || index >= SHIS_VIDEO_QUEUE_SLOTS) return;
    mutexLock(&VideoQueueMutex);
    VideoQueue[index].state = SHIS_SLOT_EMPTY;
    mutexUnlock(&VideoQueueMutex);
}

static bool SendAudioPacket(void)
{
    if ((APkt.Header.MetaData & PacketMeta_Content_Data) == 0 || APkt.Header.DataSize == 0) return true;
    return SendFrame(SHIS_KIND_AUDIO, 0, APkt.Header.Timestamp, APkt.Data, APkt.Header.DataSize);
}

static void SHIS_SenderThread(void* unused)
{
    (void)unused;

    while (IsThreadRunning) {
        u32 generation = 0;
        if (!GetRelayGeneration(&generation)) {
            ShisConfig cfg;
            if (!LoadShisConfig(&cfg)) {
                svcSleepThread(SHIS_RETRY_NS);
                continue;
            }

            int relay = ConnectRelay(&cfg);
            if (relay == SOCKET_INVALID) {
                svcSleepThread(SHIS_RETRY_NS);
                continue;
            }

            MarkVideoDiscontinuity();
            InstallRelay(relay);
            continue;
        }

        int slotIndex = AcquireQueuedVideo();
        if (slotIndex < 0) {
            svcSleepThread(SHIS_QUEUE_WAIT_NS);
            continue;
        }

        ShisVideoSlot* slot = &VideoQueue[slotIndex];
        bool ok = SendFrame(SHIS_KIND_VIDEO, slot->flags, slot->timestamp, slot->data, slot->size);
        ReleaseVideoSlot(slotIndex);

        if (!ok) {
            MarkVideoDiscontinuity();
            DropRelay();
            if (IsThreadRunning) svcSleepThread(SHIS_RETRY_NS);
        }
    }
}

static void SHIS_VideoThread(void* unused)
{
    (void)unused;
    u32 activeGeneration = 0;

    CaptureSetNalHashing(false, false);
    CaptureSetPPSSPSInject(true);
    CaptureVideoConnected();

    while (IsThreadRunning) {
        bool valid = CaptureReadVideo();
        if (!IsThreadRunning) break;

        u32 generation = 0;
        bool connected = GetRelayGeneration(&generation);
        if (!connected) {
            activeGeneration = 0;
            MarkVideoDiscontinuity();
            continue;
        }

        if (generation != activeGeneration) {
            // Force SysDVR parameter injection on a new relay session and make
            // the queue wait for a fresh IDR before forwarding video.
            CaptureVideoConnected();
            MarkVideoDiscontinuity();
            activeGeneration = generation;
        }

        if (!valid) {
            // GRC can reject an oversized/corrupt frame. Never let subsequent
            // dependent P-frames reach the decoder after such a discontinuity.
            MarkVideoDiscontinuity();
            continue;
        }

        QueueVideoPacket();
    }
}

static void SHIS_AudioThread(void* unused)
{
    (void)unused;
    u32 activeGeneration = 0;
    while (IsThreadRunning) {
        u32 generation = 0;
        if (!GetRelayGeneration(&generation)) {
            activeGeneration = 0;
            svcSleepThread(SHIS_AUDIO_WAIT_NS);
            continue;
        }
        if (generation != activeGeneration) {
            CaptureSetAudioBatching(0);
            CaptureAudioConnected();
            activeGeneration = generation;
        }
        bool valid = CaptureReadAudio();
        if (!IsThreadRunning) break;
        if (!valid) continue;
        if (!SendAudioPacket()) {
            activeGeneration = 0;
            svcSleepThread(SHIS_AUDIO_WAIT_NS);
        }
    }
}

static void SHIS_Init(void)
{
    mutexInit(&RelayMutex);
    mutexInit(&VideoQueueMutex);
    RelaySocket = SOCKET_INVALID;
    RelayGeneration = 0;
    memset(VideoQueue, 0, sizeof(VideoQueue));
    VideoQueueSequence = 0;
    VideoNeedsIDR = true;

    CaptureSetNalHashing(false, false);
    CaptureSetPPSSPSInject(true);
    CaptureSetAudioBatching(0);

    memset(SenderThreadStack, 0, sizeof(SenderThreadStack));
    LaunchThread(&SenderThread, SHIS_SenderThread, NULL,
        SenderThreadStack, sizeof(SenderThreadStack), 0x2D);
}

static void SHIS_Exit(void)
{
    // IsThreadRunning is already false when Core calls ExitFn. Closing the
    // relay makes any pending non-blocking send leave promptly.
    DropRelay();
    JoinThread(&SenderThread);
    MarkVideoDiscontinuity();
}

const StreamMode TCP_MODE = {
    SHIS_Init,
    SHIS_Exit,
    SHIS_VideoThread,
    SHIS_AudioThread,
    NULL,
    NULL
};

#endif
