#include "probe.h"
#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PROBE_TIMEOUT_MS  300
#define PROBE_BUFSIZE     512

/* ── Chip/board keyword table ────────────────────────────────────────────── */
/*
 * Each entry maps a keyword (found in hostname, SSH banner, or HTTP body)
 * to a vendor string. Keywords are matched case-insensitively.
 * Order matters: more specific entries first.
 */
typedef struct {
    const char *keyword;
    const char *vendor;
} ChipKeyword;

static const ChipKeyword CHIP_KEYWORDS[] = {
    /* Rockchip boards */
    { "luckfox",       "Luckfox (Rockchip RK35xx)"         },
    { "luckfox-lyra",  "Luckfox Lyra Plus (RK3506G2)"      },
    { "rockchip",      "Fuzhou Rockchip Electronics"        },
    { "rk3506",        "Rockchip RK3506"                    },
    { "rk3568",        "Rockchip RK3568"                    },
    { "rk3588",        "Rockchip RK3588"                    },
    { "rk3399",        "Rockchip RK3399"                    },
    { "rk3328",        "Rockchip RK3328"                    },
    { "rk3288",        "Rockchip RK3288"                    },
    /* Orange Pi */
    { "orangepi",      "Orange Pi (Rockchip/Allwinner)"     },
    /* Raspberry Pi */
    { "raspberrypi",   "Raspberry Pi (Broadcom BCM)"        },
    { "raspberry",     "Raspberry Pi (Broadcom BCM)"        },
    /* Allwinner */
    { "allwinner",     "Allwinner Technology"               },
    { "sunxi",         "Allwinner (sunxi)"                  },
    { "h616",          "Allwinner H616"                     },
    { "h6",            "Allwinner H6"                       },
    /* Amlogic */
    { "amlogic",       "Amlogic"                            },
    { "meson",         "Amlogic (meson)"                    },
    { "s905",          "Amlogic S905"                       },
    /* NXP / iMX */
    { "imx",           "NXP i.MX"                          },
    { "nxp",           "NXP Semiconductors"                 },
    /* Texas Instruments */
    { "am335x",        "Texas Instruments AM335x"           },
    { "am64",          "Texas Instruments AM64x"            },
    { "beaglebone",    "BeagleBone (TI AM335x)"             },
    /* STM32 */
    { "stm32",         "STMicroelectronics STM32"           },
    /* Renesas */
    { "renesas",       "Renesas Electronics"                },
    /* NVIDIA Jetson */
    { "jetson",        "NVIDIA Jetson"                      },
    { "tegra",         "NVIDIA Tegra"                       },
    /* MediaTek */
    { "mediatek",      "MediaTek"                           },
    { "mt7621",        "MediaTek MT7621"                    },
    { "mt7986",        "MediaTek MT7986"                    },
    /* Qualcomm */
    { "qcom",          "Qualcomm"                           },
    { "snapdragon",    "Qualcomm Snapdragon"                },
    /* Intel / x86 embedded */
    { "intel",         "Intel"                              },
    /* Generic Linux clues */
    { "openwrt",       "OpenWrt Router"                     },
    { "openwrt",       "OpenWrt Router"                     },
    { "dd-wrt",        "DD-WRT Router"                      },
    { "mikrotik",      "MikroTik RouterOS"                  },
    { "routeros",      "MikroTik RouterOS"                  },
    { "ubiquiti",      "Ubiquiti Networks"                  },
    { "unifi",         "Ubiquiti UniFi"                     },
    { "draytek",       "DrayTek"                            },
    { "synology",      "Synology NAS"                       },
    { "qnap",          "QNAP NAS"                           },
    { "freenas",       "TrueNAS / FreeNAS"                  },
    { "truenas",       "TrueNAS"                            },
    { "android",       "Android Device"                     },
    /* Industrial */
    { "keyence",       "Keyence"                            },
    { "mitsubishi",    "Mitsubishi Electric"                },
    { "siemens",       "Siemens"                            },
    { "omron",         "Omron"                              },
};

static const size_t CHIP_KEYWORD_COUNT =
    sizeof(CHIP_KEYWORDS) / sizeof(CHIP_KEYWORDS[0]);

/* ── Helper: case-insensitive substring match ────────────────────────────── */

static const char *match_keywords(const char *haystack)
{
    if (!haystack || !*haystack) return NULL;

    /* Lowercase copy for matching */
    char lower[PROBE_BUFSIZE];
    size_t i;
    for (i = 0; i < sizeof(lower) - 1 && haystack[i]; i++)
        lower[i] = (char)(haystack[i] | 0x20); /* ASCII tolower */
    lower[i] = '\0';

    for (size_t k = 0; k < CHIP_KEYWORD_COUNT; k++) {
        if (strstr(lower, CHIP_KEYWORDS[k].keyword))
            return CHIP_KEYWORDS[k].vendor;
    }
    return NULL;
}

/* ── TCP connect with timeout ────────────────────────────────────────────── */

