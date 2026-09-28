#include <switch.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CONFIG_PATH "sdmc:/config/shis-forwarder/config.ini"
#define VIDEO_PORT 9911
#define AUDIO_PORT 9922
#define VIDEO_MAX_PAYLOAD 0x54000u
#define AUDIO_MAX_PAYLOAD 0x6000u
#define SYSDVR_HEADER_SIZE 18u
#define RELAY_HEADER_SIZE 20u
#define RETRY_NS 3000000000ULL

#define META_VIDEO (1u << 0)
#define META_AUDIO (1u << 1)
#define META_DATA (1u << 2)
#define META_REPLAY (1u << 3)
#define META_ERROR (1u << 5)

#define KIND_VIDEO 1u
#define KIND_AUDIO 2u

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

#define INNER_HEAP_SIZE (4u * 1024u * 1024u)
static char g_inner_heap[INNER_HEAP_SIZE];
static uint8_t g_payload[VIDEO_MAX_PAYLOAD];

void __libnx_initheap(void) {
    extern char *fake_heap_start;
    extern char *fake_heap_end;
    fake_heap_start = g_inner_heap;
    fake_heap_end = g_inner_heap + sizeof(g_inner_heap);
}

typedef struct {
    char relay_host[256];
    uint16_t relay_port;
    char stream[64];
    char stream_key[160];
    bool audio;
} ShisConfig;

typedef struct {
    uint32_t data_size;
    uint64_t timestamp_us;
    uint8_t metadata;
} SysDvrHeader;

static char *trim(char *text) {
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    char *end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    *end = '\0';
    return text;
}

static bool load_config(ShisConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->stream, sizeof(cfg->stream), "%s", "cris");
    cfg->audio = true;

    FILE *f = fopen(CONFIG_PATH, "r");
    if (f == NULL) {
        return false;
    }

    char line[512];
    while (fgets(line, sizeof(line), f) != NULL) {
        char *entry = trim(line);
        if (*entry == '\0' || *entry == '#' || *entry == ';') {
            continue;
        }
        char *equals = strchr(entry, '=');
        if (equals == NULL) {
            continue;
        }
        *equals = '\0';
        char *key = trim(entry);
        char *value = trim(equals + 1);

        if (strcmp(key, "relay_host") == 0) {
            snprintf(cfg->relay_host, sizeof(cfg->relay_host), "%s", value);
        } else if (strcmp(key, "relay_port") == 0) {
            long port = strtol(value, NULL, 10);
            if (port > 0 && port <= 65535) {
                cfg->relay_port = (uint16_t)port;
            }
        } else if (strcmp(key, "stream") == 0) {
            snprintf(cfg->stream, sizeof(cfg->stream), "%s", value);
        } else if (strcmp(key, "stream_key") == 0) {
            snprintf(cfg->stream_key, sizeof(cfg->stream_key), "%s", value);
        } else if (strcmp(key, "audio") == 0) {
            cfg->audio = strcmp(value, "0") != 0 && strcasecmp(value, "false") != 0 && strcasecmp(value, "no") != 0;
        }
    }
    fclose(f);

    return cfg->relay_host[0] != '\0' && cfg->relay_port != 0 && cfg->stream_key[0] != '\0';
}

static int send_all(int fd, const void *data, size_t size) {
    const uint8_t *ptr = (const uint8_t *)data;
    while (size > 0) {
        ssize_t sent = send(fd, ptr, size, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        ptr += (size_t)sent;
        size -= (size_t)sent;
    }
    return 0;
}

static int recv_exact(int fd, void *data, size_t size) {
    uint8_t *ptr = (uint8_t *)data;
    while (size > 0) {
        ssize_t received = recv(fd, ptr, size, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (received == 0) {
            return -1;
        }
        ptr += (size_t)received;
        size -= (size_t)received;
    }
    return 0;
}

static uint32_t read_u32_le(const uint8_t *p) {
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p) {
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        value |= ((uint64_t)p[i]) << (i * 8);
    }
    return value;
}

static void write_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xFFu);
    p[1] = (uint8_t)((value >> 8) & 0xFFu);
    p[2] = (uint8_t)((value >> 16) & 0xFFu);
    p[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static void write_u64_le(uint8_t *p, uint64_t value) {
    for (unsigned int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)((value >> (i * 8)) & 0xFFu);
    }
}

