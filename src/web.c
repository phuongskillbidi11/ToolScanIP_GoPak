#include "web.h"
#include "scanner.h"
#include "oui.h"
#include "comments.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ── Shared state ────────────────────────────────────────────────────────── */

static ScanResult g_result;
static char       g_iface[64];
static char       g_comments_file[256];
static char       g_ssh_user[64];
static char       g_last_scan[64];   /* human-readable timestamp */

static void do_scan(void)
{
    memset(&g_result, 0, sizeof(g_result));
    oui_init();
    comments_load(g_comments_file);
    scan_network(g_iface, &g_result);
    comments_apply(&g_result);
    oui_free();
    comments_free();

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(g_last_scan, sizeof(g_last_scan), "%Y-%m-%d %H:%M:%S", tm);
}

/* ── HTTP helpers ────────────────────────────────────────────────────────── */

/* Write a string to fd, ignoring partial-write edge cases on local sockets */
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

/* printf-style write to fd via a stack buffer */
static void wfmt(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static void wfmt(int fd, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    wstr(fd, buf);
}

/* ── HTML page ───────────────────────────────────────────────────────────── */

static const char *HTML_HEAD =
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"utf-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
"<title>IP Scanner</title>\n"
"<style>\n"
"*{box-sizing:border-box;margin:0;padding:0}\n"
"body{font-family:'Courier New',monospace;background:#1e1e2e;color:#cdd6f4;"
     "padding:24px;font-size:14px}\n"
"h1{color:#89b4fa;font-size:22px;margin-bottom:4px}\n"
".subtitle{color:#6c7086;font-size:12px;margin-bottom:20px}\n"
".topbar{display:flex;align-items:center;gap:12px;margin-bottom:20px}\n"
".btn{background:#89b4fa;color:#1e1e2e;border:none;padding:7px 18px;"
     "border-radius:6px;font-family:inherit;font-size:13px;cursor:pointer;"
     "font-weight:bold;text-decoration:none;display:inline-block}\n"
".btn:hover{background:#74c7ec}\n"
".btn-rescan{background:#a6e3a1;color:#1e1e2e}\n"
".btn-rescan:hover{background:#94e2d5}\n"
"table{width:100%;border-collapse:collapse;background:#181825;"
      "border-radius:10px;overflow:hidden;box-shadow:0 2px 16px #00000066}\n"
"thead tr{background:#313244}\n"
"th{padding:10px 14px;color:#89b4fa;font-weight:bold;text-align:left;"
   "font-size:12px;text-transform:uppercase;letter-spacing:.05em}\n"
"td{padding:9px 14px;border-bottom:1px solid #313244;vertical-align:middle}\n"
"tr:last-child td{border-bottom:none}\n"
"tbody tr{cursor:pointer;transition:background .15s}\n"
"tbody tr:hover{background:#313244}\n"
"tbody tr.selected{background:#45475a}\n"
".dot{color:#a6e3a1;font-size:16px}\n"
".num{color:#6c7086;text-align:right;width:36px}\n"
".ip{color:#f9e2af;font-weight:bold}\n"
".mac{color:#6c7086;font-size:12px}\n"
".vendor{color:#cdd6f4}\n"
".comment{color:#cba6f7}\n"
".hostname{color:#cdd6f4}\n"
".ssh-panel{margin-top:20px;background:#181825;border:1px solid #313244;"
           "border-radius:8px;padding:16px;display:none}\n"
".ssh-panel h3{color:#89b4fa;margin-bottom:10px;font-size:14px}\n"
".ssh-box{display:flex;align-items:center;gap:10px;flex-wrap:wrap}\n"
".ssh-cmd{background:#313244;color:#a6e3a1;padding:8px 14px;"
         "border-radius:6px;font-family:inherit;font-size:13px;"
         "flex:1;word-break:break-all}\n"
".copy-btn{background:#313244;color:#cdd6f4;border:1px solid #45475a;"
          "padding:7px 14px;border-radius:6px;font-family:inherit;"
          "font-size:12px;cursor:pointer}\n"
".copy-btn:hover{background:#45475a}\n"
".copy-btn.copied{color:#a6e3a1;border-color:#a6e3a1}\n"
".info{color:#6c7086;font-size:12px;margin-top:8px}\n"
".luckfox-badge{background:#cba6f7;color:#1e1e2e;font-size:10px;"
               "padding:2px 6px;border-radius:4px;margin-left:6px;"
               "font-weight:bold;vertical-align:middle}\n"
"</style>\n"
"</head>\n"
"<body>\n";

static const char *HTML_FOOT =
"<script>\n"
"var sel=-1;\n"
"function pick(n,ip,comment,autopass){\n"
"  var rows=document.querySelectorAll('tbody tr');\n"
"  rows.forEach(function(r){r.classList.remove('selected');});\n"
"  if(sel===n){sel=-1;document.getElementById('ssh-panel').style.display='none';return;}\n"
"  sel=n;\n"
"  rows[n].classList.add('selected');\n"
"  var cmd=autopass\n"
"    ?'sshpass -p luckfox ssh root@'+ip\n"
"    :'ssh root@'+ip;\n"
"  document.getElementById('ssh-cmd-text').textContent=cmd;\n"
"  document.getElementById('host-info').textContent='Host: '+ip+(comment?' — '+comment:'');\n"
"  document.getElementById('ssh-panel').style.display='block';\n"
"  var cb=document.getElementById('copy-btn');\n"
"  cb.textContent='Copy';\n"
"  cb.className='copy-btn';\n"
"}\n"
"function copyCmd(){\n"
"  var cmd=document.getElementById('ssh-cmd-text').textContent;\n"
"  navigator.clipboard.writeText(cmd).then(function(){\n"
"    var cb=document.getElementById('copy-btn');\n"
"    cb.textContent='Copied!';\n"
"    cb.className='copy-btn copied';\n"
"    setTimeout(function(){cb.textContent='Copy';cb.className='copy-btn';},2000);\n"
"  });\n"
"}\n"
"</script>\n"
"</body></html>\n";

static void send_page(int fd)
{
    /* HTTP headers */
    wstr(fd,
         "HTTP/1.0 200 OK\r\n"
         "Content-Type: text/html; charset=utf-8\r\n"
         "Connection: close\r\n"
         "\r\n");

    wstr(fd, HTML_HEAD);

    /* Title + toolbar */
    wfmt(fd,
         "<h1>&#127760; IP Scanner</h1>\n"
         "<div class=\"subtitle\">Interface: %s &nbsp;|&nbsp; "
         "Last scan: %s &nbsp;|&nbsp; %d host(s) found</div>\n"
         "<div class=\"topbar\">\n"
         "  <a class=\"btn btn-rescan\" href=\"/rescan\">&#8635; Rescan</a>\n"
         "</div>\n",
         g_iface, g_last_scan, g_result.count);

    if (g_result.count == 0) {
        wstr(fd, "<p style=\"color:#f38ba8\">No hosts found.</p>\n");
        wstr(fd, HTML_FOOT);
        return;
    }

    /* Table */
    wstr(fd,
         "<table>\n"
         "<thead><tr>"
         "<th class=\"num\">#</th>"
         "<th>&#9679;</th>"
         "<th>Hostname</th>"
         "<th>IP Address</th>"
         "<th>Manufacturer</th>"
         "<th>MAC Address</th>"
         "<th>Comments</th>"
         "</tr></thead>\n"
         "<tbody>\n");

    for (int i = 0; i < g_result.count; i++) {
        const Host *h = &g_result.hosts[i];

        /* Detect Luckfox auto-password pattern */
        int autopass = (strstr(h->comment, "Line") && strstr(h->comment, "[GM")) ? 1 : 0;

        /* Escape single quotes in comment for JS */
        char safe_comment[COMMENT_LEN * 2];
        int sc = 0;
        for (int k = 0; h->comment[k] && sc < (int)sizeof(safe_comment) - 2; k++) {
            if (h->comment[k] == '\'') safe_comment[sc++] = '\\';
            safe_comment[sc++] = h->comment[k];
        }
        safe_comment[sc] = '\0';

        wfmt(fd,
             "<tr onclick=\"pick(%d,'%s','%s',%d)\">\n"
             "  <td class=\"num\">%d</td>\n"
             "  <td><span class=\"dot\">&#9679;</span></td>\n"
             "  <td class=\"hostname\">%s</td>\n"
             "  <td class=\"ip\">%s</td>\n"
             "  <td class=\"vendor\">%s</td>\n"
             "  <td class=\"mac\">%s</td>\n"
             "  <td class=\"comment\">%s%s</td>\n"
             "</tr>\n",
             i, h->ip, safe_comment, autopass,
             i + 1,
             h->hostname,
             h->ip,
             h->vendor,
             h->mac_str,
             h->comment,
             autopass ? "<span class=\"luckfox-badge\">luckfox</span>" : "");
    }

    wstr(fd, "</tbody></table>\n");

    /* SSH panel (shown on row click) */
    wstr(fd,
         "<div class=\"ssh-panel\" id=\"ssh-panel\">\n"
         "  <h3>SSH Command</h3>\n"
         "  <div class=\"ssh-box\">\n"
         "    <div class=\"ssh-cmd\" id=\"ssh-cmd-text\"></div>\n"
         "    <button class=\"copy-btn\" id=\"copy-btn\" onclick=\"copyCmd()\">Copy</button>\n"
         "  </div>\n"
         "  <div class=\"info\" id=\"host-info\"></div>\n"
         "</div>\n");

    wstr(fd, HTML_FOOT);
}

static void send_redirect(int fd, const char *location)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "HTTP/1.0 302 Found\r\nLocation: %s\r\nConnection: close\r\n\r\n",
             location);
    wstr(fd, buf);
}

