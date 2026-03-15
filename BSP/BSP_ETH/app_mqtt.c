/*
*********************************************************************************************************
*
*   模块名称 : MQTT 客户端
*   文件名称 : app_mqtt.c
*   说    明 : 基于 lwIP 内置 RAW API MQTT 客户端实现定时数据推送。
*              数据来源目前为模拟值，接入真实 ADC 后替换
*              APP_MQTT_GetVoltage() 和 APP_MQTT_GetTemperature() 即可。
*
*   lwIP MQTT RAW API 要点 :
*       - 所有操作在主循环调用，非线程安全，不可在中断里调用
*       - 发布前必须确认连接状态 mqtt_client_is_connected()
*       - mqtt_publish() 是异步的，回调通知发送结果
*
*********************************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include "app_mqtt.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"

/* ============================================================
 * 内部变量
 * ============================================================ */
static mqtt_client_t *s_mqtt_client = NULL;
static uint8_t        s_connected   = 0;
static uint32_t       s_last_pub_tick = 0;

/* ============================================================
 * 模拟数据源（接入真实 ADC 后替换这两个函数）
 * ============================================================ */

/* 模拟内部参考电压，单位 mV，真实值约 1200mV */
static uint32_t APP_MQTT_GetVoltage(void)
{
    /* TODO: 替换为真实 ADC 采样值
     * return (uint32_t)(adc_value * 3300 / 4096);
     */
    static uint32_t v = 1180;
    v += (v % 7) - 3;   /* 模拟轻微抖动 */
    if (v > 1220) v = 1180;
    return v;
}

/* 模拟内部温度，单位 0.1°C，如 253 = 25.3°C */
static int32_t APP_MQTT_GetTemperature(void)
{
    /* TODO: 替换为真实内部温度传感器采样值
     * 正点原子例程公式：temp = ((vtemp - V25) / Avg_Slope) + 25
     */
    static int32_t t = 253;
    t += (t % 3) - 1;   /* 模拟轻微波动 */
    if (t > 280) t = 250;
    return t;
}

/* ============================================================
 * MQTT 回调函数
 * ============================================================ */

/* 连接结果回调 */
static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                                mqtt_connection_status_t status)
{
    (void)client;
    (void)arg;

    if (status == MQTT_CONNECT_ACCEPTED)
    {
        s_connected = 1;
        printf("[MQTT] Connected to broker\r\n");
    }
    else
    {
        s_connected = 0;
        printf("[MQTT] Connection failed, status=%d\r\n", (int)status);
    }
}

/* 发布结果回调（QoS 0 不触发，QoS 1/2 才触发）*/
static void mqtt_pub_request_cb(void *arg, err_t result)
{
    (void)arg;
    if (result != ERR_OK)
    {
        printf("[MQTT] Publish failed, err=%d\r\n", (int)result);
    }
}

/* ============================================================
 * 发布一条消息
 * ============================================================ */
static void mqtt_publish_msg(const char *topic, const char *payload)
{
    if (!s_connected || s_mqtt_client == NULL) return;

    err_t err = mqtt_publish(
        s_mqtt_client,
        topic,
        payload,
        (uint16_t)strlen(payload),
        MQTT_PUBLISH_QOS,
        MQTT_PUBLISH_RETAIN,
        mqtt_pub_request_cb,
        NULL
    );

    if (err != ERR_OK)
    {
        printf("[MQTT] mqtt_publish err=%d topic=%s\r\n", (int)err, topic);
    }
}

/* ============================================================
 * APP_MQTT_Connect
 * 功能：创建 MQTT 客户端并发起连接
 * 调用时机：DHCP 获取到 IP 之后调用一次
 * ============================================================ */
void APP_MQTT_Connect(void)
{
    ip_addr_t broker_ip;
    struct mqtt_connect_client_info_t ci;

    printf("[MQTT] Connecting to broker %s:%d\r\n",
           MQTT_BROKER_IP, MQTT_BROKER_PORT);

    /* 将字符串 IP 转换为 ip_addr_t */
    if (ipaddr_aton(MQTT_BROKER_IP, &broker_ip) == 0)
    {
        printf("[MQTT] Invalid broker IP address\r\n");
        return;
    }

    /* 创建客户端（只创建一次）*/
    if (s_mqtt_client == NULL)
    {
        s_mqtt_client = mqtt_client_new();
        if (s_mqtt_client == NULL)
        {
            printf("[MQTT] mqtt_client_new() failed\r\n");
            return;
        }
    }
    else
    {
        /* 复用前先断开，释放旧的 TCP PCB，防止 ERR_MEM */
        mqtt_disconnect(s_mqtt_client);
    }

    /* 连接参数 */
    memset(&ci, 0, sizeof(ci));
    ci.client_id   = MQTT_CLIENT_ID;
    ci.client_user = NULL;   /* 无需认证，如有密码填这里 */
    ci.client_pass = NULL;
    ci.keep_alive  = 60;     /* 心跳间隔 60s */

    err_t err = mqtt_client_connect(
        s_mqtt_client,
        &broker_ip,
        MQTT_BROKER_PORT,
        mqtt_connection_cb,
        NULL,
        &ci
    );

    if (err != ERR_OK)
    {
        printf("[MQTT] mqtt_client_connect err=%d\r\n", (int)err);
    }
}

/* ============================================================
 * APP_MQTT_Poll
 * 功能：检测连接状态，定时发布数据
 * 调用时机：主循环里每次 MX_LWIP_Process() 之后调用
 * ============================================================ */
void APP_MQTT_Poll(void)
{
    char payload[32];

    /* 未连接：限速重连，每 5 秒才尝试一次 */
    if (s_mqtt_client != NULL && !mqtt_client_is_connected(s_mqtt_client))
    {
        if (s_connected)
        {
            s_connected = 0;
            printf("[MQTT] Disconnected, will reconnect in 5s...\r\n");
        }

        static uint32_t s_reconnect_tick = 0;
        if (HAL_GetTick() - s_reconnect_tick >= 5000)
        {
            s_reconnect_tick = HAL_GetTick();
            APP_MQTT_Connect();
        }
        return;
    }
    if (!s_connected) return;

    /* 定时发布 */
    uint32_t now = HAL_GetTick();
    if (now - s_last_pub_tick < MQTT_PUBLISH_INTERVAL_MS) return;
    s_last_pub_tick = now;

    /* 发布电压 */
    uint32_t voltage = APP_MQTT_GetVoltage();
    snprintf(payload, sizeof(payload), "%lu", (unsigned long)voltage);
    mqtt_publish_msg("gateway/sensor/voltage", payload);

    /* 发布温度（格式：25.3）*/
    int32_t temp = APP_MQTT_GetTemperature();
    snprintf(payload, sizeof(payload), "%ld.%ld",
             (long)(temp / 10), (long)(temp % 10));
    mqtt_publish_msg("gateway/sensor/temp", payload);

    printf("[MQTT] Published: voltage=%lumV  temp=%ld.%ldC\r\n",
           (unsigned long)voltage,
           (long)(temp / 10), (long)(temp % 10));
}

/* ============================================================
 * APP_MQTT_IsConnected
 * ============================================================ */
uint8_t APP_MQTT_IsConnected(void)
{
    return s_connected;
}
