#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

/* DHT11 接在 PA0（单总线，需外部 4.7k 上拉到 3.3V；模块板上一般已带） */
#define DHT11_GPIO_PORT    GPIOA
#define DHT11_GPIO_PIN     GPIO_Pin_0

void DHT11_Init(void);            // 初始化 GPIO 并上拉空闲
/* 读取温湿度：成功返回 0，temp/humi 为整数（摄氏度 / 百分比）
   失败返回负值（-1 无响应，-2 校验错） */
int  DHT11_Read(int8_t *temp, int8_t *humi);

#endif
