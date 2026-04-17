#include "app_network.h"

void app_network_init(void)
{
    // MX_LWIP_Init();
    printf("Network initialized.\r\n");
}

void app_network_handler(void)
{
    app_dhcp_poll();
    app_mqtt_poll();
    app_ping_poll();
    app_tcpService_poll();
    app_mDNS_poll();
}