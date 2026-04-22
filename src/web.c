#include "web.h"
#include "scanner.h"
#include "oui.h"
#include "comments.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Embedded HTML — generated at build time by:
 *   xxd -i web/scanner.html | sed ... > src/scanner_html.h  */
#include "scanner_html.h"

/* ── Shared state ────────────────────────────────────────────────────────── */

static ScanResult g_result;
static char       g_iface[64];
static char       g_comments_file[256];
static char       g_mqttmap_file[256];
static char       g_last_scan[64];

static void do_scan(void)
{
    memset(&g_result, 0, sizeof(g_result));
    oui_init();
    comments_load(g_mqttmap_file, 1);   /* lower priority: auto from MQTT */
    comments_load(g_comments_file, 2);  /* higher priority: manual overrides */
    scan_network(g_iface, &g_result);
    comments_apply(&g_result);
    oui_free();
    comments_free();

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(g_last_scan, sizeof(g_last_scan), "%Y-%m-%d %H:%M:%S", tm);
}

/* ── HTTP helpers ────────────────────────────────────────────────────────── */

static void wstr(int fd, const char *s)
{
    size_t len = strlen(s);
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, s + sent, len - sent);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

static void wfmt(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static void wfmt(int fd, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    wstr(fd, buf);
}

/* ── JSON helpers ────────────────────────────────────────────────────────── */

/* Write a JSON-escaped string to fd */
static void json_str(int fd, const char *s)
{
    wstr(fd, "\"");
    for (; *s; s++) {
        if      (*s == '"')  wstr(fd, "\\\"");
        else if (*s == '\\') wstr(fd, "\\\\");
        else if (*s == '\n') wstr(fd, "\\n");
        else if (*s == '\r') wstr(fd, "\\r");
        else if (*s == '\t') wstr(fd, "\\t");
        else { char c[2] = {*s, '\0'}; wstr(fd, c); }
    }
    wstr(fd, "\"");
}

/* ── Routes ──────────────────────────────────────────────────────────────── */

/* GET / — serve embedded scanner.html */
static void route_root(int fd)
{
    wfmt(fd,
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: text/html; charset=utf-8\r\n"
         "Content-Length: %u\r\n"
         "Connection: close\r\n"
         "\r\n",
         scanner_html_len);
    write(fd, scanner_html, scanner_html_len);
}

/* GET /api/scan — return current scan results as JSON */
static void route_api_scan(int fd)
{
    wstr(fd,
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: application/json\r\n"
         "Connection: close\r\n"
         "\r\n");

    wfmt(fd, "{\"iface\":\"%s\",\"last_scan\":\"%s\",\"count\":%d,\"hosts\":[\n",
         g_iface, g_last_scan, g_result.count);

    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];
        wstr(fd, "  {");
        wstr(fd, "\"ip\":");      json_str(fd, h->ip);
        wstr(fd, ",\"mac\":");    json_str(fd, h->mac_str);
        wstr(fd, ",\"hostname\":"); json_str(fd, h->hostname);
        wstr(fd, ",\"vendor\":"); json_str(fd, h->vendor);
        wstr(fd, ",\"comment\":"); json_str(fd, h->comment);
        wstr(fd, ",\"csrc\":");   json_str(fd, h->comment_src);
        wfmt(fd, ",\"online\":%s}", h->online ? "true" : "false");
        if (i < g_result.count - 1) wstr(fd, ",");
        wstr(fd, "\n");
    }

    wstr(fd, "]}\n");
}

/* GET /rescan — run new scan, return 200 (JS handles reload) */
static void route_rescan(int fd)
{
    printf("  [web] Rescanning %s ...\n", g_iface);
    fflush(stdout);
    do_scan();
    printf("  [web] Done — %d host(s)\n", g_result.count);

    wstr(fd,
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "Connection: close\r\n"
         "\r\n"
         "ok\n");
}

static void route_404(int fd)
{
    wstr(fd,
         "HTTP/1.0 404 Not Found\r\n"
         "Content-Type: text/plain\r\n"
         "Connection: close\r\n"
         "\r\n"
         "Not found\n");
}

/* ── Comment API helpers ─────────────────────────────────────────────────── */

static void url_decode(char *dst, const char *src, size_t dstlen)
{
    size_t i = 0;
    while (*src && i + 1 < dstlen) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

/* Parse a single field from a URL-encoded form string (key=val&key2=val2...) */
static void get_field(const char *src, const char *name, char *out, size_t outlen)
{
    char needle[128];
    snprintf(needle, sizeof(needle), "%s=", name);
    const char *p = strstr(src, needle);
    if (!p) { out[0] = '\0'; return; }
    p += strlen(needle);
    const char *end = strchr(p, '&');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    char raw[1024];
    if (len >= sizeof(raw)) len = sizeof(raw) - 1;
    memcpy(raw, p, len);
    raw[len] = '\0';
    url_decode(out, raw, outlen);
}

/* POST /api/comment  body: key=...&comment=... */
static void route_api_comment_save(int fd, const char *req)
{
    const char *body = strstr(req, "\r\n\r\n");
    if (!body) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n"
                 "Connection: close\r\n\r\nbad request\n");
        return;
    }
    body += 4;

    char key[128], comment[256];
    get_field(body, "key",     key,     sizeof(key));
    get_field(body, "comment", comment, sizeof(comment));

    if (!key[0]) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n"
                 "Connection: close\r\n\r\nmissing key\n");
        return;
    }

    comments_load(g_mqttmap_file, 1);
    comments_load(g_comments_file, 2);
    comments_save(g_comments_file, key, comment);
    comments_apply(&g_result);
    comments_free();

    printf("  [api] saved comment: %s = %s\n", key, comment);

    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
             "Connection: close\r\n\r\n{\"ok\":true}\n");
}

