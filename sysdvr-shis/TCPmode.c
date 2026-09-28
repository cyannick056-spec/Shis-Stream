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

#define SHIS_KIND_VIDEO 1u

static char ConfigBuffer[SHIS_CONFIG_MAX];
static u8 DnsBuffer[SHIS_DNS_MAX];

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
    if (R_FAILED(rc)) {
        fsFsClose(&fs);
        return false;
    }

    s64 fileSize = 0;
    rc = fsFileGetSize(&file, &fileSize);
    if (R_FAILED(rc) || fileSize <= 0 || fileSize >= (s64)sizeof(ConfigBuffer)) {
        fsFileClose(&file);
        fsFsClose(&fs);
        return false;
    }

    u64 bytesRead = 0;
    rc = fsFileRead(&file, 0, ConfigBuffer, (u64)fileSize, 0, &bytesRead);
    fsFileClose(&file);
    fsFsClose(&fs);
    if (R_FAILED(rc) || bytesRead != (u64)fileSize) return false;
    ConfigBuffer[fileSize] = 0;

    char* cursor = ConfigBuffer;
    while (*cursor) {
        char* line = cursor;
        char* nl = strchr(cursor, '\n');
        if (nl) {
            *nl = 0;
            cursor = nl + 1;
        } else {
            cursor += strlen(cursor);
        }

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

static u16 ReadBE16(const u8* p)
{
    return ((u16)p[0] << 8) | p[1];
}

static u32 ReadBE32(const u8* p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

// sfdnsres serializes hostent data in Nintendo's network resolver format.
// This parser intentionally mirrors libnx resolver.c but uses static memory.
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
    u32 aliases = ReadBE32(p);
    p += 4;
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

    // Match libnx gethostbyname()'s conversion for Nintendo's serialized address.
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
        SocketClose(&sock);
        return SOCKET_INVALID;
    }

    char auth[SHIS_AUTH_MAX];
    const char prefix[] = "SHIS/1 ";
    size_t used = 0;
    size_t n = sizeof(prefix) - 1;
    if (n >= sizeof(auth)) { SocketClose(&sock); return SOCKET_INVALID; }
    memcpy(auth + used, prefix, n); used += n;
    n = strlen(cfg->stream);
    if (used + n + 1 >= sizeof(auth)) { SocketClose(&sock); return SOCKET_INVALID; }
    memcpy(auth + used, cfg->stream, n); used += n;
    auth[used++] = ' ';
    n = strlen(cfg->key);
    if (used + n + 1 >= sizeof(auth)) { SocketClose(&sock); return SOCKET_INVALID; }
    memcpy(auth + used, cfg->key, n); used += n;
    auth[used++] = '\n';

    if (!SocketSendAll(sock, auth, (u32)used)) {
        SocketClose(&sock);
        return SOCKET_INVALID;
    }

    char response[3] = {0};
    if (!SocketRecevExact(sock, response, sizeof(response)) || memcmp(response, "OK\n", 3) != 0) {
        SocketClose(&sock);
        return SOCKET_INVALID;
    }

    return sock;
}

static void WriteLE32(u8* p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static void WriteLE64(u8* p, u64 v)
{
    for (int i = 0; i < 8; ++i) p[i] = (u8)(v >> (i * 8));
}

static bool HasIDR(const u8* data, u32 size)
{
    for (u32 i = 0; i + 5 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0) {
            if (i + 4 < size && data[i + 2] == 0 && data[i + 3] == 1)
                if ((data[i + 4] & 0x1F) == 5) return true;
            if (i + 3 < size && data[i + 2] == 1)
                if ((data[i + 3] & 0x1F) == 5) return true;
        }
    }
    return false;
}

static bool SendVideoPacket(int sock)
{
    if ((VPkt.Header.MetaData & PacketMeta_Content_Mask) != PacketMeta_Content_Data || VPkt.Header.DataSize == 0)
        return true;

    u8 header[SHIS_RELAY_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, "SHFR", 4);
    header[4] = SHIS_KIND_VIDEO;
    header[5] = HasIDR(VPkt.Data, VPkt.Header.DataSize) ? 1 : 0;
    WriteLE64(header + 8, VPkt.Header.Timestamp);
    WriteLE32(header + 16, VPkt.Header.DataSize);

    return SocketSendAll(sock, header, sizeof(header)) &&
        SocketSendAll(sock, VPkt.Data, VPkt.Header.DataSize);
}

static void SHIS_VideoThread(void* unused)
{
    (void)unused;

    while (IsThreadRunning) {
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

        CaptureSetNalHashing(false, false);
        CaptureSetPPSSPSInject(true);
        CaptureVideoConnected();

        while (IsThreadRunning) {
            bool valid = CaptureReadVideo();
            if (!IsThreadRunning) break;
            if (!valid) continue;
            if (!SendVideoPacket(relay)) break;
        }

        SocketClose(&relay);
        if (IsThreadRunning) svcSleepThread(SHIS_RETRY_NS);
    }
}

static void SHIS_Init(void)
{
    CaptureSetNalHashing(false, false);
    CaptureSetPPSSPSInject(true);
}

const StreamMode TCP_MODE = {
    SHIS_Init,
    NULL,
    SHIS_VideoThread,
    NULL,
    NULL,
    NULL
};

#endif
