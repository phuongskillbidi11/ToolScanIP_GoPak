#ifndef OUI_H
#define OUI_H

#include <stdint.h>
#include <stddef.h>

/*
 * Load the OUI database from the system.
 * Tries (in order):
 *   /usr/share/wireshark/manuf
 *   /usr/share/nmap/nmap-mac-prefixes
 *   /var/lib/ieee-data/oui.txt
 * Falls back to the built-in table if none are found.
 * Call once before any oui_lookup().
 */
void oui_init(void);

/*
 * Look up the manufacturer name for a MAC address.
 * Detects randomized/locally-administered MACs (bit 0x02 of byte 0).
 * Searches the loaded system database first, then the built-in table.
 * Writes the result into `buf` (at most `len` bytes).
 */
void oui_lookup(const uint8_t *mac, char *buf, size_t len);

/* Free memory used by the dynamic database. */
void oui_free(void);

#endif /* OUI_H */
