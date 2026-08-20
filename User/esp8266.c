#include "esp8266.h"
#include "usart.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

/* 缓存模块当前 STA IP，供 ESP8266_IsConnected() 判断 DHCP 是否真正拿到地址；
   ESP8266_PrintIP() 调用时刷新此值 */
static char g_sta_ip[20] = "0.0.0.0";

/* 等待期望字符串出现在接收缓冲中，超时返回 ESP8266_TIMEOUT */
static int WaitForToken(const char* token, uint32_t timeout_ms)
{
    uint32_t start = Delay_GetTick();
    uint16_t tlen = (uint16_t)strlen(token);
    uint16_t matched = 0;

    while ((Delay_GetTick() - start) < timeout_ms)
    {
        while (WIFI_RecvAvailable() > 0)
        {
            uint8_t ch = WIFI_RecvByte();
            /* 把模块返回的内容同时转发到调试串口，方便观察 */
            USART_SendData(DEBUG_USART, ch);
            while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);

            if (ch == (uint8_t)token[matched])
            {
                matched++;
                if (matched == tlen) return ESP8266_OK;
            }
            else
            {
                matched = (ch == (uint8_t)token[0]) ? 1 : 0;
            }
        }
    }
    return ESP8266_TIMEOUT;
}

/* 发送 AT 指令并等待期望回复；expect 为 NULL 时仅延时 timeout_ms 毫秒 */
int ESP8266_SendCmd(const char* cmd, const char* expect, uint32_t timeout_ms)
{
    WIFI_RecvFlush();                       // 丢弃上次残留数据
    USART_SendString(WIFI_USART, cmd);
    if (expect == NULL)
    {
        Delay_ms(timeout_ms);
        return ESP8266_OK;
    }
    return WaitForToken(expect, timeout_ms);
}

/* 上电初始化：测试 AT、关闭回显、设为 Station 模式 */
int ESP8266_Init(void)
{
    Delay_ms(3000);                         // 上电/复位后多等 3 秒，确保模块完成 boot
                                        // （boot 未完成时发 AT 会回 busy p...）

    /* 重试 AT 最多 5 次，兼容模块启动/偶发 busy 状态 */
    int at_ok = 0;
    for (uint8_t i = 0; i < 5; i++)
    {
        if (ESP8266_SendCmd("AT\r\n", "OK", 1500) == ESP8266_OK)
        {
            at_ok = 1;
            break;
        }
        Delay_ms(800);
    }
    if (at_ok == 0) return -1;

    ESP8266_SendCmd("ATE0\r\n", "OK", 1000);            // 关闭回显，输出更干净
    if (ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK", 2000) != ESP8266_OK)
        return -2;                          // 1 = Station 模式
    ESP8266_SendCmd("AT+CIPMUX=0\r\n", "OK", 1000);     // 单连接模式，状态更干净，避免多连接残留
    return 0;
}

/* 连接 WiFi 热点 */
int ESP8266_JoinAP(const char* ssid, const char* pwd)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);
    return ESP8266_SendCmd(buf, "OK", 20000);  // 联网较慢，给足时间（CWJAP 关联常需 10~15 秒）
}

/* 等待出现 t1 或 t2 任一期望字符串（超时返回 -1）；期间实时转发到调试串口 */
static int WaitForAny(const char* t1, const char* t2, uint32_t timeout_ms)
{
    uint32_t start = Delay_GetTick();
    uint16_t l1 = (uint16_t)strlen(t1), l2 = (uint16_t)strlen(t2);
    uint16_t m1 = 0, m2 = 0;

    while ((Delay_GetTick() - start) < timeout_ms)
    {
        while (WIFI_RecvAvailable() > 0)
        {
            uint8_t ch = WIFI_RecvByte();
            USART_SendData(DEBUG_USART, ch);          // 转发，便于观察模块原始回显
            while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);

            if (ch == (uint8_t)t1[m1]) { m1++; if (m1 == l1) return ESP8266_OK; }
            else m1 = (ch == (uint8_t)t1[0]) ? 1 : 0;

            if (l2 && ch == (uint8_t)t2[m2]) { m2++; if (m2 == l2) return ESP8266_OK; }
            else if (l2) m2 = (ch == (uint8_t)t2[0]) ? 1 : 0;
        }
    }
    return ESP8266_TIMEOUT;
}