static int tcp_connect(const char *ip, int port, int timeout_ms)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    /* Non-blocking connect */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    inet_aton(ip, &addr.sin_addr);

    int ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };

    if (select(sock + 1, NULL, &wfds, NULL, &tv) <= 0) {
        close(sock);
        return -1;
    }

    /* Check if connect succeeded */
    int err = 0;
    socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err) { close(sock); return -1; }

    /* Restore blocking mode */
    fcntl(sock, F_SETFL, flags);
    return sock;
}

/* ── TCP read with timeout ───────────────────────────────────────────────── */

static ssize_t tcp_read(int sock, char *buf, size_t len, int timeout_ms)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    if (select(sock + 1, &rfds, NULL, NULL, &tv) <= 0) return -1;
    return recv(sock, buf, len - 1, 0);
}

/* ── Probe methods ───────────────────────────────────────────────────────── */

/* 1. SSH banner grab (port 22) — server sends banner immediately on connect */
static const char *probe_ssh(const char *ip)
{
    int sock = tcp_connect(ip, 22, PROBE_TIMEOUT_MS);
    if (sock < 0) return NULL;

    char buf[PROBE_BUFSIZE] = {0};
    ssize_t n = tcp_read(sock, buf, sizeof(buf), PROBE_TIMEOUT_MS);
    close(sock);
    if (n <= 0) return NULL;

    buf[n] = '\0';
    return match_keywords(buf);
}

/* 2. HTTP probe (port 80) — send minimal GET, look at Server + body */
static const char *probe_http(const char *ip)
{
    int sock = tcp_connect(ip, 80, PROBE_TIMEOUT_MS);
    if (sock < 0) return NULL;

    const char *req = "GET / HTTP/1.0\r\nHost: device\r\n\r\n";
    send(sock, req, strlen(req), 0);

    char buf[PROBE_BUFSIZE] = {0};
    ssize_t n = tcp_read(sock, buf, sizeof(buf), PROBE_TIMEOUT_MS);
    close(sock);
    if (n <= 0) return NULL;

    buf[n] = '\0';
    return match_keywords(buf);
}

/* 3. Telnet probe (port 23) — some embedded devices send a login banner */
static const char *probe_telnet(const char *ip)
{
    int sock = tcp_connect(ip, 23, PROBE_TIMEOUT_MS);
    if (sock < 0) return NULL;

    char buf[PROBE_BUFSIZE] = {0};
    ssize_t n = tcp_read(sock, buf, sizeof(buf), PROBE_TIMEOUT_MS);
    close(sock);
    if (n <= 0) return NULL;

    buf[n] = '\0';
    return match_keywords(buf);
}

/* ── mDNS helpers ────────────────────────────────────────────────────────── */

/* Encode "26.20.168.192.in-addr.arpa" into DNS wire-format labels */
static int dns_encode_name(const char *name, uint8_t *buf, int maxlen)
{
    int pos = 0;
    while (*name) {
        const char *dot = strchr(name, '.');
        int lablen = dot ? (int)(dot - name) : (int)strlen(name);
        if (pos + 1 + lablen >= maxlen) return -1;
        buf[pos++] = (uint8_t)lablen;
        memcpy(buf + pos, name, lablen);
        pos += lablen;
        if (!dot) break;
        name = dot + 1;
    }
    if (pos >= maxlen) return -1;
    buf[pos++] = 0;
    return pos;
}

/*
 * Decode a DNS name from `pkt` at `offset`, handling pointer compression.
 * Returns the offset *after* the name in the original message (not followed
 * pointers), or -1 on error.
 */
static int dns_decode_name(const uint8_t *pkt, int pktlen, int offset,
                            char *out, int outlen)
{
    int pos        = offset;
    int out_pos    = 0;
    int jumped     = 0;
    int end_offset = -1;

    while (pos < pktlen) {
        uint8_t llen = pkt[pos];

        if (llen == 0) {
            if (!jumped) end_offset = pos + 1;
            break;
        }

        if ((llen & 0xC0) == 0xC0) {
            /* Pointer compression */
            if (pos + 1 >= pktlen) return -1;
            if (!jumped) end_offset = pos + 2;
            jumped = 1;
            pos = ((llen & 0x3F) << 8) | pkt[pos + 1];
            continue;
        }

        pos++;
        if (out_pos > 0 && out_pos < outlen - 1)
            out[out_pos++] = '.';

        int copy = llen;
        if (out_pos + copy >= outlen) copy = outlen - out_pos - 1;
        memcpy(out + out_pos, pkt + pos, copy);
        out_pos += copy;
        pos += llen;
    }

    out[out_pos] = '\0';
    return (end_offset >= 0) ? end_offset : pos + 1;
}

/*
 * 4. mDNS probe (UDP port 5353).
 * Sends a PTR query for the reverse IP address directly to the host
 * (unicast mDNS). If the device runs avahi/mdns it replies with its
 * hostname (e.g. "luckfox.local") which we match against the keyword table.
 *
 * Also sends the same query to the mDNS multicast group 224.0.0.251
 * so that devices which only respond to multicast are also discovered.
 */
