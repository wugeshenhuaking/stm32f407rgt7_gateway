/*
 * app_ping.c
 * Improved Ping with DNS callback
 */

#include "app_ping.h"
#include "lwip/raw.h"
#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/dns.h"
#include "lwip/ip.h"
#include <stdio.h>
#include <string.h>

static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);


static struct raw_pcb *ping_pcb = NULL;
static uint16_t ping_seq_num = 0;
static uint32_t ping_start_time = 0;

/* DNS 解析完成回调（改名避免冲突） */
static void my_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr != NULL && !ip_addr_isany(ipaddr)) {
        printf("DNS resolved: %s -> %s\r\n", name, ip4addr_ntoa(ipaddr));
        ping_ip(ip_2_ip4(ipaddr));
    } else {
        printf("DNS resolve failed for %s\r\n", name);
    }
}

/* 初始化 Ping */
void ping_init(void)
{
    ping_pcb = raw_new(IP_PROTO_ICMP);
    if (ping_pcb) {
        raw_recv(ping_pcb, ping_recv, NULL);
        raw_bind(ping_pcb, IP_ADDR_ANY);
        printf("Ping module initialized.\r\n");
    } else {
        printf("Failed to create raw pcb!\r\n");
    }
}

/* 接收 ICMP 回复 */
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    struct icmp_echo_hdr *iecho;
    uint32_t rtt;

    if (p->len >= sizeof(struct icmp_echo_hdr)) {
        iecho = (struct icmp_echo_hdr *)p->payload;

        if (iecho->type == ICMP_ER && iecho->id == 0x1234) {
            rtt = HAL_GetTick() - ping_start_time;
            printf("Reply from %s: bytes=32 time=%d ms TTL=64\r\n", 
                   ip4addr_ntoa(addr), rtt);
        }
    }
    pbuf_free(p);
    return 1;
}

/* Ping IP 地址 */
void ping_ip(const ip4_addr_t *ip)
{
    struct pbuf *p;
    struct icmp_echo_hdr *iecho;
    size_t len = sizeof(struct icmp_echo_hdr) + 32;

    p = pbuf_alloc(PBUF_IP, (u16_t)len, PBUF_RAM);
    if (p == NULL) return;

    iecho = (struct icmp_echo_hdr *)p->payload;
    iecho->type = ICMP_ECHO;
    iecho->code = 0;
    iecho->id = 0x1234;
    iecho->seqno = htons(++ping_seq_num);

    ping_start_time = HAL_GetTick();

    for (uint16_t i = 0; i < 32; i++) {
        ((uint8_t*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (uint8_t)i;
    }

    iecho->chksum = 0;
    iecho->chksum = inet_chksum(iecho, len);

    raw_sendto(ping_pcb, p, ip);
    pbuf_free(p);
}

/* 主 ping 函数 */
void ping(const char *target)
{
    ip4_addr_t ipaddr;

    printf("Pinging %s ...\r\n", target);

    if (ip4addr_aton(target, &ipaddr)) {
        // 是IP地址
        ping_ip(&ipaddr);
    } else {
        // 是域名，异步DNS解析
        err_t err = dns_gethostbyname(target, &ipaddr, my_dns_found, NULL);
        if (err == ERR_OK) {
            ping_ip(&ipaddr);
        } else if (err == ERR_INPROGRESS) {
            printf("DNS resolving in progress...\r\n");
        } else {
            printf("Failed to start DNS query!\r\n");
        }
    }
}