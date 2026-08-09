#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

#define ESP8266_OK       0
#define ESP8266_TIMEOUT -1

int  ESP8266_SendCmd(const char* cmd, const char* expect, uint32_t timeout_ms);
int  ESP8266_Init(void);                          // AT 测试 + 设为 Station 模式
int  ESP8266_JoinAP(const char* ssid, const char* pwd);
int  ESP8266_ConnectTCP(const char* ip, const char* port);
int  ESP8266_CloseTCP(void);                     // 关闭当前 TCP 连接（失败/重连前调用）
int  ESP8266_IsConnected(void);                     // 查询是否已连上 AP
void ESP8266_PrintIP(void);                         // 打印分配到的 IP（诊断用）
int  ESP8266_SendTCP(const char* data, uint16_t len);
int  ESP8266_HttpGet(const char* host, const char* path, const char* port);
int  ESP8266_HttpPost(const char* host, const char* path, const char* port, const char* body);

#endif