/* DELETE /api/comment?key=...&src=... */
static void route_api_comment_delete(int fd, const char *path)
{
    char key[128], src[16];
    const char *q = strchr(path, '?');
    if (q) {
        get_field(q + 1, "key", key, sizeof(key));
        get_field(q + 1, "src", src, sizeof(src));
    } else {
        key[0] = '\0';
        src[0] = '\0';
    }

    if (!key[0]) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n"
                 "Connection: close\r\n\r\nmissing key\n");
        return;
    }

    /* "mqtt" → delete from mqttmap; everything else → delete from comments */
    const char *target_file = (strcmp(src, "mqtt") == 0)
                              ? g_mqttmap_file : g_comments_file;

    comments_load(g_mqttmap_file, 1);
    comments_load(g_comments_file, 2);
    comments_delete(target_file, key);
    comments_apply(&g_result);
    comments_free();

    printf("  [api] deleted comment: %s (src=%s, file=%s)\n", key, src, target_file);

    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
             "Connection: close\r\n\r\n{\"ok\":true}\n");
}

/* ── Multi-SCP helpers ───────────────────────────────────────────────────── */

static int valid_ip(const char *ip)
{
    int a, b, c, d, n;
    return sscanf(ip, "%d.%d.%d.%d%n", &a, &b, &c, &d, &n) == 4
           && ip[n] == '\0'
           && a >= 0 && a <= 255 && b >= 0 && b <= 255
           && c >= 0 && c <= 255 && d >= 0 && d <= 255;
}

static int valid_path(const char *p)
{
    if (!p || *p != '/') return 0;
    for (const char *c = p; *c; c++)
        if (*c == ';' || *c == '|' || *c == '&' || *c == '`' ||
            *c == '$' || *c == '\'' || *c == '\n' || *c == '\r')
            return 0;
    return 1;
}

/* Minimal JSON string field extractor: {"key":"value"} */
static int json_str_val(const char *json, const char *key,
                        char *out, size_t outlen)
{
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) { out[0] = '\0'; return 0; }
    p += strlen(needle);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n') p++;
    if (*p != '"') { out[0] = '\0'; return 0; }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < outlen) {
        if (*p == '\\') { p++; if (*p) out[i++] = *p++; continue; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (int)i;
}

/* Minimal JSON string array extractor: {"key":["v1","v2"]} */
#define JSON_ITEM_LEN 512
static int json_str_arr(const char *json, const char *key,
                        char (*out)[JSON_ITEM_LEN], int maxitems)
{
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p && *p != '[') p++;
    if (!*p) return 0;
    p++;
    int count = 0;
    while (*p && *p != ']' && count < maxitems) {
        while (*p && *p != '"' && *p != ']') p++;
        if (*p != '"') break;
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < JSON_ITEM_LEN) {
            if (*p == '\\') { p++; if (*p) out[count][i++] = *p++; continue; }
            out[count][i++] = *p++;
        }
        out[count][i] = '\0';
        if (*p == '"') p++;
        count++;
    }
    return count;
}

/* Check if a program exists in PATH */
static int cmd_exists(const char *name)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", name);
    return system(cmd) == 0;
}

/* GET /api/browse?ip=<ip>&path=<path>
   SSH into host, ls -la the path, return JSON entries. */