/* 建立 TCP 连接（发送 AT+CIPSTART，接受 OK 或 ALREADY CONNECTED） */
int ESP8266_ConnectTCP(const char* ip, const char* port)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);

    for (uint8_t try = 0; try < 2; try++)
    {
        if (try > 0)                            // 第二次先关旧连接再重连
        {
            /* 等 OK 而非只延时 400ms：CIPCLOSE 要发 TCP FIN 包，模块需要 1~2 秒
               只延时 400ms 就发 CIPSTART 会让模块回 "busy p..." */
            ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 2000);
            Delay_ms(500);                       // 额外等模块状态机归位
        }
        WIFI_RecvFlush();                       // 清掉上次残留回显，避免误匹配
        /* 发 CIPSTART 前先发 AT 同步，确保模块不处于 busy 状态
           （CWJAP? / CIFSR 等指令后模块内部可能还在处理，直接 CIPSTART 会 busy） */
        ESP8266_SendCmd("AT\r\n", "OK", 1000);
        WIFI_RecvFlush();
        USART_SendString(WIFI_USART, buf);      // 真正发出 AT+CIPSTART 指令
        /* 接受 OK 或 ALREADY CONNECTED，都视为连接成功 */
        if (WaitForAny("OK", "ALREADY CONNECTED", 8000) == ESP8266_OK)
            return 0;
    }
    return ESP8266_TIMEOUT;
}

/* 关闭当前 TCP 连接（失败/重连前调用，避免半开 socket 卡住后续 CIPSEND） */
int ESP8266_CloseTCP(void)
{
    return ESP8266_SendCmd("AT+CIPCLOSE\r\n", NULL, 500);
}

/* 查询当前是否已连上 WiFi 且 DHCP 拿到地址（双校验：关联 + IP 非 0.0.0.0）
   返回 0 = 已连可用；返回 -1 = 不可用（关联失败或 IP 未分配） */
int ESP8266_IsConnected(void)
{
    /* 步骤1：查 WiFi 关联状态（CWJAP? 关联成功回 OK） */
    if (ESP8266_SendCmd("AT+CWJAP?\r\n", "OK", 3000) != ESP8266_OK)
        return -1;

    /* 步骤2：检查 IP 缓存是否有效；首次进入或刚重连时主动刷新一次 */
    if (g_sta_ip[0] == '\0' || strcmp(g_sta_ip, "0.0.0.0") == 0)
    {
        ESP8266_PrintIP();   /* 内部会刷新 g_sta_ip */
        if (g_sta_ip[0] == '\0' || strcmp(g_sta_ip, "0.0.0.0") == 0)
            return -1;      /* DHCP 还没拿到地址 = 不可用，避免假连接 */
    }
    return 0;
}

/* 打印模块分配到的 IP，并把 STA IP 缓存到 g_sta_ip（供 IsConnected 判断） */
void ESP8266_PrintIP(void)
{
    WIFI_RecvFlush();                                  // 丢弃上次残留
    USART_SendString(WIFI_USART, "AT+CIFSR\r\n");
    /* 在等待期间实时转发模块回显，这样 IP 才会真正显示在调试串口 */
    char buf[64] = {0};
    int n = 0;
    uint32_t start = Delay_GetTick();
    while ((Delay_GetTick() - start) < 1000 && n < 63)
    {
        while (WIFI_RecvAvailable() > 0)
        {
            uint8_t ch = WIFI_RecvByte();
            USART_SendData(DEBUG_USART, ch);
            while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
            if (n < 63) buf[n++] = ch;
        }
    }
    buf[n] = '\0';
    /* 解析 +CIFSR:STAIP,"x.x.x.x" 中的 IP，写入全局缓存 */
    char* p = strstr(buf, "STAIP,\"");
    if (p)
    {
        sscanf(p, "STAIP,\"%19[^\"]\"", g_sta_ip);
    }
}

/* 通过 CIPSEND 发送指定长度的数据 */
int ESP8266_SendTCP(const char* data, uint16_t len)
{
    char buf[32];                                  // 必须足够大：最大 "AT+CIPSEND=65535\r\n" 含结尾共 19 字节
    snprintf(buf, sizeof(buf), "AT+CIPSEND=%d\r\n", len);
    if (ESP8266_SendCmd(buf, ">", 2000) != ESP8266_OK)
        return -1;
    WIFI_SendData(data, len);
    return WaitForToken("SEND OK", 3000);
}

/* 简易 HTTP GET 示例 */
int ESP8266_HttpGet(const char* host, const char* path, const char* port)
{
    if (ESP8266_ConnectTCP(host, port) != ESP8266_OK)
        return -1;
    char req[300];
    int n = snprintf(req, sizeof(req),
        "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    return ESP8266_SendTCP(req, (uint16_t)n);
}

/* 简易 HTTP POST：发送 JSON 等正文到服务器（用于上传传感器数据） */
int ESP8266_HttpPost(const char* host, const char* path, const char* port, const char* body)
{
    if (ESP8266_ConnectTCP(host, port) != ESP8266_OK)
        return -1;

    char buf[400];
    int  hlen  = snprintf(buf, sizeof(buf),
        "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\nConnection: close\r\n\r\n",
        path, host, (int)strlen(body));
    int  total = hlen + (int)strlen(body);
    if (total > (int)sizeof(buf)) return -2;
    memcpy(buf + hlen, body, strlen(body));     // 头部 + 正文拼成一段一次性发送

    return ESP8266_SendTCP(buf, (uint16_t)total);
}
