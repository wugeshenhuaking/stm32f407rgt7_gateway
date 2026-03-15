#ifndef __APP_MQTT_H
#define __APP_MQTT_H

/*
*********************************************************************************************************
*
*   模块名称 : MQTT 客户端
*   文件名称 : app_mqtt.h
*   说    明 : 基于 lwIP 内置 MQTT 客户端（mqtt.c），通过以太网向 PC 端
*              Broker 推送数据。
*
*   使用方法 :
*       1. 在 main.c 的主循环里：
*              MX_LWIP_Process();          // lwIP 轮询
*              APP_MQTT_Poll();            // MQTT 心跳 + 定时发布
*
*       2. DHCP 获取到 IP 后调用一次：
*              APP_MQTT_Connect();
*
*   Topic 格式 :
*       gateway/sensor/voltage   →  模拟电压值 (mV)
*       gateway/sensor/temp      →  模拟温度值 (°C)
*
*********************************************************************************************************
*/

#include "lwip.h"
#include "lwip/apps/mqtt.h"

/* ============================================================
 * Broker 配置（根据实际环境修改）
 * ============================================================ */
#define MQTT_BROKER_IP      "192.168.63.201"   /* PC 端 Broker IP */
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "stm32f407_gw"
#define MQTT_PUBLISH_QOS    0                 /* QoS 0，不需要 ACK，裸机够用 */
#define MQTT_PUBLISH_RETAIN 0

/* 发布间隔，单位 ms */
#define MQTT_PUBLISH_INTERVAL_MS   2000

/* ============================================================
 * 函数声明
 * ============================================================ */
void APP_MQTT_Connect(void);
void APP_MQTT_Poll(void);
uint8_t APP_MQTT_IsConnected(void);

#endif /* __APP_MQTT_H */
