#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <net/ethernet.h>

/* ARP hardware/protocol types */
#define ARP_HW_ETHER   1
#define ARP_PROTO_IPV4 0x0800
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

/* ARP payload (RFC 826) */
typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];   /* sender hardware (MAC) */
    uint8_t  spa[4];   /* sender protocol (IP)  */
    uint8_t  tha[6];   /* target hardware (MAC) */
    uint8_t  tpa[4];   /* target protocol (IP)  */
} __attribute__((packed)) ArpPacket;

/* Full Ethernet frame containing an ARP packet */
typedef struct {
    struct ethhdr eth;
    ArpPacket     arp;
} __attribute__((packed)) EthArpFrame;

/*
 * Open a raw AF_PACKET socket bound to `iface`.
 * Returns socket fd on success, -1 on error.
 * Requires CAP_NET_RAW (run as root or with sudo).
 */
int arp_open_socket(const char *iface);

/*
 * Send an ARP request asking "who has target_ip, tell src_ip".
 * sock      – fd from arp_open_socket()
 * iface     – interface name (e.g. "eth0")
 * src_mac   – 6-byte MAC of the sending interface
 * src_ip    – 4-byte source IPv4 address (network byte order)
 * target_ip – 4-byte target IPv4 address (network byte order)
 * Returns 0 on success, -1 on error.
 */
int arp_send_request(int sock, const char *iface,
                     const uint8_t *src_mac,
                     const uint8_t *src_ip,
                     const uint8_t *target_ip);

/*
 * Wait up to `timeout_ms` milliseconds for an ARP reply.
 * On success fills ip_out[4] and mac_out[6] and returns 1.
 * Returns 0 on timeout, -1 on error.
 */
int arp_recv_reply(int sock, uint8_t *ip_out, uint8_t *mac_out,
                   int timeout_ms);

#endif /* ARP_H */
