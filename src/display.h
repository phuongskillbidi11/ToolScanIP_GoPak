#ifndef DISPLAY_H
#define DISPLAY_H

#include "scanner.h"

/*
 * Print the scan results as a formatted table to stdout.
 * Columns: Status | Hostname | IP Address | Manufacturer | MAC Address | Comments
 */
void display_results(const ScanResult *result);

/*
 * Print a progress spinner / live counter while scanning.
 * Call from the main thread between sending and results.
 */
void display_header(void);

#endif /* DISPLAY_H */
