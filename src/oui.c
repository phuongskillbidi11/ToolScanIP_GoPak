#include "oui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define VENDOR_LEN 64

/* ── Built-in fallback OUI table ─────────────────────────────────────────── */

typedef struct {
    uint8_t     oui[3];
    const char *vendor;
} OuiEntry;

static const OuiEntry BUILTIN_TABLE[] = {
    /* Apple */
    {{0x00,0x1C,0xB3}, "Apple"},
    {{0x00,0x25,0xBC}, "Apple"},
    {{0x3C,0x07,0x54}, "Apple"},
    {{0x40,0xA6,0xD9}, "Apple"},
    {{0x58,0x55,0xCA}, "Apple"},
    {{0xAC,0xBC,0x32}, "Apple"},
    {{0xD8,0xBB,0x2C}, "Apple"},
    {{0xF0,0x18,0x98}, "Apple"},
    /* Samsung */
    {{0x00,0x15,0x99}, "Samsung Electronics"},
    {{0x2C,0xAE,0x2B}, "Samsung Electronics"},
    {{0x5C,0x49,0x79}, "Samsung Electronics"},
    {{0x8C,0x77,0x12}, "Samsung Electronics"},
    {{0xBC,0x20,0xA4}, "Samsung Electronics"},
    /* Intel */
    {{0x00,0x1B,0x21}, "Intel Corporate"},
    {{0x18,0x3D,0xA2}, "Intel Corporate"},
    {{0x28,0xD2,0x44}, "Intel Corporate"},
    {{0x40,0x25,0xC2}, "Intel Corporate"},
    {{0x54,0x27,0x1E}, "Intel Corporate"},
    {{0x7C,0x5C,0xF8}, "Intel Corporate"},
    {{0xA4,0x34,0xD9}, "Intel Corporate"},
    {{0xF8,0x28,0x19}, "Intel Corporate"},
    /* Realtek */
    {{0x00,0xE0,0x4C}, "Realtek Semiconductor"},
    {{0x52,0x54,0x00}, "Realtek (QEMU/KVM)"},
    /* Broadcom */
    {{0x00,0x10,0x18}, "Broadcom"},
    {{0xB8,0x27,0xEB}, "Raspberry Pi Foundation"},
    {{0xDC,0xA6,0x32}, "Raspberry Pi Trading"},
    {{0xE4,0x5F,0x01}, "Raspberry Pi Trading"},
    /* Cisco */
    {{0x00,0x00,0x0C}, "Cisco Systems"},
    {{0x00,0x1A,0xE3}, "Cisco Systems"},
    {{0x58,0xAC,0x78}, "Cisco Systems"},
    /* Huawei */
    {{0x00,0x18,0x82}, "Huawei Technologies"},
    {{0x28,0x6E,0xD4}, "Huawei Technologies"},
    {{0x70,0x72,0x3C}, "Huawei Technologies"},
    {{0xAC,0xE8,0x7B}, "Huawei Technologies"},
    /* TP-Link */
    {{0x14,0xCC,0xBB}, "TP-Link Technologies"},
    {{0x30,0xDE,0x4B}, "TP-Link Technologies"},
    {{0x50,0xC7,0xBF}, "TP-Link Technologies"},
    {{0x6C,0x72,0x20}, "TP-Link Technologies"},
    {{0x74,0xDA,0xDA}, "TP-Link Technologies"},
    {{0x90,0xF6,0x52}, "TP-Link Technologies"},
    /* ASUS */
    {{0x00,0x1A,0x92}, "ASUSTek Computer"},
    {{0x10,0x02,0xB5}, "ASUSTek Computer"},
    {{0x40,0x16,0x7E}, "ASUSTek Computer"},
    {{0xAC,0x22,0x0B}, "ASUSTek Computer"},
    /* Dell */
    {{0x00,0x14,0x22}, "Dell"},
    {{0x18,0x03,0x73}, "Dell"},
    {{0x78,0x2B,0xCB}, "Dell"},
    /* HP */
    {{0x00,0x0F,0x20}, "Hewlett-Packard"},
    {{0x18,0xA9,0x05}, "Hewlett-Packard"},
    {{0x3C,0xD9,0x2B}, "Hewlett-Packard"},
    /* Lenovo */
    {{0x34,0x17,0xEB}, "Lenovo"},
    {{0x40,0x2C,0x76}, "Lenovo"},
    {{0x54,0xEE,0x75}, "Lenovo"},
    {{0x70,0xF1,0xA1}, "Lenovo"},
    /* Microsoft */
    {{0x00,0x0D,0x3A}, "Microsoft"},
    {{0x28,0x18,0x78}, "Microsoft"},
    {{0x60,0x45,0xBD}, "Microsoft"},
    {{0x7C,0x1E,0x52}, "Microsoft"},
    /* Google */
    {{0x3C,0x5A,0xB4}, "Google"},
    {{0x54,0x60,0x09}, "Google"},
    {{0xF4,0xF5,0xDB}, "Google"},
    /* Amazon */
    {{0x0C,0x47,0xC9}, "Amazon Technologies"},
    {{0x38,0xF7,0x3D}, "Amazon Technologies"},
    {{0x44,0x65,0x0D}, "Amazon Technologies"},
    {{0x68,0x37,0xE9}, "Amazon Technologies"},
    {{0x10,0x4B,0x46}, "Amazon Technologies"},
    /* Xiaomi */
    {{0x28,0xE3,0x1F}, "Xiaomi Communications"},
    {{0x50,0x8F,0x4C}, "Xiaomi Communications"},
    {{0x64,0x09,0x80}, "Xiaomi Communications"},
    {{0xF4,0x8B,0x32}, "Xiaomi Communications"},
    /* Netgear */
    {{0x00,0x14,0x6C}, "Netgear"},
    {{0x20,0x4E,0x7F}, "Netgear"},
    {{0x28,0xC6,0x8E}, "Netgear"},
    {{0x9C,0xD3,0x6D}, "Netgear"},
    /* D-Link */
    {{0x00,0x0D,0x88}, "D-Link"},
    {{0x14,0xD6,0x4D}, "D-Link"},
    {{0x84,0xC9,0xB2}, "D-Link"},
    /* Ubiquiti */
    {{0x04,0x18,0xD6}, "Ubiquiti Networks"},
    {{0x24,0xA4,0x3C}, "Ubiquiti Networks"},
    {{0x44,0xD9,0xE7}, "Ubiquiti Networks"},
    {{0x68,0x72,0x51}, "Ubiquiti Networks"},
    {{0xDC,0x9F,0xDB}, "Ubiquiti Networks"},
    /* MikroTik */
    {{0x08,0x55,0x31}, "MikroTik"},
    {{0x18,0xFD,0x74}, "MikroTik"},
    {{0x2C,0xC8,0x1B}, "MikroTik"},
    {{0x30,0xBE,0x3B}, "MikroTik"},
    {{0x48,0x8F,0x5A}, "MikroTik"},
    {{0x4C,0x5E,0x0C}, "MikroTik"},
    {{0x6C,0x3B,0x6B}, "MikroTik"},
    {{0x74,0x4D,0x28}, "MikroTik"},
    {{0xB8,0x69,0xF4}, "MikroTik"},
    {{0xCC,0x2D,0xE0}, "MikroTik"},
    {{0xDC,0x2C,0x6E}, "MikroTik"},
    {{0xE4,0x8D,0x8C}, "MikroTik"},
    /* Liteon */
    {{0x18,0xF0,0xE4}, "Liteon Technology"},
    {{0x20,0x47,0xDA}, "Liteon Technology"},
    {{0x64,0x00,0x6A}, "Liteon Technology"},
    {{0x74,0xE5,0x0B}, "Liteon Technology"},
    {{0x80,0x00,0x0B}, "Liteon Technology"},
    {{0xCC,0xB0,0xDA}, "Liteon Technology"},
    /* VMware / VirtualBox */
    {{0x00,0x0C,0x29}, "VMware"},
    {{0x00,0x50,0x56}, "VMware"},
    {{0x08,0x00,0x27}, "VirtualBox (Oracle)"},
    /* Qualcomm / Atheros */
    {{0x10,0xCE,0xA9}, "Qualcomm Atheros"},
    {{0x40,0xA5,0xEF}, "Qualcomm Atheros"},
    {{0xE8,0xDE,0x27}, "Qualcomm Atheros"},
    /* MediaTek */
    {{0x00,0x0C,0xE7}, "MediaTek"},
    {{0x78,0x44,0xFD}, "MediaTek"},
    /* Espressif */
    {{0x24,0x6F,0x28}, "Espressif (ESP32)"},
    {{0x30,0xAE,0xA4}, "Espressif (ESP32)"},
    {{0x3C,0x71,0xBF}, "Espressif (ESP32)"},
    {{0xEC,0xFA,0xBC}, "Espressif (ESP8266)"},
    /* Murata */
    {{0x04,0xE6,0x76}, "Murata Manufacturing"},
    {{0x60,0x6D,0x3C}, "Murata Manufacturing"},
    /* Rockchip (MA-M block 10:DC:B6:90:00:00–10:DC:B6:9F:FF:FF)
     * 3-byte match is approximate; Luckfox/Orange Pi use 10:DC:B6:9x */
    {{0x10,0xDC,0xB6}, "Fuzhou Rockchip Electronics"},
    /* WCH — CH182H2 PHY used on Luckfox Lyra Plus (RK3506G2) */
    {{0xAC,0x15,0xA2}, "WCH (Qinheng Microelectronics)"},
};

