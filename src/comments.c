#include "comments.h"
#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char key[64];       /* MAC (XX:XX:XX:XX:XX:XX) or IP (x.x.x.x) */
    char text[COMMENT_LEN];
} CommentEntry;

static CommentEntry *entries = NULL;
static int           entry_count = 0;
static int           entry_cap   = 0;

static void push(const char *key, const char *text)
{
    if (entry_count >= entry_cap) {
        entry_cap = entry_cap ? entry_cap * 2 : 16;
        entries = realloc(entries, entry_cap * sizeof(CommentEntry));
        if (!entries) { entry_cap = entry_count = 0; return; }
    }
    snprintf(entries[entry_count].key,  sizeof(entries[0].key),  "%s", key);
    snprintf(entries[entry_count].text, sizeof(entries[0].text), "%s", text);
    entry_count++;
}

void comments_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Skip blank lines and comments */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* Split on first '=' */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key  = line;
        char *text = eq + 1;

        /* Strip trailing whitespace from key */
        char *end = key + strlen(key) - 1;
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';

        /* Skip leading whitespace from text */
        while (*text == ' ' || *text == '\t') text++;

        if (*key && *text)
            push(key, text);
    }
    fclose(f);
}

void comments_apply(ScanResult *result)
{
    for (int i = 0; i < result->count; i++) {
        Host *h = &result->hosts[i];
        const char *by_ip  = NULL;
        const char *by_mac = NULL;

        for (int j = 0; j < entry_count; j++) {
            if (strcasecmp(entries[j].key, h->mac_str) == 0)
                by_mac = entries[j].text;
            else if (strcmp(entries[j].key, h->ip) == 0)
                by_ip  = entries[j].text;
        }

        /* MAC takes priority over IP */
        const char *chosen = by_mac ? by_mac : by_ip;
        if (chosen)
            snprintf(h->comment, COMMENT_LEN, "%s", chosen);
    }
}

int comments_save(const char *path, const char *key, const char *comment)
{
    /* Read existing lines, replace if key exists, append if not */
    FILE *f = fopen(path, "r");
    char lines[512][256];
    int  nlines = 0;
    int  found  = 0;

    if (f) {
        while (nlines < 512 && fgets(lines[nlines], 256, f))
            nlines++;
        fclose(f);
    }

    /* Look for existing entry to update */
    for (int i = 0; i < nlines; i++) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        tmp[strcspn(tmp, "\r\n")] = '\0';
        char *eq = strchr(tmp, '=');
        if (!eq) continue;
        *eq = '\0';
        /* Strip trailing whitespace */
        char *end = tmp + strlen(tmp) - 1;
        while (end > tmp && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (strcasecmp(tmp, key) == 0) {
            snprintf(lines[i], 256, "%s=%s\n", key, comment);
            found = 1;
            break;
        }
    }

    if (!found && nlines < 512)
        snprintf(lines[nlines++], 256, "%s=%s\n", key, comment);

    /* Write back */
    f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < nlines; i++)
        fputs(lines[i], f);
    fclose(f);

    /* Update in-memory entries so a subsequent comments_apply() sees the
     * new value without requiring a full comments_load() from disk. */
    for (int i = 0; i < entry_count; i++) {
        if (strcasecmp(entries[i].key, key) == 0) {
            snprintf(entries[i].text, sizeof(entries[i].text), "%s", comment);
            return 0;
        }
    }
    /* Not found in memory — add it (only if text is non-empty, matching
     * the same rule used by comments_load). */
    if (comment[0])
        push(key, comment);

    return 0;
}

int comments_delete(const char *path, const char *key)
{
    FILE *f = fopen(path, "r");
    char lines[512][256];
    int  nlines = 0;

    if (f) {
        while (nlines < 512 && fgets(lines[nlines], 256, f))
            nlines++;
        fclose(f);
    }

    f = fopen(path, "w");
    if (!f) return -1;

    for (int i = 0; i < nlines; i++) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        tmp[strcspn(tmp, "\r\n")] = '\0';
        char *eq = strchr(tmp, '=');
        if (eq) {
            *eq = '\0';
            char *end = tmp + strlen(tmp) - 1;
            while (end > tmp && (*end == ' ' || *end == '\t')) *end-- = '\0';
            if (strcasecmp(tmp, key) == 0) continue; /* skip this entry */
        }
        fputs(lines[i], f);
    }
    fclose(f);

    /* Remove from in-memory entries */
    for (int i = 0; i < entry_count; i++) {
        if (strcasecmp(entries[i].key, key) == 0) {
            for (int j = i; j < entry_count - 1; j++)
                entries[j] = entries[j + 1];
            entry_count--;
            break;
        }
    }

    return 0;
}

void comments_free(void)
{
    free(entries);
    entries = NULL;
    entry_count = entry_cap = 0;
}
