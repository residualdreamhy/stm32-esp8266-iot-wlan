#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"
#include <stdio.h>

/* 调试串口：连接 PC（ST-Link 虚拟串口 或 USB-TTL 模块） */
#define DEBUG_USART  USART1
/* WiFi 模块串口：连接 ATK-MW8266D */
#define WIFI_USART   USART2

void     USART1_Init(uint32_t baud);              // 调试串口（PA9-TX / PA10-RX）
void     USART2_Init(uint32_t baud);              // WiFi 模块串口（PA2-TX / PA3-RX，开接收中断）
void     USART_SendString(USART_TypeDef* USARTx, const char* str);
void     WIFI_SendData(const char* data, uint16_t len); // 发送原始数据给模块

uint16_t WIFI_RecvAvailable(void);                // 接收缓冲中可读字节数
uint8_t  WIFI_RecvByte(void);                     // 读取 1 字节（无数据返回 0）
void     WIFI_RecvFlush(void);                    // 清空接收缓冲

void     WIFI_UART_IRQ(void);                     // 供 USART2 中断服务程序调用

#endif
