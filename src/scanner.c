#include "scanner.h"
#include "arp.h"
#include "oui.h"
#include "probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

/* ── Receiver thread context ─────────────────────────────────────────────── */

typedef struct {
    int         sock;
    ScanResult *result;
    int         done;       /* set to 1 by main thread when sending finishes */
} RecvCtx;

static void record_host(ScanResult *result, const uint8_t *ip, const uint8_t *mac)
{
    char ip_str[IP_STR_LEN];
    snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",
             ip[0], ip[1], ip[2], ip[3]);

    pthread_mutex_lock(&result->lock);

    /* Avoid duplicate entries */
    for (int i = 0; i < result->count; i++) {
        if (strcmp(result->hosts[i].ip, ip_str) == 0) {
            pthread_mutex_unlock(&result->lock);
            return;
        }
    }

    if (result->count >= MAX_HOSTS) {
        pthread_mutex_unlock(&result->lock);
        return;
    }

    Host *h = &result->hosts[result->count++];
    memset(h, 0, sizeof(*h));
    snprintf(h->ip, IP_STR_LEN, "%s", ip_str);
    memcpy(h->mac, mac, 6);
    snprintf(h->mac_str, MAC_STR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    h->online = 1;

    pthread_mutex_unlock(&result->lock);
}

static void *receiver_thread(void *arg)
{
    RecvCtx *ctx = (RecvCtx *)arg;
    uint8_t  ip[4], mac[6];

    while (1) {
        int ret = arp_recv_reply(ctx->sock, ip, mac, 200 /* ms */);
        if (ret == 1) {
            record_host(ctx->result, ip, mac);
        } else if (ret < 0) {
            break;
        }
        /* If sending is done and we got a timeout, drain for one more second */
        if (ctx->done && ret == 0) {
            break;
        }
    }
    return NULL;
}

/* ── Interface helpers ───────────────────────────────────────────────────── */

static int get_iface_info(const char *iface,
                          uint8_t *mac_out,
                          uint32_t *ip_out,
                          uint32_t *mask_out)
{
    struct ifaddrs *ifap, *ifa;
    int found_ip = 0;

    /* MAC address via ioctl */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        perror("ioctl(SIOCGIFHWADDR)");
        close(sock);
        return -1;
    }
    memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);
    close(sock);

    /* IP + netmask via getifaddrs */
    if (getifaddrs(&ifap) < 0) return -1;

    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, iface) != 0) continue;

        struct sockaddr_in *sa   = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
        *ip_out   = ntohl(sa->sin_addr.s_addr);
        *mask_out = ntohl(mask->sin_addr.s_addr);
        found_ip  = 1;
        break;
    }
    freeifaddrs(ifap);
    return found_ip ? 0 : -1;
}

/* ── Hostname + vendor resolution ───────────────────────────────────────── */

static int host_cmp(const void *a, const void *b)
{
    const Host *ha = (const Host *)a;
    const Host *hb = (const Host *)b;
    struct in_addr ia, ib;
    inet_aton(ha->ip, &ia);
    inet_aton(hb->ip, &ib);
    uint32_t ua = ntohl(ia.s_addr);
    uint32_t ub = ntohl(ib.s_addr);
    return (ua > ub) - (ua < ub);
}

static void *resolve_one(void *arg)
{
    Host *h = (Host *)arg;

    /* Hostname (reverse DNS) — getnameinfo is thread-safe */
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_aton(h->ip, &sa.sin_addr);

    char name[HOSTNAME_LEN];
    if (getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                    name, sizeof(name), NULL, 0, NI_NAMEREQD) == 0)
        strncpy(h->hostname, name, HOSTNAME_LEN - 1);
    else
        strncpy(h->hostname, h->ip, HOSTNAME_LEN - 1);

    /* Vendor from OUI */
    oui_lookup(h->mac, h->vendor, VENDOR_LEN);

    /* If OUI didn't identify it, probe the device directly */
    if (strcmp(h->vendor, "Unknown") == 0 ||
        strncmp(h->vendor, "Randomized", 10) == 0)
    {
        probe_vendor(h);
    }

    return NULL;
}

static void resolve_hosts(ScanResult *result)
{
    int n = result->count;
    pthread_t *threads = malloc((size_t)n * sizeof(pthread_t));
    if (!threads) {
        /* Fallback: sequential */
        for (int i = 0; i < n; i++) resolve_one(&result->hosts[i]);
        return;
    }

    for (int i = 0; i < n; i++)
        pthread_create(&threads[i], NULL, resolve_one, &result->hosts[i]);
    for (int i = 0; i < n; i++)
        pthread_join(threads[i], NULL);

    free(threads);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

int scan_network(const char *iface, ScanResult *result)
{
    uint8_t  src_mac[6];
    uint32_t src_ip_h, mask_h;

    pthread_mutex_init(&result->lock, NULL);
    result->count = 0;

    if (get_iface_info(iface, src_mac, &src_ip_h, &mask_h) < 0) {
        fprintf(stderr, "Cannot get interface info for '%s'\n", iface);
        return -1;
    }

    /* Only scan networks up to /16 (65534 hosts) */
    uint32_t host_bits = ~mask_h;
    if (host_bits > 0xFFFF) {
        fprintf(stderr,
                "Network is larger than /16 (%u hosts). "
                "Use a more specific interface or range.\n",
                host_bits - 1);
        return -1;
    }

    uint32_t net_h   = src_ip_h & mask_h;
    uint32_t host_n  = host_bits - 1;   /* exclude broadcast */

    printf("Scanning %s: %u.%u.%u.%u/%u  (%u hosts)\n",
           iface,
           (net_h >> 24) & 0xFF, (net_h >> 16) & 0xFF,
           (net_h >> 8)  & 0xFF, net_h & 0xFF,
           __builtin_popcount(mask_h),
           host_n);
    fflush(stdout);

    /* Open raw socket */
    int sock = arp_open_socket(iface);
    if (sock < 0) return -1;

    /* Start receiver thread */
    RecvCtx ctx = { .sock = sock, .result = result, .done = 0 };
    pthread_t thr;
    pthread_create(&thr, NULL, receiver_thread, &ctx);

    /* Send ARP requests */
    uint8_t src_ip[4];
    src_ip[0] = (src_ip_h >> 24) & 0xFF;
    src_ip[1] = (src_ip_h >> 16) & 0xFF;
    src_ip[2] = (src_ip_h >> 8)  & 0xFF;
    src_ip[3] = src_ip_h & 0xFF;

    for (uint32_t offset = 1; offset <= host_n; offset++) {
        uint32_t target_h = net_h + offset;
        uint8_t  tgt[4];
        tgt[0] = (target_h >> 24) & 0xFF;
        tgt[1] = (target_h >> 16) & 0xFF;
        tgt[2] = (target_h >> 8)  & 0xFF;
        tgt[3] = target_h & 0xFF;

        arp_send_request(sock, iface, src_mac, src_ip, tgt);

        /* Small delay to avoid overwhelming slower networks/switches */
        if ((offset % 50) == 0)
            usleep(2000); /* 2 ms every 50 packets */
    }

    /* Signal receiver to drain remaining replies */
    ctx.done = 1;
    pthread_join(thr, NULL);
    close(sock);

    /* Sort by IP and enrich */
    qsort(result->hosts, result->count, sizeof(Host), host_cmp);
    resolve_hosts(result);

    return 0;
}
