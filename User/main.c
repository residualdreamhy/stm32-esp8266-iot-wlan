#include "stm32f10x.h"
#include "usart.h"
#include "delay.h"
#include "esp8266.h"
#include "dht11.h"
#include "oled.h"
#include <stdio.h>
#include <string.h>

/* ============ 本地服务器配置（自建看板用，见 server/app.py） ============
   把 SERVER_IP 改成你电脑在局域网里的 IP：
   命令行执行 ipconfig，看“IPv4 地址”（如 192.168.1.x）。
   模块是另一台设备，不能用 localhost / 127.0.0.1。
   服务器在工程目录的 server/ 下，命令行运行：python app.py */
#define SERVER_IP   "192.168.0.105"   // 你电脑的局域网 IP
#define SERVER_PORT "8000"
#define SERVER_PATH "/api/data"

/* 把模块后续返回的数据实时转发到调试串口，便于观察 HTTP 响应 */
static void ForwardWifi(uint32_t ms)
{
    uint32_t t = Delay_GetTick();
    while ((Delay_GetTick() - t) < ms)
    {
        if (WIFI_RecvAvailable() > 0)
        {
            uint8_t ch = WIFI_RecvByte();
            USART_SendData(DEBUG_USART, ch);
            while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
        }
    }
}

int main(void)
{
    /* 外设初始化 */
    Delay_Init();

    /* 启动闪烁指示：证明程序已运行、时钟正常（PC13 板载 LED） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    for (uint8_t i = 0; i < 6; i++)
    {
        GPIO_WriteBit(GPIOC, GPIO_Pin_13, (BitAction)(i % 2));
        Delay_ms(200);
    }
    GPIO_SetBits(GPIOC, GPIO_Pin_13);   // 闪烁结束后灭灯

    USART1_Init(115200);   // 调试输出，接 PC
    USART2_Init(115200);   // 连接 ATK-MW8266D 模块
    DHT11_Init();          // 温湿度传感器（PA0）
    OLED_Init();           // OLED 显示屏（PB8/PB9 软件 I2C，江协科技驱动）

    printf("STM32F103 + ATK-MW8266D + DHT11 + OLED start\r\n");

    /* OLED 开机提示 */
    OLED_Clear();
    OLED_ShowString(1, 1, "IOT DEMO");
    OLED_ShowString(2, 1, "BOOTING");

    /* ESP8266 初始化 + 连 WiFi */
    if (ESP8266_Init() == 0)
        printf("[OK] module ready (AT / CWMODE)\r\n");
    else
        printf("[ERR] no response, check wiring / baud / EN pin\r\n");

    /* 连接 WiFi：CWJAP 关联有时需要 10~15 秒，单条指令已在底层给足 20 秒；
       若指令超时但模块其实已连上，用 IsConnected 复检兜底，避免误报“连不上” */
    int wifi_ok = 0;
    for (uint8_t i = 0; i < 2 && !wifi_ok; i++)
    {
        if (i > 0) { printf("[INFO] retry WiFi...\r\n"); Delay_ms(1500); }
        if (ESP8266_JoinAP("TP-LINK_64BD", "88888888") == ESP8266_OK)
            wifi_ok = 1;
        else if (ESP8266_IsConnected() == 0)   // 指令超时，模块却已连上
            wifi_ok = 1;
    }
    if (wifi_ok)
        printf("[OK] WiFi connected\r\n");
    else
        printf("[ERR] WiFi connect failed\r\n");

    /* 打印模块 IP，确认是否真的拿到了网络地址（诊断上传失败用） */
    printf("IP: ");
    ESP8266_PrintIP();
    printf("\r\n");

    char body[64];
    int8_t temp, humi;

    while (1)
    {
        if (DHT11_Read(&temp, &humi) == 0)
        {
            /* 1) 显示在 OLED 上（第一行温度，第二行湿度） */
            OLED_Clear();
            OLED_ShowString(1, 1, "TEMP:");
            OLED_ShowNum(1, 6, (uint32_t)(temp < 0 ? -temp : temp), 2);
            OLED_ShowString(1, 8, "C");
            OLED_ShowString(2, 1, "HUMI:");
            OLED_ShowNum(2, 6, (uint32_t)(humi < 0 ? -humi : humi), 2);
            OLED_ShowString(2, 8, "%");

            /* 2) 通过 WiFi 上传（POST JSON 到本地服务器，浏览器看实时曲线）
                  服务器：server/app.py（python 标准库，零依赖）；
                  SERVER_IP 改成你电脑局域网 IP。postman-echo 仅作验证用，已切到本地。 */
            printf("Temp=%dC Humi=%d%%\r\n", temp, humi);
            printf("upload -> %s:%s%s\r\n", SERVER_IP, SERVER_PORT, SERVER_PATH);
            snprintf(body, sizeof(body), "{\"t\":%d,\"h\":%d}", temp, humi);

            /* 上传前确认 WiFi 仍连着；掉线则重连（便于定位“模块无 IP”导致的上传失败） */
            if (ESP8266_IsConnected() != 0)
            {
                printf("[INFO] WiFi lost, reconnecting...\r\n");
                ESP8266_JoinAP("TP-LINK_64BD", "88888888");
            }

            /* 上传带一次重试：失败后先关闭可能半开的 socket 再试，
               规避 ESP8266 在 Connection:close 后偶发的 CIPSEND 卡死 */
            int uploaded = 0;
            for (uint8_t attempt = 1; attempt <= 2 && !uploaded; attempt++)
            {
                if (ESP8266_HttpPost(SERVER_IP, SERVER_PATH, SERVER_PORT, body) == ESP8266_OK)
                {
                    uploaded = 1;
                    printf("[OK] uploaded (try %d)\r\n", attempt);
                    ForwardWifi(2000);   // 等待并打印服务器响应
                }
                else
                {
                    printf("[ERR] upload failed (try %d)\r\n", attempt);
                    ESP8266_CloseTCP();   // 释放半开连接，避免下次 CIPSEND 卡住
                    Delay_ms(500);
                }
            }
            if (!uploaded)
                printf("[WARN] upload gave up this cycle, retry next loop\r\n");
        }
        else
        {
            printf("[ERR] DHT11 read failed\r\n");
            OLED_Clear();
            OLED_ShowString(1, 1, "DHT11 ERR");
        }

        Delay_ms(5000);   // 每 5 秒采集/上传一次
    }
}
