#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/* OLED 0.96" SSD1306（128x64），软件 I2C，接在 PB8(SCL)/PB9(SDA)
   驱动来自江协科技（原江科大）OLED 例程，配 OLED_Font.h 中的 OLED_F8x16 8x16 字库。
   注意：GPIO 配置为开漏输出，需外加上拉电阻（多数 0.96 寸模块板上已带 4.7k 上拉）。 */

void OLED_Init(void);                              // 初始化并清屏
void OLED_Clear(void);                             // 清屏
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);   // 显示一个字符
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String); // 显示字符串
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);    // 显示无符号数字
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length); // 显示带符号数字
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);  // 显示十六进制
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);  // 显示二进制

#endif