static const size_t BUILTIN_SIZE = sizeof(BUILTIN_TABLE) / sizeof(BUILTIN_TABLE[0]);

/* ── Dynamic database loaded from system file ────────────────────────────── */

typedef struct {
    uint8_t oui[3];
    char    vendor[VENDOR_LEN];
} DynEntry;

static DynEntry *dyn_table = NULL;
static size_t    dyn_count = 0;
static size_t    dyn_cap   = 0;

/* System OUI database paths to try, in preference order */
static const char *SYSTEM_PATHS[] = {
    "/usr/share/wireshark/manuf",
    "/usr/share/nmap/nmap-mac-prefixes",
    "/var/lib/ieee-data/oui.txt",
    NULL
};

/* ── File parsers ─────────────────────────────────────────────────────────── */

static void dyn_push(const uint8_t *oui, const char *vendor)
{
    if (dyn_count >= dyn_cap) {
        dyn_cap = dyn_cap ? dyn_cap * 2 : 4096;
        dyn_table = realloc(dyn_table, dyn_cap * sizeof(DynEntry));
        if (!dyn_table) { dyn_cap = dyn_count = 0; return; }
    }
    memcpy(dyn_table[dyn_count].oui, oui, 3);
    snprintf(dyn_table[dyn_count].vendor, VENDOR_LEN, "%s", vendor);
    dyn_count++;
}