static int connect_tcp(const char *host, uint16_t port) {
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", (unsigned int)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *result = NULL;
    if (getaddrinfo(host, port_text, &hints, &result) != 0) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *it = result; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0) {
            continue;
        }
        int one = 1;
        (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

static int connect_relay(const ShisConfig *cfg) {
    int fd = connect_tcp(cfg->relay_host, cfg->relay_port);
    if (fd < 0) {
        return -1;
    }

    char auth[384];
    int length = snprintf(auth, sizeof(auth), "SHIS/1 %s %s\n", cfg->stream, cfg->stream_key);
    if (length <= 0 || (size_t)length >= sizeof(auth) || send_all(fd, auth, (size_t)length) != 0) {
        close(fd);
        return -1;
    }

    char response[32];
    size_t used = 0;
    while (used + 1 < sizeof(response)) {
        char c = '\0';
        if (recv_exact(fd, &c, 1) != 0) {
            close(fd);
            return -1;
        }
        response[used++] = c;
        if (c == '\n') {
            break;
        }
    }
    response[used] = '\0';
    if (strcmp(response, "OK\n") != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int sysdvr_handshake(int fd, bool video) {
    uint8_t hello[10];
    if (recv_exact(fd, hello, sizeof(hello)) != 0) {
        return -1;
    }
    if (memcmp(hello, "SysDVR|", 7) != 0 || hello[9] != 0) {
        return -1;
    }
    if (hello[7] != '0' || (hello[8] != '2' && hello[8] != '3')) {
        return -1;
    }

    uint8_t request[16];
    memset(request, 0, sizeof(request));
    request[0] = 0xAA;
    request[1] = 0xAA;
    request[2] = 0xAA;
    request[3] = 0xAA;
    request[4] = hello[7];
    request[5] = hello[8];
    request[6] = video ? META_VIDEO : META_AUDIO;
    request[7] = video ? (1u << 1) : 0u; // Inject SPS/PPS, no NAL replay hashes.
    request[8] = 0; // Lowest-latency audio: no batching.

    if (send_all(fd, request, sizeof(request)) != 0) {
        return -1;
    }

    size_t response_size = hello[8] == '3' ? 72u : 4u;
    uint8_t response[72];
    if (recv_exact(fd, response, response_size) != 0) {
        return -1;
    }
    return read_u32_le(response) == 6u ? 0 : -1;
}

static int connect_sysdvr(uint16_t port, bool video) {
    int fd = connect_tcp("127.0.0.1", port);
    if (fd < 0) {
        return -1;
    }
    if (sysdvr_handshake(fd, video) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int read_sysdvr_header(int fd, SysDvrHeader *header) {
    uint8_t raw[SYSDVR_HEADER_SIZE];
    if (recv_exact(fd, raw, sizeof(raw)) != 0) {
        return -1;
    }
    if (read_u32_le(raw) != 0xCCCCCCCCu) {
        return -1;
    }
    header->data_size = read_u32_le(raw + 4);
    header->timestamp_us = read_u64_le(raw + 8);
    header->metadata = raw[16];
    return 0;
}

static bool payload_has_idr(const uint8_t *data, size_t size) {
    if (size < 5) {
        return false;
    }
    for (size_t i = 0; i + 5 <= size; ++i) {
        if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            if ((data[i + 4] & 0x1Fu) == 5u) {
                return true;
            }
        }
    }
    return false;
}

static int relay_send_frame(int relay_fd, uint8_t kind, uint8_t flags, uint64_t timestamp_us, const uint8_t *payload, uint32_t payload_size) {
    uint8_t header[RELAY_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, "SHFR", 4);
    header[4] = kind;
    header[5] = flags;
    write_u64_le(header + 8, timestamp_us);
    write_u32_le(header + 16, payload_size);

    if (send_all(relay_fd, header, sizeof(header)) != 0) {
        return -1;
    }
    return send_all(relay_fd, payload, payload_size);
}

static int read_and_forward(int source_fd, int relay_fd, bool video) {
    SysDvrHeader header;
    if (read_sysdvr_header(source_fd, &header) != 0) {
        return -1;
    }

    uint32_t max_payload = video ? VIDEO_MAX_PAYLOAD : AUDIO_MAX_PAYLOAD;
    if (header.data_size > max_payload) {
        return -1;
    }
    if (header.data_size > 0 && recv_exact(source_fd, g_payload, header.data_size) != 0) {
        return -1;
    }

    uint8_t wanted_type = video ? META_VIDEO : META_AUDIO;
    if ((header.metadata & wanted_type) == 0) {
        return 0;
    }
    if ((header.metadata & META_ERROR) != 0 || (header.metadata & META_REPLAY) != 0) {
        return 0;
    }
    if ((header.metadata & META_DATA) == 0 || header.data_size == 0) {
        return 0;
    }

    uint8_t flags = 0;
    if (video && payload_has_idr(g_payload, header.data_size)) {
        flags |= 1u;
    }
    return relay_send_frame(relay_fd, video ? KIND_VIDEO : KIND_AUDIO, flags, header.timestamp_us, g_payload, header.data_size);
}

static void run_forwarder(const ShisConfig *cfg) {
    int relay_fd = connect_relay(cfg);
    if (relay_fd < 0) {
        return;
    }

    int video_fd = connect_sysdvr(VIDEO_PORT, true);
    if (video_fd < 0) {
        close(relay_fd);
        return;
    }

    int audio_fd = -1;
    if (cfg->audio) {
        audio_fd = connect_sysdvr(AUDIO_PORT, false);
        if (audio_fd < 0) {
            close(video_fd);
            close(relay_fd);
            return;
        }
    }

    struct pollfd fds[2];
    fds[0].fd = video_fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    nfds_t count = 1;
    if (audio_fd >= 0) {
        fds[1].fd = audio_fd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;
        count = 2;
    }

    for (;;) {
        int result = poll(fds, count, 5000);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (result == 0) {
            continue;
        }

        bool failed = false;
        for (nfds_t i = 0; i < count; ++i) {
            if ((fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                failed = true;
                break;
            }
            if ((fds[i].revents & POLLIN) != 0) {
                bool video = i == 0;
                if (read_and_forward(fds[i].fd, relay_fd, video) != 0) {
                    failed = true;
                    break;
                }
            }
        }
        if (failed) {
            break;
        }
    }

    if (audio_fd >= 0) {
        close(audio_fd);
    }
    close(video_fd);
    close(relay_fd);
}

void __attribute__((weak)) __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc)) {
        fatalThrow(rc);
    }
    rc = fsInitialize();
    if (R_FAILED(rc)) {
        fatalThrow(rc);
    }
    rc = socketInitializeDefault();
    if (R_FAILED(rc)) {
        fatalThrow(rc);
    }
    (void)fsdevMountSdmc();
}

void __attribute__((weak)) __appExit(void) {
    fsdevUnmountAll();
    socketExit();
    fsExit();
    smExit();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    svcSleepThread(10000000000ULL);

    for (;;) {
        ShisConfig cfg;
        if (load_config(&cfg)) {
            run_forwarder(&cfg);
        }
        svcSleepThread(RETRY_NS);
    }

    return 0;
}
