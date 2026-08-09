#include "delay.h"

static volatile uint32_t g_sys_tick = 0;

/* 初始化 SysTick 为 1ms 定时中断；并使能 DWT 周期计数器用于微秒延时 */
void Delay_Init(void)
{
    /* 使能 Cortex-M3 的 DWT 周期计数器（供 Delay_us 使用，精度到 1us） */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* SystemCoreClock 由 system_stm32f10x.c 按实际系统时钟填充，
       默认 HSI 8MHz，若开启 PLL 则为 72MHz，此处计算均正确 */
    if (SysTick_Config(SystemCoreClock / 1000) != 0)
    {
        while (1);   // 配置失败时停机，避免后续延时错乱
    }
}

/* 毫秒级阻塞延时 */
void Delay_ms(uint32_t ms)
{
    uint32_t start = g_sys_tick;
    while ((g_sys_tick - start) < ms);
}

/* 微秒级阻塞延时（基于 DWT 周期计数器，精度高，用于 DHT11 单总线时序） */
void Delay_us(uint32_t us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < cycles);
}

/* 获取当前毫秒计数（用于 AT 指令超时判断） */
uint32_t Delay_GetTick(void)
{
    return g_sys_tick;
}

/* 在 SysTick 中断中调用，累加时基 */
void Delay_SysTickHandler(void)
{
    g_sys_tick++;
}