static void send_404(int fd)
{
    wstr(fd,
         "HTTP/1.0 404 Not Found\r\n"
         "Content-Type: text/plain\r\n"
         "Connection: close\r\n"
         "\r\n"
         "Not found\n");
}

/* ── Public entry point ──────────────────────────────────────────────────── */

void web_serve(const char *iface, const char *comments_file,
               const char *ssh_user, int port)
{
    strncpy(g_iface,         iface,         sizeof(g_iface) - 1);
    strncpy(g_comments_file, comments_file, sizeof(g_comments_file) - 1);
    strncpy(g_ssh_user,      ssh_user,      sizeof(g_ssh_user) - 1);

    /* Initial scan */
    printf("  [web] Scanning %s ...\n", iface);
    fflush(stdout);
    do_scan();
    printf("  [web] Found %d host(s). Serving on http://0.0.0.0:%d/\n\n",
           g_result.count, port);

    /* Create TCP socket */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return; }

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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

    /* Accept loop */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int fd = accept(srv, (struct sockaddr *)&client_addr, &client_len);
        if (fd < 0) continue;

        /* Read request line */
        char req[512];
        ssize_t n = read(fd, req, sizeof(req) - 1);
        if (n <= 0) { close(fd); continue; }
        req[n] = '\0';

        /* Parse method and path from first line */
        char method[8] = "", path[256] = "";
        sscanf(req, "%7s %255s", method, path);

        if (strcmp(method, "GET") == 0) {
            if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
                send_page(fd);
            } else if (strcmp(path, "/rescan") == 0) {
                printf("  [web] Rescanning %s ...\n", g_iface);
                fflush(stdout);
                do_scan();
                printf("  [web] Done — %d host(s)\n", g_result.count);
                send_redirect(fd, "/");
            } else if (strcmp(path, "/favicon.ico") == 0) {
                send_404(fd);
            } else {
                send_404(fd);
            }
        }

        close(fd);
    }

    close(srv);
}
