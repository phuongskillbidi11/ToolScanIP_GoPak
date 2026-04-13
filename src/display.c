#include "display.h"
#include "scanner.h"

#include <stdio.h>
#include <string.h>

/* ANSI color codes */
#define ANSI_RESET    "\033[0m"
#define ANSI_BOLD     "\033[1m"
#define ANSI_GREEN    "\033[32m"
#define ANSI_CYAN     "\033[36m"
#define ANSI_YELLOW   "\033[33m"
#define ANSI_WHITE    "\033[97m"
#define ANSI_DIM      "\033[2m"

/* Box-drawing characters (UTF-8) */
#define H  "\xe2\x94\x80"   /* ─ */
#define V  "\xe2\x94\x82"   /* │ */
#define TL "\xe2\x94\x8c"   /* ┌ */
#define TR "\xe2\x94\x90"   /* ┐ */
#define BL "\xe2\x94\x94"   /* └ */
#define BR "\xe2\x94\x98"   /* ┘ */
#define TM "\xe2\x94\xac"   /* ┬ */
#define BM "\xe2\x94\xb4"   /* ┴ */
#define LM "\xe2\x94\x9c"   /* ├ */
#define RM "\xe2\x94\xa4"   /* ┤ */
#define CR "\xe2\x94\xbc"   /* ┼ */

/* Online icon: filled green circle (●) */
#define ICON_ONLINE  "\033[32m\xe2\x97\x8f\033[0m"  /* ● */

/* Column widths (characters, excluding borders) */
#define W_NUM      3
#define W_STATUS   6
#define W_HOSTNAME 24
#define W_IP       15
#define W_VENDOR   22
#define W_MAC      17
#define W_COMMENT  14

static void repeat(const char *s, int n)
{
    for (int i = 0; i < n; i++) fputs(s, stdout);
}

static void print_sep(const char *left, const char *mid, const char *right)
{
    fputs(left, stdout);
    repeat(H, W_NUM + 2);      fputs(mid, stdout);
    repeat(H, W_STATUS + 2);   fputs(mid, stdout);
    repeat(H, W_HOSTNAME + 2); fputs(mid, stdout);
    repeat(H, W_IP + 2);       fputs(mid, stdout);
    repeat(H, W_VENDOR + 2);   fputs(mid, stdout);
    repeat(H, W_MAC + 2);      fputs(mid, stdout);
    repeat(H, W_COMMENT + 2);
    fputs(right, stdout);
    putchar('\n');
}

static void print_row(const char *num,
                      const char *status,
                      const char *hostname,
                      const char *ip,
                      const char *vendor,
                      const char *mac,
                      const char *comment,
                      int is_header)
{
    if (is_header) {
        printf(" " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V " " ANSI_BOLD ANSI_CYAN "%-*s" ANSI_RESET
                 " " V "\n",
               W_NUM,      num,
               W_STATUS,   status,
               W_HOSTNAME, hostname,
               W_IP,       ip,
               W_VENDOR,   vendor,
               W_MAC,      mac,
               W_COMMENT,  comment);
    } else {
        /* status contains ANSI codes — print raw, then pad manually */
        int status_vis = 1;  /* "●" is 1 visible character */
        int pad = W_STATUS - status_vis;

        printf(" " V " " ANSI_DIM "%*s" ANSI_RESET
                 " " V " %s%*s"
                 " " V " %-*s"
                 " " V " " ANSI_YELLOW "%-*s" ANSI_RESET
                 " " V " %-*s"
                 " " V " " ANSI_DIM "%-*s" ANSI_RESET
                 " " V " %-*s"
                 " " V "\n",
               W_NUM, num,
               status, pad, "",
               W_HOSTNAME, hostname,
               W_IP,       ip,
               W_VENDOR,   vendor,
               W_MAC,      mac,
               W_COMMENT,  comment);
    }
}

void display_header(void)
{
    printf(ANSI_BOLD "\n  IP Scanner — ARP Network Discovery\n" ANSI_RESET);
    printf(ANSI_DIM  "  Requires root / CAP_NET_RAW\n\n" ANSI_RESET);
}

void display_results(const ScanResult *result)
{
    if (result->count == 0) {
        printf("\n  No hosts found.\n");
        return;
    }

    printf("\n  Found " ANSI_BOLD ANSI_GREEN "%d" ANSI_RESET " host(s):\n\n",
           result->count);

    /* Top border */
    fputs(" ", stdout);
    print_sep(TL, TM, TR);

    /* Header */
    print_row("#", "Status", "Hostname", "IP Address",
              "Manufacturer", "MAC Address", "Comments", 1);

    /* Header separator */
    fputs(" ", stdout);
    print_sep(LM, CR, RM);

    /* Data rows */
    for (int i = 0; i < result->count; i++) {
        const Host *h = &result->hosts[i];

        /* Truncate long strings to fit columns */
        char hostname[W_HOSTNAME + 1];
        char vendor[W_VENDOR + 1];
        char numstr[W_NUM + 1];
        snprintf(hostname, sizeof(hostname), "%s", h->hostname);
        snprintf(vendor,   sizeof(vendor),   "%s", h->vendor);
        snprintf(numstr,   sizeof(numstr),   "%d", i + 1);

        print_row(numstr, h->online ? ICON_ONLINE : " ",
                  hostname, h->ip, vendor, h->mac_str, h->comment, 0);
    }

    /* Bottom border */
    fputs(" ", stdout);
    print_sep(BL, BM, BR);

    printf("\n  " ANSI_DIM "Legend: " ICON_ONLINE " = Online\n" ANSI_RESET "\n");
}