static void route_api_browse(int fd, const char *query)
{
    char ip[64], path[512];
    get_field(query, "ip",   ip,   sizeof(ip));
    get_field(query, "path", path, sizeof(path));
    if (!path[0]) snprintf(path, sizeof(path), "/root");

    if (!valid_ip(ip) || !valid_path(path)) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"invalid ip or path\"}\n");
        return;
    }

    if (!cmd_exists("sshpass")) {
        wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n"
                 "{\"error\":\"sshpass not found — run: sudo apt install sshpass\"}\n");
        return;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "sshpass -p luckfox ssh "
        "-o StrictHostKeyChecking=no "
        "-o ConnectTimeout=5 "
        "root@%s "
        "\"ls -la \\\"%s\\\"\" 2>&1",
        ip, path);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        wstr(fd, "HTTP/1.0 500 Internal Server Error\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"popen failed\"}\n");
        return;
    }

    char ls_out[8192] = {0};
    size_t n = fread(ls_out, 1, sizeof(ls_out) - 1, fp);
    ls_out[n] = '\0';
    int rc = WEXITSTATUS(pclose(fp));

    if (rc != 0) {
        wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n");
        wstr(fd, "{\"error\":");
        json_str(fd, ls_out[0] ? ls_out : "SSH connection failed");
        wstr(fd, "}\n");
        return;
    }

    /* Parse ls -la: skip 8 tokens to reach filename */
    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
             "Connection: close\r\n\r\n");
    wstr(fd, "{\"path\":");
    json_str(fd, path);
    wstr(fd, ",\"entries\":[");

    int first = 1;
    const char *line = ls_out;
    while (*line) {
        const char *nl = strchr(line, '\n');
        size_t llen = nl ? (size_t)(nl - line) : strlen(line);

        char ftype = *line;
        if (ftype == '-' || ftype == 'd' || ftype == 'l') {
            /* Tokenize ls -la: perms nlinks owner group size mon day time name... */
            /* indices:         0     1      2     3     4    5   6   7    8+    */
            const char *tstart[9]; int tlen9[9]; int ntok = 0;
            const char *p = line;
            while (ntok < 9 && p < line + llen) {
                while (p < line + llen && (*p == ' ' || *p == '\t')) p++;
                if (p >= line + llen) break;
                tstart[ntok] = p;
                if (ntok == 8) {
                    tlen9[ntok] = (int)(line + llen - p);
                } else {
                    while (p < line + llen && *p != ' ' && *p != '\t') p++;
                    tlen9[ntok] = (int)(p - tstart[ntok]);
                }
                ntok++;
            }
            if (ntok < 9) { line = nl ? nl + 1 : line + llen; if (!nl) break; continue; }

            char perms[12] = {0};
            memcpy(perms, tstart[0], tlen9[0] < 11 ? tlen9[0] : 10);

            char owner[32] = {0};
            memcpy(owner, tstart[2], tlen9[2] < 31 ? tlen9[2] : 30);

            long fsz = atol(tstart[4]);

            char date[20] = {0};
            {
                char mo[4]={0}, dy[3]={0}, tm[6]={0};
                memcpy(mo, tstart[5], tlen9[5] < 3 ? tlen9[5] : 3);
                memcpy(dy, tstart[6], tlen9[6] < 2 ? tlen9[6] : 2);
                memcpy(tm, tstart[7], tlen9[7] < 5 ? tlen9[7] : 5);
                snprintf(date, sizeof(date), "%s %s %s", mo, dy, tm);
            }

            size_t namelen = (size_t)tlen9[8];
            char name[256] = {0};
            if (namelen >= sizeof(name)) namelen = sizeof(name) - 1;
            memcpy(name, tstart[8], namelen);
            while (namelen > 0 && (name[namelen-1] == '\r' || name[namelen-1] == '\n'
                                   || name[namelen-1] == ' '))
                name[--namelen] = '\0';
            char *arrow = strstr(name, " -> ");
            if (arrow) *arrow = '\0';

            if (!name[0] || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
                line = nl ? nl + 1 : line + llen;
                if (!nl) break;
                continue;
            }

            char etype = (ftype == 'd') ? 'd' : (ftype == 'l' ? 'l' : 'f');
            if (!first) wstr(fd, ",");
            wstr(fd, "{\"name\":"); json_str(fd, name);
            wfmt(fd, ",\"type\":\"%c\",\"size\":%ld", etype, fsz);
            wstr(fd, ",\"perms\":"); json_str(fd, perms);
            wstr(fd, ",\"owner\":"); json_str(fd, owner);
            wstr(fd, ",\"date\":"); json_str(fd, date);
            wstr(fd, "}");
            first = 0;
        }

        line = nl ? nl + 1 : line + llen;
        if (!nl) break;
    }
    wstr(fd, "]}\n");
}

/* ── Multi-SCP transfer ──────────────────────────────────────────────────── */

#define MAX_SCP_TARGETS  16
#define MAX_SCP_FILES     8

typedef struct {
    int  job_id;
    char source_ip[64];
    char source_paths[MAX_SCP_FILES][JSON_ITEM_LEN];
    int  num_paths;
    char target_ip[64];
    char target_dir[256];
    int  exit_code;
    char output[2048];
} ScpJob;

