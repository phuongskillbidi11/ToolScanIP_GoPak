#ifndef WEB_H
#define WEB_H

/*
 * Start a minimal HTTP server on the given port.
 * Runs the ARP scan on startup and on every /rescan request.
 * Serves a styled HTML table at http://<host>:<port>/
 * Never returns (infinite accept loop).
 */
void web_serve(const char *iface, const char *comments_file,
               const char *ssh_user, int port);

#endif /* WEB_H */
