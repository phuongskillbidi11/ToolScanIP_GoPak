#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <net/if.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "scanner.h"
#include "display.h"
#include "oui.h"
#include "comments.h"
#include "web.h"

#define ANSI_RESET  "\033[0m"
#define ANSI_BOLD   "\033[1m"

/* Default comments file: ~/.ipscanner.comments
 * When run under sudo, use the real user's home (SUDO_USER), not root's. */
static void default_comments_path(char *buf, size_t len)
{
    const char *sudo_user = getenv("SUDO_USER");
    if (sudo_user && sudo_user[0]) {
        struct passwd *pw = getpwnam(sudo_user);
        if (pw) {
            snprintf(buf, len, "%s/.ipscanner.comments", pw->pw_dir);
            return;
        }
    }
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/tmp";
    }
    snprintf(buf, len, "%s/.ipscanner.comments", home);
}

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-i interface] [-C key=comment] [-f file] [-U user] [-P pass] [-w port] [-j file] [-l] [-h]\n"
            "\n"
            "  -i <iface>        Network interface to scan (default: eth0)\n"
            "  -C key=comment    Save a comment (key = IP or MAC address)\n"
            "  -f <file>         Comments file (default: ~/.ipscanner.comments)\n"
            "  -U <user>         SSH username for interactive connect (default: root)\n"
            "  -P <pass>         SSH password — uses sshpass for auto-login\n"
            "  -w <port>         Start web server on given port (e.g. -w 8080)\n"
            "  -j <file>         Save scan results as JSON\n"
            "  -l                List available interfaces\n"
            "  -h                Show this help\n"
            "\n"
            "Examples:\n"
            "  sudo %s -i eth0\n"
            "  sudo %s -i eth0 -U root -P luckfox\n"
            "  sudo %s -i eth0 -w 8080\n"
            "  sudo %s -i eth0 -j /tmp/scan.json\n"
            "  sudo %s -i eth0 -C 192.168.20.26=\"Luckfox Lyra Plus IoT\"\n"
            "\n"
            "After scanning, enter a host number to SSH into it.\n",
            prog, prog, prog, prog, prog, prog);
}

static void list_interfaces(void)
{
    printf("Available interfaces:\n");
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    while (fgets(line, sizeof(line), f)) {
        char iface[64];
        if (sscanf(line, " %63[^:]:", iface) == 1)
            if (strcmp(iface, "lo") != 0)
                printf("  %s\n", iface);
    }
    fclose(f);
}