static void *scp_worker(void *arg)
{
    ScpJob *job = (ScpJob *)arg;
    FILE   *fp;
    size_t  n;
    char    cmd[2048];

    /* Staging dir on the Pi — unique per job index */
    char tmpdir[128];
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/ipscp_%d", job->job_id);

    /* Fresh staging dir */
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", tmpdir, tmpdir);
    system(cmd);

    /* ── Step 1: Download from source device → Pi /tmp ─────────────────── */
    int pos = snprintf(cmd, sizeof(cmd),
        "sshpass -p luckfox scp "
        "-o StrictHostKeyChecking=no "
        "-o ConnectTimeout=30 -r ");
    for (int i = 0; i < job->num_paths && pos < (int)sizeof(cmd) - 256; i++)
        pos += snprintf(cmd + pos, sizeof(cmd) - pos,
            "'root@%s:%s' ", job->source_ip, job->source_paths[i]);
    pos += snprintf(cmd + pos, sizeof(cmd) - pos, "'%s/' 2>&1", tmpdir);

    fp = popen(cmd, "r");
    if (!fp) {
        snprintf(job->output, sizeof(job->output), "download: popen failed");
        job->exit_code = -1;
        goto cleanup;
    }
    n = fread(job->output, 1, sizeof(job->output) / 2 - 1, fp);
    job->output[n] = '\0';
    job->exit_code = WEXITSTATUS(pclose(fp));

    if (job->exit_code != 0) {
        /* Prepend label so user knows it was the download step */
        char tmp2[sizeof(job->output)];
        snprintf(tmp2, sizeof(tmp2), "[download from %s failed]\n%s",
                 job->source_ip, job->output);
        strncpy(job->output, tmp2, sizeof(job->output) - 1);
        goto cleanup;
    }

    /* ── Step 2: Ensure target directory exists ────────────────────────── */
    snprintf(cmd, sizeof(cmd),
        "sshpass -p luckfox ssh "
        "-o StrictHostKeyChecking=no "
        "-o ConnectTimeout=10 "
        "root@%s 'mkdir -p \"%s\"' 2>&1",
        job->target_ip, job->target_dir);
    {
        FILE *mkfp = popen(cmd, "r");
        if (mkfp) {
            char mkbuf[256] = {0};
            size_t mk = fread(mkbuf, 1, sizeof(mkbuf) - 1, mkfp);
            mkbuf[mk] = '\0';
            int mkrc = WEXITSTATUS(pclose(mkfp));
            if (mkrc != 0) {
                snprintf(job->output, sizeof(job->output),
                    "[mkdir -p %s failed]\n%s", job->target_dir, mkbuf);
                job->exit_code = mkrc;
                goto cleanup;
            }
        }
    }

    /* ── Step 3: Upload from Pi /tmp → target device ───────────────────── */
    /* Write a temp shell script so the glob expands correctly */
    {
        char script[128];
        snprintf(script, sizeof(script), "/tmp/ipscp_ul_%s.sh", job->target_ip);

        fp = fopen(script, "w");
        if (!fp) {
            snprintf(job->output, sizeof(job->output), "cannot create upload script");
            job->exit_code = -1;
            goto cleanup;
        }
        fprintf(fp,
            "#!/bin/sh\n"
            "sshpass -p luckfox scp "
            "-o StrictHostKeyChecking=no "
            "-o ConnectTimeout=30 -r "
            "%s/* 'root@%s:%s' 2>&1\n",
            tmpdir, job->target_ip, job->target_dir);
        fclose(fp);

        snprintf(cmd, sizeof(cmd), "chmod +x '%s'", script);
        system(cmd);

        fp = popen(script, "r");
        if (!fp) {
            snprintf(job->output, sizeof(job->output), "upload: popen failed");
            job->exit_code = -1;
            unlink(script);
            goto cleanup;
        }
        n = fread(job->output, 1, sizeof(job->output) - 1, fp);
        job->output[n] = '\0';
        job->exit_code = WEXITSTATUS(pclose(fp));
        unlink(script);

        /* Retry once if host key mismatch */
        if (job->exit_code != 0 &&
            strstr(job->output, "Host key verification failed")) {
            char kc[128];
            snprintf(kc, sizeof(kc),
                "ssh-keygen -R '%s' >/dev/null 2>&1", job->target_ip);
            system(kc);

            fp = fopen(script, "w");
            if (fp) {
                fprintf(fp,
                    "#!/bin/sh\n"
                    "sshpass -p luckfox scp "
                    "-o StrictHostKeyChecking=no "
                    "-o ConnectTimeout=30 -r "
                    "%s/* 'root@%s:%s' 2>&1\n",
                    tmpdir, job->target_ip, job->target_dir);
                fclose(fp);
                snprintf(cmd, sizeof(cmd), "chmod +x '%s'", script);
                system(cmd);
                fp = popen(script, "r");
                if (fp) {
                    n = fread(job->output, 1, sizeof(job->output) - 1, fp);
                    job->output[n] = '\0';
                    job->exit_code = WEXITSTATUS(pclose(fp));
                }
                unlink(script);
            }
        }
    }

cleanup:
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmpdir);
    system(cmd);
    return NULL;
}

