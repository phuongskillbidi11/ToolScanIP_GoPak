#ifndef PROBE_H
#define PROBE_H

#include "scanner.h"

/*
 * Attempt to identify the chip/vendor of a discovered host using
 * non-MAC methods:
 *   1. Hostname pattern matching (luckfox → Rockchip, etc.)
 *   2. SSH banner grab (port 22) — reads first line for OS/board clues
 *   3. HTTP probe (port 80) — reads Server header / page title
 *
 * If a chip vendor is identified, h->vendor is overwritten.
 * Called only when h->vendor is "Unknown" or "Randomized MAC (Privacy)".
 */
void probe_vendor(Host *h);

#endif /* PROBE_H */
