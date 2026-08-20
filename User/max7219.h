#ifndef __MAX7219_H
#define __MAX7219_H

#include "stm32f10x.h"

/* ============ MAX7219 LED 8x8 点阵驱动（软件 SPI）============
   参考文档：MAX7219中文.pdf（电子发烧友）

   【MAX7219 芯片引脚】（24 脚 DIP/SO 封装）
     Pin 1  DIN   串行数据输入（时钟上升沿载入 16 位移位寄存器）
     Pin 12 LOAD  载入数据（上升沿锁存最后 16 位到内部寄存器）
     Pin 13 CLK   时钟输入（最大 10MHz，上升沿移入数据）
     Pin 24 DOUT  串行数据输出（16.5 个时钟周期后有效，级联用）
     Pin 19 V+    +5V
     Pin 4,9 GND  地（两脚必须同时接地）
     Pin 18 ISET  段电流设置（外接电阻到 V+，最小 9.53kΩ）
     Pin 2,3,5-8,10,11 DIG0~DIG7  行驱动（共阴极，低电平有效）
     Pin 14-17,20-23 SEG A~G,DP    列驱动（高电平点亮）

   【16 位串行数据格式】（手册表 1）
     D15~D12: 无效位（×）
     D11~D8:  寄存器地址（4 位）
     D7~D0:   数据（8 位）
     传输顺序：MSB 先发（D15 最先）

   【模块接线】（5 脚 -> STM32）
     ★ VCC  -> 5V（3.3V 也能亮但偏暗）
     ★ GND  -> GND
     ★ DIN  -> PA7  （必须接 DIN，不是 DOUT！DOUT 是输出脚，接了等于白发）
     ★ CS   -> PA4  （对应 MAX7219 的 LOAD 脚）
     ★ CLK  -> PA5  （对应 MAX7219 的 CLK 脚）

   注意：很多模块两边都有引脚，左边 DIN（输入），右边 DOUT（输出）。
         必须接 DIN 那一侧！如果只焊了一边，检查是不是 DOUT 那边。
*/

/* ---- STM32 引脚定义（PA4=CS, PA5=CLK, PA7=DIN）---- */
#define MAX_DIN_PORT    GPIOA
#define MAX_DIN_PIN     GPIO_Pin_7
#define MAX_CLK_PORT    GPIOA
#define MAX_CLK_PIN     GPIO_Pin_5
#define MAX_CS_PORT     GPIOA
#define MAX_CS_PIN      GPIO_Pin_4

/* ---- MAX7219 寄存器地址（手册表 2）---- */
#define REG_DIGIT_0     0x01   /* 第 1 行（行 0） */
#define REG_DIGIT_1     0x02
#define REG_DIGIT_2     0x03
#define REG_DIGIT_3     0x04
#define REG_DIGIT_4     0x05
#define REG_DIGIT_5     0x06
#define REG_DIGIT_6     0x07
#define REG_DIGIT_7     0x08   /* 第 8 行（行 7） */
#define REG_DECODE      0x09   /* 译码模式：0x00=不译码（直接控每个 LED） */
#define REG_INTENSITY   0x0A   /* 亮度 0x00~0x0F */
#define REG_SCAN_LIMIT   0x0B   /* 扫描位数 0x00~0x07（0x07=全部 8 行） */
#define REG_SHUTDOWN    0x0C   /* 0x00=掉电关闭，0x01=正常工作 */
#define REG_TEST         0x0F   /* 0x00=正常，0x01=全亮测试 */

/* ---- API ---- */
void MAX7219_Init(void);                    /* 初始化：退出掉电、不译码、扫8行、中等亮度、清屏 */
void MAX7219_Write(uint8_t reg, uint8_t data); /* 写寄存器（16 位：地址+数据） */
void MAX7219_Clear(void);                   /* 清屏（8 行全灭） */
void MAX7219_SetBrightness(uint8_t level);  /* 亮度 0~15 */
void MAX7219_Test(uint8_t on);              /* 1=全亮测试，0=正常（慎用，部分兼容芯片关不掉） */
void MAX7219_SetPixel(uint8_t row, uint8_t col, uint8_t on); /* 点亮/熄灭单个像素 */
void MAX7219_ShowChar(char ch);             /* 显示单个字符 */
void MAX7219_ShowNum2(int8_t num);          /* 显示 0~99 两位数（十位1秒+个位1秒） */
void MAX7219_Scroll(const char* text, uint16_t ms_per_col);  /* 滚动显示字符串 */

#endif