/* POST /api/multi-scp
   Mode "multi-target": {"mode":"multi-target","source_ip":"...","source_paths":[...],"target_ips":[...],"target_dir":"..."}
   Mode "multi-dir":    {"mode":"multi-dir","source_ip":"...","source_paths":[...],"target_ip":"...","target_dirs":["...",...]}
   Returns: {"results":[{"target":"<ip-or-dir>","status":"ok"|"fail","output":"..."},...]}
*/
static void route_api_multi_scp(int fd, const char *req)
{
    const char *body = strstr(req, "\r\n\r\n");
    if (!body) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"no body\"}\n");
        return;
    }
    body += 4;

    char mode[32] = "multi-target";
    char source_ip[64];
    char source_paths[MAX_SCP_FILES][JSON_ITEM_LEN];
    char target_ips[MAX_SCP_TARGETS][JSON_ITEM_LEN];
    char target_dirs[MAX_SCP_TARGETS][JSON_ITEM_LEN];
    char target_dir[256]       = "";
    char target_ip_single[64]  = "";

    json_str_val(body, "mode",      mode,            sizeof(mode));
    json_str_val(body, "source_ip", source_ip,       sizeof(source_ip));
    int num_paths    = json_str_arr(body, "source_paths", source_paths, MAX_SCP_FILES);
    int is_multi_dir = (strcmp(mode, "multi-dir") == 0);
    int num_targets;

    if (is_multi_dir) {
        json_str_val(body, "target_ip",  target_ip_single, sizeof(target_ip_single));
        num_targets = json_str_arr(body, "target_dirs", target_dirs, MAX_SCP_TARGETS);
    } else {
        json_str_val(body, "target_dir", target_dir, sizeof(target_dir));
        num_targets = json_str_arr(body, "target_ips",  target_ips,  MAX_SCP_TARGETS);
    }

    if (!valid_ip(source_ip) || num_paths == 0 || num_targets == 0) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"missing or invalid fields\"}\n");
        return;
    }
    if (is_multi_dir && !valid_ip(target_ip_single)) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"invalid target_ip for multi-dir mode\"}\n");
        return;
    }

    if (!cmd_exists("sshpass")) {
        wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n"
                 "{\"error\":\"sshpass not found — run: sudo apt install sshpass\"}\n");
        return;
    }

    /* Normalize target directory/directories: ensure leading '/' and trailing '/' */
    if (is_multi_dir) {
        for (int i = 0; i < num_targets; i++) {
            char *td = target_dirs[i];
            if (!td[0]) {
                strncpy(td, "/root/", JSON_ITEM_LEN - 1);
            } else {
                if (td[0] != '/') {
                    char fixed[JSON_ITEM_LEN + 2];
                    snprintf(fixed, sizeof(fixed), "/%s", td);
                    strncpy(td, fixed, JSON_ITEM_LEN - 1);
                    td[JSON_ITEM_LEN - 1] = '\0';
                }
                size_t tdlen = strlen(td);
                if (td[tdlen - 1] != '/' && tdlen < (size_t)(JSON_ITEM_LEN - 1)) {
                    td[tdlen] = '/'; td[tdlen + 1] = '\0';
                }
            }
            if (!valid_path(td)) {
                wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                         "Connection: close\r\n\r\n{\"error\":\"invalid target dir path\"}\n");
                return;
            }
        }
    } else {
        if (!target_dir[0]) {
            strncpy(target_dir, "/root/", sizeof(target_dir) - 1);
        } else {
            if (target_dir[0] != '/') {
                char fixed[260];
                snprintf(fixed, sizeof(fixed), "/%s", target_dir);
                strncpy(target_dir, fixed, sizeof(target_dir) - 1);
            }
            size_t tdlen = strlen(target_dir);
            if (target_dir[tdlen - 1] != '/' && tdlen < sizeof(target_dir) - 1) {
                target_dir[tdlen] = '/'; target_dir[tdlen + 1] = '\0';
            }
        }
    }

    for (int i = 0; i < num_paths; i++) {
        if (!valid_path(source_paths[i])) {
            wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                     "Connection: close\r\n\r\n{\"error\":\"invalid source path\"}\n");
            return;
        }
    }

    if (is_multi_dir)
        printf("  [scp] %d file(s) from %s → %d dir(s) on %s\n",
               num_paths, source_ip, num_targets, target_ip_single);
    else
        printf("  [scp] %d file(s) from %s → %d target(s) → %s\n",
               num_paths, source_ip, num_targets, target_dir);

    /* Build jobs and launch one thread per target/directory */
    ScpJob    jobs[MAX_SCP_TARGETS];
    pthread_t threads[MAX_SCP_TARGETS];

    for (int i = 0; i < num_targets; i++) {
        memset(&jobs[i], 0, sizeof(jobs[i]));
        jobs[i].job_id    = i;
        snprintf(jobs[i].source_ip,  sizeof(jobs[i].source_ip),  "%s", source_ip);
        jobs[i].num_paths = num_paths;
        for (int j = 0; j < num_paths; j++)
            snprintf(jobs[i].source_paths[j], JSON_ITEM_LEN, "%s", source_paths[j]);

        if (is_multi_dir) {
            snprintf(jobs[i].target_ip,  sizeof(jobs[i].target_ip),  "%s", target_ip_single);
            snprintf(jobs[i].target_dir, sizeof(jobs[i].target_dir), "%s", target_dirs[i]);
        } else {
            snprintf(jobs[i].target_ip,  sizeof(jobs[i].target_ip),  "%s", target_ips[i]);
            snprintf(jobs[i].target_dir, sizeof(jobs[i].target_dir), "%s", target_dir);
        }
        pthread_create(&threads[i], NULL, scp_worker, &jobs[i]);
    }

    for (int i = 0; i < num_targets; i++)
        pthread_join(threads[i], NULL);

    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
             "Connection: close\r\n\r\n");
    wstr(fd, "{\"results\":[");
    for (int i = 0; i < num_targets; i++) {
        if (i) wstr(fd, ",");
        wstr(fd, "{\"target\":");
        /* multi-dir: report destination directory; multi-target: report target IP */
        json_str(fd, is_multi_dir ? jobs[i].target_dir : jobs[i].target_ip);
        wfmt(fd, ",\"status\":\"%s\",\"output\":",
             jobs[i].exit_code == 0 ? "ok" : "fail");
        json_str(fd, jobs[i].output);
        wstr(fd, "}");
    }
    wstr(fd, "]}\n");
}

