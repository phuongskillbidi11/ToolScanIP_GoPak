#include "comments.h"
#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char key[64];       /* MAC (XX:XX:XX:XX:XX:XX) or IP (x.x.x.x) */
    char text[COMMENT_LEN];
    int  source;        /* 1 = mqttmap, 2 = manual comments */
} CommentEntry;

static CommentEntry *entries = NULL;
static int           entry_count = 0;
static int           entry_cap   = 0;

static void push(const char *key, const char *text, int source)
{
    if (entry_count >= entry_cap) {
        entry_cap = entry_cap ? entry_cap * 2 : 16;
        entries = realloc(entries, entry_cap * sizeof(CommentEntry));
        if (!entries) { entry_cap = entry_count = 0; return; }
    }
    snprintf(entries[entry_count].key,  sizeof(entries[0].key),  "%s", key);
    snprintf(entries[entry_count].text, sizeof(entries[0].text), "%s", text);
    entries[entry_count].source = source;
    entry_count++;
}

/* source: 1 = mqttmap (lower priority), 2 = manual comments (higher priority) */
void comments_load(const char *path, int source)
{
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key  = line;
        char *text = eq + 1;

        char *end = key + strlen(key) - 1;
        while (end > key && (*end == ' ' || *end == '\t')) *end-- = '\0';
        while (*text == ' ' || *text == '\t') text++;

        if (*key && *text)
            push(key, text, source);
    }
    fclose(f);
}

void comments_apply(ScanResult *result)
{
    for (int i = 0; i < result->count; i++) {
        Host *h = &result->hosts[i];
        const char *by_ip  = NULL;  int ip_src  = 0;
        const char *by_mac = NULL;  int mac_src = 0;

        for (int j = 0; j < entry_count; j++) {
            if (strcasecmp(entries[j].key, h->mac_str) == 0) {
                /* If we already have a match from a different source → override */
                if (by_mac && mac_src != entries[j].source)
                    mac_src = 3; /* override */
                else
                    mac_src = entries[j].source;
                by_mac = entries[j].text;
            } else if (strcmp(entries[j].key, h->ip) == 0) {
                by_ip  = entries[j].text;
                ip_src = entries[j].source;
            }
        }

        const char *chosen = by_mac ? by_mac : by_ip;
        int         src    = by_mac ? mac_src : ip_src;

        if (chosen) {
            snprintf(h->comment, COMMENT_LEN, "%s", chosen);
            if      (src == 1) snprintf(h->comment_src, sizeof(h->comment_src), "mqtt");
            else if (src == 2) snprintf(h->comment_src, sizeof(h->comment_src), "manual");
            else if (src == 3) snprintf(h->comment_src, sizeof(h->comment_src), "override");
            else               snprintf(h->comment_src, sizeof(h->comment_src), "none");
        } else {
            snprintf(h->comment_src, sizeof(h->comment_src), "none");
        }
    }
}

int comments_save(const char *path, const char *key, const char *comment)
{
    FILE *f = fopen(path, "r");
    char lines[512][256];
    int  nlines = 0;
    int  found  = -1;   /* index of the slot we'll write the updated entry into */

    if (f) {
        while (nlines < 512 && fgets(lines[nlines], 256, f))
            nlines++;
        fclose(f);
    }

    /* Scan all lines: blank out every duplicate for this key, remember the last */
    for (int i = 0; i < nlines; i++) {
        char tmp[256];
        snprintf(tmp, sizeof(tmp), "%s", lines[i]);
        tmp[strcspn(tmp, "\r\n")] = '\0';
        char *eq = strchr(tmp, '=');
        if (!eq) continue;
        *eq = '\0';
        char *end = tmp + strlen(tmp) - 1;
        while (end > tmp && (*end == ' ' || *end == '\t')) *end-- = '\0';

        if (strcasecmp(tmp, key) == 0) {
            if (found >= 0) lines[found][0] = '\0'; /* blank earlier duplicate */
            found = i;
        }
    }

    if (found >= 0)
        snprintf(lines[found], 256, "%s=%s\n", key, comment);
    else if (nlines < 512)
        snprintf(lines[nlines++], 256, "%s=%s\n", key, comment);

    /* Write back, skipping blanked-out duplicate lines */
    f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < nlines; i++)
        if (lines[i][0]) fputs(lines[i], f);
    fclose(f);

    /* Update in-memory: remove ALL matching entries then add the new value */
    for (int i = 0; i < entry_count; ) {
        if (strcasecmp(entries[i].key, key) == 0) {
            for (int j = i; j < entry_count - 1; j++)
                entries[j] = entries[j + 1];
            entry_count--;
        } else {
            i++;
        }
    }
    if (comment[0])
        push(key, comment, 2); /* manual */

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

/* Derive ~/.ipscanner.mqttmap from ~/.ipscanner.comments path */
void mqttmap_path_from_comments(const char *comments_path,
                                char *out, size_t outlen)
{
    const char *suffix = ".comments";
    size_t clen = strlen(comments_path);
    size_t slen = strlen(suffix);
    if (clen > slen &&
        strcasecmp(comments_path + clen - slen, suffix) == 0) {
        snprintf(out, outlen, "%.*s.mqttmap", (int)(clen - slen), comments_path);
    } else {
        snprintf(out, outlen, "%s.mqttmap", comments_path);
    }
}

/* Register a MAC in the MQTT map file (no comment — MQTT sync fills it).
 * Does nothing if the MAC is already present in the file. */
int mqttmap_register(const char *path, const char *mac)
{
    if (!mac || !mac[0]) return 0;

    /* Check if already present */
    FILE *f = fopen(path, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *eq  = strchr(line, '=');
            char *end = eq ? eq : line + strlen(line);
            while (end > line && (end[-1] == ' ' || end[-1] == '\t')) end--;
            size_t klen = (size_t)(end - line);
            char key_buf[64] = {0};
            if (klen < sizeof(key_buf)) {
                memcpy(key_buf, line, klen);
                if (strcasecmp(key_buf, mac) == 0) {
                    fclose(f);
                    printf("Already registered: %s in %s\n", mac, path);
                    return 0;
                }
            }
        }
        fclose(f);
    }

    /* Append MAC= (empty comment — MQTT sync will fill it) */
    f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s=\n", mac);
    fclose(f);
    printf("Registered: %s in %s  (run mqtt-sync-comments.sh to fill label)\n",
           mac, path);
    return 0;
}

void comments_free(void)
{
    free(entries);
    entries = NULL;
    entry_count = entry_cap = 0;
}