/*
 * Parse wireshark manuf format:
 *   10:4B:46\tAmazon\tAmazon Technologies Inc.
 * We use the third (long) field if present, else the second.
 */
static int parse_wireshark(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    size_t loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        /* Parse OUI: XX:XX:XX or XX-XX-XX, skip MA-M/MA-S (longer prefixes) */
        unsigned int a, b, c;
        char sep1, sep2;
        if (sscanf(line, "%02X%c%02X%c%02X", &a, &sep1, &b, &sep2, &c) != 5)
            continue;
        if ((sep1 != ':' && sep1 != '-') || (sep2 != ':' && sep2 != '-'))
            continue;

        /* Skip if the 7th char is not a tab/space (longer OUI block) */
        char *p = line + 8;   /* after "XX:XX:XX" */
        if (*p != '\t' && *p != ' ') continue;

        /* Skip to second tab (long vendor name field) */
        char *f1 = strchr(p, '\t');
        char *vendor = NULL;
        if (f1) {
            char *f2 = strchr(f1 + 1, '\t');
            vendor = f2 ? f2 + 1 : f1 + 1;
        }
        if (!vendor || *vendor == '\0') continue;

        /* Strip trailing newline */
        size_t vlen = strlen(vendor);
        while (vlen > 0 && (vendor[vlen-1] == '\n' || vendor[vlen-1] == '\r'))
            vendor[--vlen] = '\0';
        if (vlen == 0) continue;

        uint8_t oui[3] = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
        dyn_push(oui, vendor);
        loaded++;
    }
    fclose(f);
    return (int)loaded;
}