/* ── Remote filesystem ops ───────────────────────────────────────────────── */

static void ssh_exec(int fd, const char *ip, const char *shellcmd)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "sshpass -p luckfox ssh "
        "-o StrictHostKeyChecking=no -o ConnectTimeout=5 "
        "root@%s '%s' 2>&1", ip, shellcmd);
    FILE *fp = popen(cmd, "r");
    char out[512] = {0};
    int rc = 0;
    if (fp) { fread(out, 1, sizeof(out) - 1, fp); rc = WEXITSTATUS(pclose(fp)); }
    if (rc != 0) {
        wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":");
        json_str(fd, out[0] ? out : "command failed");
        wstr(fd, "}\n");
    } else {
        wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"ok\":true}\n");
    }
}

/* POST /api/mkdir  body: ip=...&path=... */
static void route_api_mkdir(int fd, const char *req)
{
    const char *body = strstr(req, "\r\n\r\n");
    if (!body) { wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                          "Connection: close\r\n\r\n{\"error\":\"no body\"}\n"); return; }
    body += 4;
    char ip[64], path[512];
    get_field(body, "ip",   ip,   sizeof(ip));
    get_field(body, "path", path, sizeof(path));
    if (!valid_ip(ip) || !valid_path(path)) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"invalid ip or path\"}\n"); return;
    }
    char sc[640];
    snprintf(sc, sizeof(sc), "mkdir -p \"%s\"", path);
    ssh_exec(fd, ip, sc);
}

/* POST /api/rm  body: ip=...&path=... */
static void route_api_rm(int fd, const char *req)
{
    const char *body = strstr(req, "\r\n\r\n");
    if (!body) { wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                          "Connection: close\r\n\r\n{\"error\":\"no body\"}\n"); return; }
    body += 4;
    char ip[64], path[512];
    get_field(body, "ip",   ip,   sizeof(ip));
    get_field(body, "path", path, sizeof(path));
    if (!valid_ip(ip) || !valid_path(path) || strlen(path) < 4) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"invalid ip or path\"}\n"); return;
    }
    char sc[640];
    snprintf(sc, sizeof(sc), "rm -rf \"%s\"", path);
    ssh_exec(fd, ip, sc);
}

/* POST /api/rename  body: ip=...&from=...&to=... */
static void route_api_rename(int fd, const char *req)
{
    const char *body = strstr(req, "\r\n\r\n");
    if (!body) { wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                          "Connection: close\r\n\r\n{\"error\":\"no body\"}\n"); return; }
    body += 4;
    char ip[64], from[512], to[512];
    get_field(body, "ip",   ip,   sizeof(ip));
    get_field(body, "from", from, sizeof(from));
    get_field(body, "to",   to,   sizeof(to));
    if (!valid_ip(ip) || !valid_path(from) || !valid_path(to)) {
        wstr(fd, "HTTP/1.0 400 Bad Request\r\nContent-Type: application/json\r\n"
                 "Connection: close\r\n\r\n{\"error\":\"invalid ip or path\"}\n"); return;
    }
    char sc[1100];
    snprintf(sc, sizeof(sc), "mv \"%s\" \"%s\"", from, to);
    ssh_exec(fd, ip, sc);
}

/* ── /regen helpers ──────────────────────────────────────────────────────── */

/* HTML-escape a string into a FILE */
static void fhtml(FILE *f, const char *s)
{
    for (; *s; s++) {
        switch (*s) {
            case '<': fputs("&lt;",  f); break;
            case '>': fputs("&gt;",  f); break;
            case '&': fputs("&amp;", f); break;
            case '"': fputs("&quot;",f); break;
            default:  fputc(*s, f);
        }
    }
}

/* Write /etc/nginx/conf.d/ipscan-proxy.conf from current scan results */
static void write_nginx_proxy_conf(void)
{
    FILE *f = fopen("/etc/nginx/conf.d/ipscan-proxy.conf", "w");
    if (!f) { perror("ipscan-proxy.conf"); return; }

    fprintf(f, "# Auto-generated by ipscanner /regen — do not edit manually\n");

    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];
        if (!strstr(h->comment, "Line") || !strstr(h->comment, "[GM")) continue;
        int last = 0;
        sscanf(h->ip, "%*d.%*d.%*d.%d", &last);
        fprintf(f,
            "\n# %s  (%s)\n"
            "server {\n"
            "    listen %d;\n"
            "    server_name _;\n"
            "    location / {\n"
            "        proxy_pass         http://%s:8000;\n"
            "        proxy_http_version 1.1;\n"
            "        proxy_set_header   Upgrade    $http_upgrade;\n"
            "        proxy_set_header   Connection \"upgrade\";\n"
            "        proxy_set_header   Host       $host;\n"
            "        proxy_read_timeout 3600s;\n"
            "    }\n"
            "}\n",
            h->comment, h->ip, 9000 + last, h->ip);
    }
    fclose(f);
    printf("  [regen] nginx proxy conf written\n");
}

