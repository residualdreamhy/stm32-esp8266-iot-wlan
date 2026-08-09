#ifndef __SSD1306_H
#define __SSD1306_H

#include "stm32f10x.h"

/* OLED 0.96" SSD1306（128x64），软件 I2C，接在 PB6(SCL)/PB7(SDA)
   注意：多数 0.96 寸模块板上已带 4.7k 上拉；若屏不亮，请在 SCL/SDA 各加 4.7k 上拉到 3.3V */
#define SSD1306_I2C_PORT   GPIOB
#define SSD1306_SCL_PIN    GPIO_Pin_6
#define SSD1306_SDA_PIN    GPIO_Pin_7
#define SSD1306_I2C_ADDR   0x78      // 7 位地址左移 1 位（写）

#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES      (SSD1306_HEIGHT / 8)   // 8 页

void SSD1306_Init(void);                 // 初始化并清屏
void SSD1306_Clear(void);                // 清空显存
/* 在指定像素列(col)、页(page 0~7) 绘制字符串；字体 6x8，每行高 8 像素 */
void SSD1306_DrawString(uint8_t col, uint8_t page, const char *str);
void SSD1306_Update(void);               // 把显存推送到屏幕

#endif