/*
 * Parse nmap-mac-prefixes format:
 *   104B46 Amazon Technologies Inc.
 */
static int parse_nmap(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    size_t loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        unsigned int a, b, c;
        if (sscanf(line, "%02X%02X%02X", &a, &b, &c) != 3) continue;

        char *vendor = line + 6;
        while (*vendor == ' ' || *vendor == '\t') vendor++;
        if (*vendor == '\0') continue;

        size_t vlen = strlen(vendor);
        while (vlen > 0 && (vendor[vlen-1] == '\n' || vendor[vlen-1] == '\r'))
            vendor[--vlen] = '\0';

        uint8_t oui[3] = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
        dyn_push(oui, vendor);
        loaded++;
    }
    fclose(f);
    return (int)loaded;
}

/*
 * Parse IEEE oui.txt format:
 *   10-4B-46   (hex)   Amazon Technologies Inc.
 */
static int parse_ieee(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    size_t loaded = 0;

    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "(hex)")) continue;

        unsigned int a, b, c;
        char sep1, sep2;
        if (sscanf(line, "%02X%c%02X%c%02X", &a, &sep1, &b, &sep2, &c) != 5)
            continue;

        char *vendor = strstr(line, "(hex)");
        if (!vendor) continue;
        vendor += 5;
        while (*vendor == ' ' || *vendor == '\t') vendor++;
        if (*vendor == '\0') continue;

        size_t vlen = strlen(vendor);
        while (vlen > 0 && (vendor[vlen-1] == '\n' || vendor[vlen-1] == '\r'))
            vendor[--vlen] = '\0';

        uint8_t oui[3] = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
        dyn_push(oui, vendor);
        loaded++;
    }
    fclose(f);
    return (int)loaded;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void oui_init(void)
{
    for (int i = 0; SYSTEM_PATHS[i]; i++) {
        int n = 0;
        const char *p = SYSTEM_PATHS[i];

        if (strstr(p, "manuf"))
            n = parse_wireshark(p);
        else if (strstr(p, "nmap"))
            n = parse_nmap(p);
        else if (strstr(p, "oui.txt"))
            n = parse_ieee(p);

        if (n > 0) {
            printf("  [oui] Loaded %d entries from %s\n", n, p);
            return;
        }
    }
    printf("  [oui] Using built-in table (%zu entries). "
           "Install wireshark-common for full coverage:\n"
           "        sudo apt install wireshark-common\n",
           BUILTIN_SIZE);
}

void oui_free(void)
{
    free(dyn_table);
    dyn_table = NULL;
    dyn_count = dyn_cap = 0;
}

void oui_lookup(const uint8_t *mac, char *buf, size_t len)
{
    /* Locally administered (randomized) MAC: bit 1 of first byte */
    if (mac[0] & 0x02) {
        snprintf(buf, len, "Randomized MAC (Privacy)");
        return;
    }

    /* Search dynamic (system) table first */
    for (size_t i = 0; i < dyn_count; i++) {
        if (mac[0] == dyn_table[i].oui[0] &&
            mac[1] == dyn_table[i].oui[1] &&
            mac[2] == dyn_table[i].oui[2])
        {
            snprintf(buf, len, "%s", dyn_table[i].vendor);
            return;
        }
    }

    /* Fall back to built-in table */
    for (size_t i = 0; i < BUILTIN_SIZE; i++) {
        if (mac[0] == BUILTIN_TABLE[i].oui[0] &&
            mac[1] == BUILTIN_TABLE[i].oui[1] &&
            mac[2] == BUILTIN_TABLE[i].oui[2])
        {
            snprintf(buf, len, "%s", BUILTIN_TABLE[i].vendor);
            return;
        }
    }

    snprintf(buf, len, "Unknown");
}