/* Write /var/www/ipscan/index.html from current scan results */
static void write_landing_page(void)
{
    system("mkdir -p /var/www/ipscan");
    FILE *f = fopen("/var/www/ipscan/index.html", "w");
    if (!f) { perror("index.html"); return; }

    int ndev = 0;
    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];
        if (strstr(h->comment, "Line") && strstr(h->comment, "[GM")) ndev++;
    }
    int noth = g_result.count - ndev;

    fputs("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
          "<meta charset=\"utf-8\">\n"
          "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
          "<title>Device Dashboard</title>\n"
          "<style>\n"
          "*{box-sizing:border-box;margin:0;padding:0}\n"
          "body{font-family:'Courier New',monospace;background:#1e1e2e;color:#cdd6f4;padding:24px}\n"
          "h1{color:#89b4fa;margin-bottom:4px;font-size:22px}\n"
          ".sub{color:#6c7086;font-size:12px;margin-bottom:20px}\n"
          ".btn{background:#89b4fa;color:#1e1e2e;border:none;padding:7px 18px;"
          "border-radius:6px;font-family:inherit;font-size:13px;cursor:pointer;"
          "font-weight:bold;text-decoration:none;display:inline-block;margin-bottom:20px}\n"
          ".btn:hover{background:#74c7ec}\n"
          "h2{color:#cba6f7;font-size:15px;margin:20px 0 10px}\n"
          "table{width:100%;border-collapse:collapse;background:#181825;border-radius:8px;"
          "overflow:hidden;margin-bottom:24px;box-shadow:0 2px 8px #00000055}\n"
          "thead tr{background:#313244}\n"
          "th{padding:9px 12px;color:#89b4fa;font-size:11px;text-transform:uppercase;text-align:left}\n"
          "td{padding:8px 12px;border-bottom:1px solid #313244;font-size:13px}\n"
          "tr:last-child td{border-bottom:none}\n"
          "tr[data-port]{cursor:pointer;transition:background .15s}\n"
          "tr[data-port]:hover{background:#313244}\n"
          ".dot{color:#a6e3a1}\n"
          ".comment{color:#cba6f7;font-weight:bold}\n"
          ".mac{color:#6c7086;font-size:11px}\n"
          ".port{background:#313244;color:#a6e3a1;padding:2px 8px;border-radius:4px;font-size:12px}\n"
          "a{color:#89b4fa;text-decoration:none}\n"
          "a:hover{text-decoration:underline}\n"
          "</style>\n</head>\n<body>\n", f);

    fprintf(f, "<h1>&#127760; Device Dashboard</h1>\n");
    fprintf(f, "<div class=\"sub\">Last scan: %s &nbsp;|&nbsp; %d host(s) total</div>\n",
            g_last_scan, g_result.count);
    fputs("<button class=\"btn\" id=\"rescanBtn\" onclick=\"doRescan()\">"
          "&#8635; Rescan</button>\n"
          "<a class=\"btn\" id=\"scannerLink\" href=\"#\" style=\"margin-left:8px\">"
          "&#128269; Live Scanner</a>\n", f);

    /* ── Luckfox devices table ── */
    fprintf(f, "<h2>&#9679; Luckfox Devices (%d)</h2>\n", ndev);
    fputs("<table>\n<thead><tr>"
          "<th>#</th><th>&#9679;</th><th>IP Address</th><th>Comment</th>"
          "<th>Manufacturer</th><th>MAC Address</th><th>Port</th>"
          "</tr></thead>\n<tbody>\n", f);

    int idx = 1;
    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];
        if (!strstr(h->comment, "Line") || !strstr(h->comment, "[GM")) continue;
        int last = 0;
        sscanf(h->ip, "%*d.%*d.%*d.%d", &last);
        int port = 9000 + last;
        char vend[29];
        snprintf(vend, sizeof(vend), "%s", h->vendor);
        fprintf(f, "<tr data-port=\"%d\"><td>%d</td>"
                "<td class=\"dot\">&#9679;</td><td><a href=\"#\">", port, idx++);
        fhtml(f, h->ip);
        fputs("</a></td><td class=\"comment\">", f);
        fhtml(f, h->comment);
        fputs("</td><td>", f);
        fhtml(f, vend);
        fputs("</td><td class=\"mac\">", f);
        fhtml(f, h->mac_str);
        fprintf(f, "</td><td><span class=\"port\">:%d</span></td></tr>\n", port);
    }
    fputs("</tbody>\n</table>\n", f);

    /* ── Other hosts table ── */
    fprintf(f, "<h2>Other Hosts (%d)</h2>\n", noth);
    fputs("<table>\n<thead><tr>"
          "<th>#</th><th>&#9679;</th><th>IP Address</th><th>Comment</th>"
          "<th>Manufacturer</th><th>MAC Address</th><th>Port</th>"
          "</tr></thead>\n<tbody>\n", f);

    idx = 1;
    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];
        if (strstr(h->comment, "Line") && strstr(h->comment, "[GM")) continue;
        char vend[29];
        snprintf(vend, sizeof(vend), "%s", h->vendor);
        fprintf(f, "<tr><td>%d</td><td class=\"dot\">&#9679;</td><td>", idx++);
        fhtml(f, h->ip);
        fputs("</td><td>", f);
        fhtml(f, h->comment);
        fputs("</td><td>", f);
        fhtml(f, vend);
        fputs("</td><td class=\"mac\">", f);
        fhtml(f, h->mac_str);
        fputs("</td><td>-</td></tr>\n", f);
    }
    fputs("</tbody>\n</table>\n", f);

    /* ── JavaScript ── */
    fputs("<script>\n"
          "document.getElementById('scannerLink').href="
          "'http://'+location.hostname+':8080/';\n"
          "function doRescan(){\n"
          "  var btn=document.getElementById('rescanBtn');\n"
          "  btn.disabled=true;btn.textContent='Scanning...';\n"
          "  fetch('/regen').then(function(){location.reload();})"
          ".catch(function(){btn.disabled=false;"
          "btn.innerHTML='&#8635; Rescan';});\n"
          "}\n"
          "document.querySelectorAll('tr[data-port]').forEach(function(row){\n"
          "  var port=row.getAttribute('data-port');\n"
          "  var url='http://'+location.hostname+':'+port+'/';\n"
          "  var link=row.querySelector('a');\n"
          "  if(link)link.href=url;\n"
          "  row.onclick=function(e){\n"
          "    if(e.target.tagName!=='A')window.open(url,'_blank');\n"
          "  };\n"
          "});\n"
          "</script>\n"
          "</body>\n</html>\n", f);

    fclose(f);
    printf("  [regen] landing page written\n");
}

