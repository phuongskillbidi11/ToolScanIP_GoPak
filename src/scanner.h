#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>
#include <pthread.h>

#define MAX_HOSTS    1024
#define IP_STR_LEN   16
#define MAC_STR_LEN  18
#define HOSTNAME_LEN 128
#define VENDOR_LEN   64
#define COMMENT_LEN  128

typedef struct {
    char    ip[IP_STR_LEN];
    uint8_t mac[6];
    char    mac_str[MAC_STR_LEN];
    char    hostname[HOSTNAME_LEN];
    char    vendor[VENDOR_LEN];
    char    comment[COMMENT_LEN];   /* user-editable, always empty at scan time */
    int     online;
} Host;

typedef struct {
    Host            hosts[MAX_HOSTS];
    int             count;
    pthread_mutex_t lock;
} ScanResult;

/*
 * Scan all IPv4 hosts reachable on `iface` (single /8–/30 subnet).
 * `result` is filled with discovered hosts ordered by IP.
 * Returns 0 on success, -1 on fatal error.
 */
int scan_network(const char *iface, ScanResult *result);

#endif /* SCANNER_H */
