#include "dht11.h"
#include "delay.h"

/* ---------- 底层 GPIO 切换 ---------- */
static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = DHT11_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;     // 推挽输出（发起始信号）
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &gpio);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = DHT11_GPIO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IPU;        // 输入 + 内部上拉（读数据时）
    GPIO_Init(DHT11_GPIO_PORT, &gpio);
}

/* 等待引脚变为指定电平，超时返回 -1（避免死循环） */
static int WaitPin(uint8_t level, uint32_t timeout_us)
{
    uint32_t start  = DWT->CYCCNT;
    uint32_t cycles = timeout_us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < cycles)
    {
        if (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == (level ? SET : RESET))
            return 0;
    }
    return -1;
}

/* ---------- 对外接口 ---------- */
void DHT11_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DHT11_SetOutput();
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);   // 空闲高电平
    Delay_ms(100);
}

/* 读取一个字节（8 位，每位的 50us 低电平后，高电平宽度区分 0/1） */
static uint8_t DHT11_ReadByte(void)
{
    uint8_t data = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        WaitPin(1, 100);            // 等该位起始的 50us 低电平结束（变高）
        Delay_us(40);               // 采样点：0 位高约 26us，1 位高约 70us
        data <<= 1;
        if (GPIO_ReadInputDataBit(DHT11_GPIO_PORT, DHT11_GPIO_PIN) == SET)
            data |= 1;
        WaitPin(0, 100);            // 等该位高电平结束，进入下一位的低电平
    }
    return data;
}

int DHT11_Read(int8_t *temp, int8_t *humi)
{
    uint8_t buf[5] = {0};

    /* 1. 主机拉低 ≥18ms 作为起始信号，再释放 */
    DHT11_SetOutput();
    GPIO_ResetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);
    Delay_ms(20);
    GPIO_SetBits(DHT11_GPIO_PORT, DHT11_GPIO_PIN);
    DHT11_SetInput();
    Delay_us(20);

    /* 2. 等待模块响应：拉低 80us → 拉高 80us → 进入数据 */
    if (WaitPin(0, 100) != 0) return -1;   // 等模块拉低（响应开始）
    if (WaitPin(1, 100) != 0) return -1;   // 等 80us 低结束
    if (WaitPin(0, 100) != 0) return -1;   // 等 80us 高结束，进入首数据位低电平

    /* 3. 读 40 位：湿度整数、湿度小数、温度整数、温度小数、校验和 */
    for (uint8_t i = 0; i < 5; i++)
        buf[i] = DHT11_ReadByte();

    /* 4. 校验：前四字节之和低 8 位应等于校验和 */
    if ((uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
        return -2;

    *humi = (int8_t)buf[0];
    *temp = (int8_t)buf[2];
    return 0;
}