int main(int argc, char *argv[])
{
    char iface[IFNAMSIZ] = "eth0";
    char comments_file[256];
    char save_key[64]   = "";
    char save_val[128]  = "";
    char ssh_user[64]   = "root";
    char ssh_pass[128]  = "";
    char json_file[256] = "";
    int  web_port       = 0;
    int  opt;

    default_comments_path(comments_file, sizeof(comments_file));

    while ((opt = getopt(argc, argv, "i:C:f:U:P:w:j:lh")) != -1) {
        switch (opt) {
        case 'i':
            strncpy(iface, optarg, IFNAMSIZ - 1);
            break;
        case 'U':
            strncpy(ssh_user, optarg, sizeof(ssh_user) - 1);
            break;
        case 'P':
            strncpy(ssh_pass, optarg, sizeof(ssh_pass) - 1);
            break;
        case 'w':
            web_port = atoi(optarg);
            if (web_port <= 0 || web_port > 65535) {
                fprintf(stderr, "Error: invalid port '%s'\n", optarg);
                return 1;
            }
            break;
        case 'j':
            strncpy(json_file, optarg, sizeof(json_file) - 1);
            break;
        case 'C': {
            char *eq = strchr(optarg, '=');
            if (!eq) {
                fprintf(stderr, "Error: -C requires key=comment format\n");
                return 1;
            }
            *eq = '\0';
            snprintf(save_key, sizeof(save_key), "%s", optarg);
            snprintf(save_val, sizeof(save_val), "%s", eq + 1);
            break;
        }
        case 'f':
            snprintf(comments_file, sizeof(comments_file), "%s", optarg);
            break;
        case 'l':
            list_interfaces();
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    /* Check for root */
    if (geteuid() != 0) {
        fprintf(stderr,
                "Error: this program requires root privileges.\n"
                "Run with:  sudo %s -i %s\n", argv[0], iface);
        return 1;
    }

    /* Web server mode — takes over, never returns */
    if (web_port) {
        web_serve(iface, comments_file, ssh_user, web_port);
        return 0;
    }

    display_header();
    oui_init();
    comments_load(comments_file);

    /* Save/update comment (after load so in-memory cache is current) */
    if (save_key[0]) {
        if (comments_save(comments_file, save_key, save_val) == 0)
            printf("Saved: %s = %s\n  File: %s\n",
                   save_key, save_val, comments_file);
        else
            fprintf(stderr, "Error: could not write to %s\n", comments_file);
    }

    ScanResult result;
    memset(&result, 0, sizeof(result));

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (scan_network(iface, &result) < 0) {
        fprintf(stderr, "Scan failed.\n");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) +
                     (t1.tv_nsec - t0.tv_nsec) / 1e9;

    comments_apply(&result);
    display_results(&result);
    oui_free();
    comments_free();

    /* Write JSON output if requested */
    if (json_file[0]) {
        FILE *jf = fopen(json_file, "w");
        if (jf) {
            fprintf(jf, "[\n");
            for (int i = 0; i < result.count; i++) {
                const Host *h = &result.hosts[i];
                #define JESC(s) do { \
                    for (const char *_p = (s); *_p; _p++) { \
                        if (*_p == '"' || *_p == '\\') fputc('\\', jf); \
                        fputc(*_p, jf); \
                    } \
                } while (0)
                fprintf(jf, "  {\"ip\":\"");    JESC(h->ip);
                fprintf(jf, "\",\"mac\":\"");    JESC(h->mac_str);
                fprintf(jf, "\",\"hostname\":\""); JESC(h->hostname);
                fprintf(jf, "\",\"vendor\":\""); JESC(h->vendor);
                fprintf(jf, "\",\"comment\":\""); JESC(h->comment);
                fprintf(jf, "\",\"online\":%s}%s\n",
                        h->online ? "true" : "false",
                        i < result.count - 1 ? "," : "");
                #undef JESC
            }
            fprintf(jf, "]\n");
            fclose(jf);
            printf("  JSON saved: %s\n", json_file);
        } else {
            fprintf(stderr, "  Warning: could not write JSON to %s\n", json_file);
        }
    }

    printf("  Scan completed in %.2f seconds.\n\n", elapsed);
    printf("  Comments file: %s\n", comments_file);
    printf("  Add comment:   %s -C \"IP=note\" or -C \"MAC=note\"\n\n",
           argv[0]);

    /* ── Interactive SSH prompt ─────────────────────────────────────────── */
    if (result.count == 0)
        return 0;

    printf("  Enter host number to connect, or 'q' to quit:\n\n");

    char line[32];
    while (1) {
        printf("  > ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        if (line[0] == 'q' || line[0] == 'Q') break;

        char *endp;
        long n = strtol(line, &endp, 10);
        if (*endp || n < 1 || n > result.count) {
            printf("  Invalid — enter 1-%d or 'q'\n", result.count);
            continue;
        }

        const Host *host = &result.hosts[n - 1];
        char target[128];
        snprintf(target, sizeof(target), "%s@%s", ssh_user, host->ip);

        /* Determine password:
         *   1. Explicit -P flag takes priority.
         *   2. Comment contains "Line" and "[GM" → Luckfox device → "luckfox".
         *   3. Otherwise plain ssh (will prompt). */
        const char *pass = NULL;
        if (ssh_pass[0]) {
            pass = ssh_pass;
        } else if (strstr(host->comment, "Line") && strstr(host->comment, "[GM")) {
            pass = "luckfox";
        }

        if (pass)
            printf("\n  Connecting to %s (auto-login) ...\n\n", target);
        else
            printf("\n  Connecting to %s ...\n\n", target);
        fflush(stdout);

        pid_t pid = fork();
        if (pid == 0) {
            if (pass) {
                execlp("sshpass", "sshpass", "-p", pass,
                       "ssh",
                       "-o", "StrictHostKeyChecking=no",
                       "-o", "ConnectTimeout=5",
                       target, NULL);
            }
            execlp("ssh", "ssh",
                   "-o", "StrictHostKeyChecking=no",
                   "-o", "ConnectTimeout=5",
                   target, NULL);
            _exit(127);
        } else if (pid > 0) {
            int wstatus;
            waitpid(pid, &wstatus, 0);
            printf("\n  Connection closed.\n\n");
        } else {
            perror("fork");
        }
    }

    return 0;
}
