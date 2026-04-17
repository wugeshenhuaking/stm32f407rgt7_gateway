// #ifndef _PING_H_
// #define _PING_H_

// #include "lwip/ip_addr.h"

// void ping_init(void);
// void ping_send_request(const char *hostname);

// #endif

#ifndef APP_PING_H
#define APP_PING_H

#include "lwip/ip_addr.h"

void ping_init(void);
void ping(const char *target);                    // 支持域名或IP
void ping_ip(const ip4_addr_t *ip);               // 直接 ping IP

#endif