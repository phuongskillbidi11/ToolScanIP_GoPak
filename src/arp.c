#include "arp.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>

/* Broadcast MAC address */
static const uint8_t BROADCAST_MAC[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
static const uint8_t ZERO_MAC[6]      = {0x00,0x00,0x00,0x00,0x00,0x00};

int arp_open_socket(const char *iface)
{
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (sock < 0) {
        perror("socket(AF_PACKET)");
        return -1;
    }

    /* Bind to the specific interface */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl(SIOCGIFINDEX)");
        close(sock);
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ARP);
    sll.sll_ifindex  = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    return sock;
}

int arp_send_request(int sock, const char *iface,
                     const uint8_t *src_mac,
                     const uint8_t *src_ip,
                     const uint8_t *target_ip)
{
    EthArpFrame frame;
    memset(&frame, 0, sizeof(frame));

    /* Ethernet header */
    memcpy(frame.eth.h_dest,   BROADCAST_MAC, 6);
    memcpy(frame.eth.h_source, src_mac,        6);
    frame.eth.h_proto = htons(ETH_P_ARP);

    /* ARP payload */
    frame.arp.htype = htons(ARP_HW_ETHER);
    frame.arp.ptype = htons(ARP_PROTO_IPV4);
    frame.arp.hlen  = 6;
    frame.arp.plen  = 4;
    frame.arp.oper  = htons(ARP_OP_REQUEST);
    memcpy(frame.arp.sha, src_mac,    6);
    memcpy(frame.arp.spa, src_ip,     4);
    memcpy(frame.arp.tha, ZERO_MAC,   6);
    memcpy(frame.arp.tpa, target_ip,  4);

    /* Get interface index for sendto */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
        return -1;

    struct sockaddr_ll dest;
    memset(&dest, 0, sizeof(dest));
    dest.sll_family   = AF_PACKET;
    dest.sll_protocol = htons(ETH_P_ARP);
    dest.sll_ifindex  = ifr.ifr_ifindex;
    dest.sll_halen    = 6;
    memcpy(dest.sll_addr, BROADCAST_MAC, 6);

    ssize_t sent = sendto(sock, &frame, sizeof(frame), 0,
                          (struct sockaddr *)&dest, sizeof(dest));
    return (sent == (ssize_t)sizeof(frame)) ? 0 : -1;
}

int arp_recv_reply(int sock, uint8_t *ip_out, uint8_t *mac_out,
                   int timeout_ms)
{
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(sock + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0)
        return ret; /* 0 = timeout, -1 = error */

    EthArpFrame frame;
    ssize_t n = recv(sock, &frame, sizeof(frame), 0);
    if (n < (ssize_t)sizeof(frame))
        return 0;

    /* We only care about ARP replies */
    if (ntohs(frame.arp.oper) != ARP_OP_REPLY)
        return 0;

    memcpy(ip_out,  frame.arp.spa, 4);
    memcpy(mac_out, frame.arp.sha, 6);
    return 1;
}
