#ifndef COMMENTS_H
#define COMMENTS_H

#include "scanner.h"

/*
 * Comments are stored in a plain text file (default: ~/.ipscanner.comments).
 * Each line is one of:
 *   MAC=comment       e.g.  FE:67:DE:21:6A:3F=Luckfox Lyra Plus IoT gateway
 *   IP=comment        e.g.  192.168.20.26=Luckfox Lyra Plus IoT gateway
 *
 * MAC-based entries survive IP changes (DHCP).
 * IP-based entries are useful for devices with randomized MACs but static IPs.
 * MAC takes priority over IP when both match.
 */

/* Load comments file into memory. Call once before comments_apply(). */
void comments_load(const char *path);

/* Apply loaded comments to all hosts in result. */
void comments_apply(ScanResult *result);

/* Save or update a comment. key = MAC string or IP string. */
int comments_save(const char *path, const char *key, const char *comment);

/* Free memory used by comments. */
void comments_free(void);

#endif /* COMMENTS_H */
