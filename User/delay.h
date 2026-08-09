#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

/* =========================================================================
 * 兼容处理：部分旧版 core_cm3.h 未提供 DWT 定义，这里手动定义寄存器，
 * 使 Delay_us / DHT11 的 DWT 周期计数微秒延时可用（仅 Cortex-M3/M4 有用）。
 * 用 #ifndef 保护，若 CMSIS 已定义则自动跳过，不会冲突。
 * ========================================================================= */
#ifndef DWT
#define DWT_BASE 0xE0001000UL
typedef struct
{
    volatile uint32_t CTRL;     /* 0x00 DWT 控制寄存器 */
    volatile uint32_t CYCCNT;   /* 0x04 周期计数（自增，CPU 每时钟 +1） */
    volatile uint32_t CPICNT;   /* 0x08 */
    volatile uint32_t EXCCNT;   /* 0x0C */
    volatile uint32_t SLEEPCNT; /* 0x10 */
    volatile uint32_t LSUCNT;   /* 0x14 */
    volatile uint32_t FOLDCNT;  /* 0x18 */
    volatile uint32_t PCSR;     /* 0x1C */
} DWT_Type;
#define DWT ((DWT_Type *) DWT_BASE)
#endif

#ifndef DWT_CTRL_CYCCNTENA_Msk
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)   /* DWT CTRL 的 CYCCNTENA 位：使能周期计数 */
#endif

#ifndef CoreDebug_DEMCR_TRCENA_Msk
#define CoreDebug_DEMCR_TRCENA_Msk (1UL << 24)  /* DEMCR 的 TRCENA 位：使能 DWT/ITM/ETM 跟踪 */
#endif

void     Delay_Init(void);        // 初始化 SysTick，提供 1ms 时基
void     Delay_ms(uint32_t ms);   // 毫秒级阻塞延时
void     Delay_us(uint32_t us);   // 微秒级阻塞延时（DWT，供 DHT11 时序）
uint32_t Delay_GetTick(void);     // 获取当前毫秒计数
void     Delay_SysTickHandler(void); // 供 stm32f10x_it.c 的 SysTick_Handler 调用

#endif