static const char *probe_mdns(const char *ip)
{
    /* Build reverse-lookup name: "a.b.c.d" -> "d.c.b.a.in-addr.arpa" */
    unsigned int a, b, c, d;
    if (sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return NULL;

    char rev[64];
    snprintf(rev, sizeof(rev), "%u.%u.%u.%u.in-addr.arpa", d, c, b, a);

    /* Build DNS query packet */
    uint8_t pkt[256];
    memset(pkt, 0, sizeof(pkt));
    /* Header */
    pkt[0] = 0x00; pkt[1] = 0x00; /* ID */
    pkt[2] = 0x00; pkt[3] = 0x00; /* Flags: standard query */
    pkt[4] = 0x00; pkt[5] = 0x01; /* QDCOUNT = 1 */
    /* Question */
    int name_len = dns_encode_name(rev, pkt + 12, (int)sizeof(pkt) - 12);
    if (name_len < 0) return NULL;
    int q_off = 12 + name_len;
    pkt[q_off]     = 0x00; pkt[q_off + 1] = 0x0C; /* QTYPE = PTR */
    pkt[q_off + 2] = 0x00; pkt[q_off + 3] = 0x01; /* QCLASS = IN */
    int pkt_len = q_off + 4;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return NULL;

    struct timeval tv = { 0, PROBE_TIMEOUT_MS * 1000 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Send unicast to target:5353 */
    struct sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons(5353);
    inet_aton(ip, &dst.sin_addr);
    sendto(sock, pkt, pkt_len, 0, (struct sockaddr *)&dst, sizeof(dst));

    /* Also send to mDNS multicast group */
    struct sockaddr_in mcast;
    memset(&mcast, 0, sizeof(mcast));
    mcast.sin_family = AF_INET;
    mcast.sin_port   = htons(5353);
    inet_aton("224.0.0.251", &mcast.sin_addr);
    sendto(sock, pkt, pkt_len, 0, (struct sockaddr *)&mcast, sizeof(mcast));

    /* Receive response */
    uint8_t resp[512];
    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    ssize_t n = recvfrom(sock, resp, sizeof(resp) - 1, 0,
                         (struct sockaddr *)&src, &srclen);
    close(sock);
    if (n < 12) return NULL;

    /* Only process replies from the target IP */
    char src_ip[16];
    inet_ntop(AF_INET, &src.sin_addr, src_ip, sizeof(src_ip));
    if (strcmp(src_ip, ip) != 0) return NULL;

    /* Check it's a response (QR bit set) */
    if (!(resp[2] & 0x80)) return NULL;

    uint16_t ancount = (uint16_t)((resp[6] << 8) | resp[7]);
    if (ancount == 0) return NULL;

    /* Skip the question section to reach the answer section */
    int pos = 12;
    uint16_t qdcount = (uint16_t)((resp[4] << 8) | resp[5]);
    for (int q = 0; q < qdcount && pos < (int)n; q++) {
        char tmp[256];
        pos = dns_decode_name(resp, (int)n, pos, tmp, sizeof(tmp));
        if (pos < 0) return NULL;
        pos += 4; /* skip QTYPE + QCLASS */
    }

    /* Parse answer records, find PTR (type 12) */
    for (int ans = 0; ans < ancount && pos + 10 < (int)n; ans++) {
        char aname[256];
        pos = dns_decode_name(resp, (int)n, pos, aname, sizeof(aname));
        if (pos < 0) return NULL;

        uint16_t rtype  = (uint16_t)((resp[pos] << 8)     | resp[pos + 1]);
        /* skip class(2) + ttl(4) */
        uint16_t rdlen  = (uint16_t)((resp[pos + 8] << 8) | resp[pos + 9]);
        pos += 10;

        if (rtype == 12 /* PTR */ && pos + rdlen <= (int)n) {
            char hostname[256];
            dns_decode_name(resp, (int)n, pos, hostname, sizeof(hostname));

            /* Strip ".local" suffix for cleaner matching */
            char *dot = strstr(hostname, ".local");
            if (dot) *dot = '\0';

            const char *vendor = match_keywords(hostname);
            if (vendor) return vendor;
        }
        pos += rdlen;
    }
    return NULL;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

void probe_vendor(Host *h)
{
    const char *found = NULL;

    /* Step 1: hostname pattern (free — already resolved via regular DNS) */
    found = match_keywords(h->hostname);
    if (found) goto done;

    /* Step 2: mDNS — query device directly on port 5353.
     * Embedded Linux boards (avahi), IoT devices, macOS, iOS all respond.
     * Returns hostname like "luckfox" -> "Luckfox (Rockchip RK35xx)" */
    found = probe_mdns(h->ip);
    if (found) goto done;

    /* Step 3: SSH banner grab */
    found = probe_ssh(h->ip);
    if (found) goto done;

    /* Step 4: HTTP */
    found = probe_http(h->ip);
    if (found) goto done;

    /* Step 5: Telnet (common on older embedded/industrial devices) */
    found = probe_telnet(h->ip);
    if (found) goto done;

    return; /* nothing found — leave vendor as-is */

done:
    snprintf(h->vendor, VENDOR_LEN, "%s", found);
}