/* GET /regen — rescan + rebuild nginx config + rebuild landing page + reload nginx */
static void route_regen(int fd)
{
    printf("  [web] /regen triggered — scanning %s ...\n", g_iface);
    fflush(stdout);

    do_scan();
    printf("  [web] Found %d host(s)\n", g_result.count);

    write_nginx_proxy_conf();
    write_landing_page();
    system("nginx -t && nginx -s reload");
    printf("  [web] nginx reloaded\n");

    wstr(fd,
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: text/plain\r\n"
         "Connection: close\r\n"
         "\r\n"
         "ok\n");
}

/* ── Public entry point ──────────────────────────────────────────────────── */

void web_serve(const char *iface, const char *comments_file,
               const char *ssh_user, int port)
{
    (void)ssh_user; /* reserved for future use */

    strncpy(g_iface,         iface,         sizeof(g_iface) - 1);
    strncpy(g_comments_file, comments_file, sizeof(g_comments_file) - 1);
    mqttmap_path_from_comments(comments_file, g_mqttmap_file, sizeof(g_mqttmap_file));

    printf("  [web] Scanning %s ...\n", iface);
    fflush(stdout);
    do_scan();
    printf("  [web] Found %d host(s). Serving on http://0.0.0.0:%d/\n\n",
           g_result.count, port);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    fcntl(srv, F_SETFD, FD_CLOEXEC); /* don't leak socket into popen() children */

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(srv); return;
    }
    if (listen(srv, 8) < 0) {
        perror("listen"); close(srv); return;
    }

    while (1) {
        int fd = accept(srv, NULL, NULL);
        if (fd < 0) continue;

        char req[8192] = {0};
        read(fd, req, sizeof(req) - 1);

        char method[8] = "", path[256] = "";
        sscanf(req, "%7s %255s", method, path);

        if (strcmp(method, "GET") == 0) {
            if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0)
                route_root(fd);
            else if (strcmp(path, "/api/scan") == 0)
                route_api_scan(fd);
            else if (strcmp(path, "/rescan") == 0)
                route_rescan(fd);
            else if (strcmp(path, "/regen") == 0)
                route_regen(fd);
            else if (strncmp(path, "/api/browse", 11) == 0) {
                const char *q = strchr(path, '?');
                route_api_browse(fd, q ? q + 1 : "");
            } else
                route_404(fd);
        } else if (strcmp(method, "POST") == 0) {
            if (strncmp(path, "/api/comment", 12) == 0)
                route_api_comment_save(fd, req);
            else if (strcmp(path, "/api/multi-scp") == 0)
                route_api_multi_scp(fd, req);
            else if (strcmp(path, "/api/mkdir") == 0)
                route_api_mkdir(fd, req);
            else if (strcmp(path, "/api/rm") == 0)
                route_api_rm(fd, req);
            else if (strcmp(path, "/api/rename") == 0)
                route_api_rename(fd, req);
            else
                route_404(fd);
        } else if (strcmp(method, "DELETE") == 0) {
            if (strncmp(path, "/api/comment", 12) == 0)
                route_api_comment_delete(fd, path);
            else
                route_404(fd);
        } else {
            route_404(fd);
        }

        close(fd);
    }

    close(srv);
}
