#include "usart.h"

/* ---------- WiFi 串口接收环形缓冲 ---------- */
#define RX_BUF_SIZE  512
static uint8_t  s_rx_buf[RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;   // 写指针（中断中自增）
static volatile uint16_t s_rx_tail = 0;   // 读指针（主循环中自增）

/* 重定向 printf 到 DEBUG_USART（Keil 需勾选 "Use MicroLIB"） */
int fputc(int ch, FILE* f)
{
    USART_SendData(DEBUG_USART, (uint8_t)ch);
    while (USART_GetFlagStatus(DEBUG_USART, USART_FLAG_TXE) == RESET);
    return ch;
}

/* 调试串口：USART1，PA9->TX，PA10->RX */
void USART1_Init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;            // PA9 -> TX（复用推挽）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;           // PA10 -> RX（浮空输入）
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);
}

/* WiFi 模块串口：USART2，PA2->TX，PA3->RX，开启接收中断 */
void USART2_Init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;            // PA2 -> TX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;            // PA3 -> RX
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baud;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);      // 开启接收中断

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

/* 发送以 '\0' 结尾的字符串 */
void USART_SendString(USART_TypeDef* USARTx, const char* str)
{
    while (*str)
    {
        USART_SendData(USARTx, (uint8_t)(*str++));
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TXE) == RESET);
    }
}

/* 发送指定长度的数据（用于 CIPSEND 发送正文） */
void WIFI_SendData(const char* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
    {
        USART_SendData(WIFI_USART, (uint8_t)data[i]);
        while (USART_GetFlagStatus(WIFI_USART, USART_FLAG_TXE) == RESET);
    }
}

uint16_t WIFI_RecvAvailable(void)
{
    return (s_rx_head - s_rx_tail + RX_BUF_SIZE) % RX_BUF_SIZE;
}

uint8_t WIFI_RecvByte(void)
{
    if (s_rx_head == s_rx_tail) return 0;
    uint8_t ch = s_rx_buf[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1) % RX_BUF_SIZE;
    return ch;
}

void WIFI_RecvFlush(void)
{
    s_rx_tail = s_rx_head;
}

/* 在 stm32f10x_it.c 的 USART2_IRQHandler 中调用 */
void WIFI_UART_IRQ(void)
{
    if (USART_GetITStatus(WIFI_USART, USART_IT_RXNE) != RESET)
    {
        uint8_t ch = USART_ReceiveData(WIFI_USART);
        uint16_t next = (s_rx_head + 1) % RX_BUF_SIZE;
        if (next != s_rx_tail)              // 缓冲未满才写入，防止覆盖
        {
            s_rx_buf[s_rx_head] = ch;
            s_rx_head = next;
        }
        USART_ClearITPendingBit(WIFI_USART, USART_IT_RXNE);
    }
}
