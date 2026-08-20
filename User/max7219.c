#include "max7219.h"
#include "delay.h"
#include <string.h>   /* strlen */

/* ============================================================
   MAX7219 驱动实现（参考 MAX7219中文.pdf）
   ============================================================ */

/* ---- 8x8 字模（按行存，每行 1 字节，bit7=最左列）----
   包含：0-9, C, H, T, :, -, 空格
   字模编码：每行 8 位，1=亮 0=灭，bit7 对应最左列 */
static const uint8_t Font8x8[][8] = {
    /* '0' */ {0x3C,0x42,0x81,0x81,0x81,0x81,0x42,0x3C},
    /* '1' */ {0x18,0x38,0x18,0x18,0x18,0x18,0x18,0x7E},
    /* '2' */ {0x7C,0x82,0x02,0x04,0x08,0x10,0x20,0xFE},
    /* '3' */ {0x7C,0x82,0x02,0x3C,0x02,0x02,0x82,0x7C},
    /* '4' */ {0x04,0x0C,0x14,0x24,0x44,0xFE,0x04,0x04},
    /* '5' */ {0xFE,0x80,0x80,0xFC,0x02,0x02,0x82,0x7C},
    /* '6' */ {0x3C,0x42,0x80,0xFC,0x82,0x82,0x82,0x7C},
    /* '7' */ {0xFE,0x82,0x02,0x04,0x08,0x10,0x10,0x10},
    /* '8' */ {0x3C,0x42,0x81,0x3C,0x81,0x81,0x42,0x3C},
    /* '9' */ {0x3C,0x42,0x81,0x81,0x7D,0x01,0x02,0x3C},
    /* 'C' */ {0x3C,0x42,0x80,0x80,0x80,0x80,0x42,0x3C},
    /* 'H' */ {0x81,0x81,0x81,0xFF,0x81,0x81,0x81,0x81},
    /* 'T' */ {0xFF,0x08,0x08,0x08,0x08,0x08,0x08,0x08},
    /* ':' */ {0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00},
    /* '-' */ {0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00},
    /* ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* 字符 -> 字模指针 */
static const uint8_t* GetFont(char ch)
{
    if (ch >= '0' && ch <= '9') return Font8x8[ch - '0'];
    switch (ch)
    {
        case 'C': return Font8x8[10];
        case 'H': return Font8x8[11];
        case 'T': return Font8x8[12];
        case ':': return Font8x8[13];
        case '-': return Font8x8[14];
        case ' ': return Font8x8[15];
        default:  return Font8x8[15];   /* 未知字符显示空 */
    }
}

/* ============================================================
   底层：软件 SPI 时序（参考手册"时序图"和"管脚描述"）

   关键时序要求（MAX7219 数据手册）：
   1. 数据在 CLK 上升沿移入内部移位寄存器（手册 Pin 13 描述）
   2. DIN 数据必须在 CLK 上升沿前建立好（建议提前 ≥ 25ns）
   3. LOAD(CS) 上升沿锁存最后 16 位数据（手册 Pin 12 描述）
   4. LOAD 必须在第 16 个 CLK 上升沿之后、第 17 个之前拉高
   5. CLK 最大 10MHz，软件模拟远低于此，无需担心速度
   ============================================================ */

/* 发送一个字节（MSB 先发，对应 D15→D0 的传输顺序） */
static void MAX7219_SendByte(uint8_t b)
{
    int8_t i;
    for (i = 7; i >= 0; i--)
    {
        /* 1. CLK 拉低（准备数据） */
        GPIO_ResetBits(MAX_CLK_PORT, MAX_CLK_PIN);
        /* 2. 设置 DIN 数据位（在上升沿之前建立） */
        if (b & (1 << i))
            GPIO_SetBits(MAX_DIN_PORT, MAX_DIN_PIN);
        else
            GPIO_ResetBits(MAX_DIN_PORT, MAX_DIN_PIN);
        /* 微小延时确保数据建立时间（手册要求 ≥25ns，GPIO 操作已远超） */
        Delay_us(1);
        /* 3. CLK 上升沿——数据移入移位寄存器 */
        GPIO_SetBits(MAX_CLK_PORT, MAX_CLK_PIN);
        Delay_us(1);
    }
}

/* ============================================================
   写一个寄存器（16 位传输：8 位地址 + 8 位数据）

   数据格式（手册表 1）：
     D15~D12 = ×（无效）
     D11~D8  = 寄存器地址
     D7~D0   = 数据

   传输顺序：先发高字节（含地址），再发低字节（数据），MSB 先
   LOAD(CS) 时序：拉低→发 16 位→拉高锁存
   ============================================================ */
void MAX7219_Write(uint8_t reg, uint8_t data)
{
    GPIO_ResetBits(MAX_CS_PORT, MAX_CS_PIN);    /* LOAD 拉低，开始传输 */
    Delay_us(2);                                /* CS 建立时间 */
    MAX7219_SendByte(reg);                      /* 高 8 位：D15~D8（含 4 位地址） */
    MAX7219_SendByte(data);                     /* 低 8 位：D7~D0（数据） */
    Delay_us(2);                                /* CS 保持时间 */
    GPIO_SetBits(MAX_CS_PORT, MAX_CS_PIN);      /* LOAD 上升沿，锁存 16 位数据 */
    Delay_us(2);                                /* 写间隔，防止连续写太快 */
}

/* ============================================================
   初始化（参考手册"初始状态"和各寄存器说明）

   上电后 MAX7219 默认进入掉电模式，显示器熄灭。
   需要依次配置：退出掉电→不译码→扫描 8 行→关测试→设亮度→清屏
   ============================================================ */
void MAX7219_Init(void)
{
    GPIO_InitTypeDef gi;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    gi.GPIO_Mode  = GPIO_Mode_Out_PP;
    gi.GPIO_Speed = GPIO_Speed_50MHz;
    gi.GPIO_Pin   = MAX_DIN_PIN | MAX_CLK_PIN | MAX_CS_PIN;
    GPIO_Init(GPIOA, &gi);

    /* 引脚初始状态：CS 高（空闲）、CLK 低、DIN 低 */
    GPIO_SetBits(MAX_CS_PORT, MAX_CS_PIN);
    GPIO_ResetBits(MAX_CLK_PORT, MAX_CLK_PIN);
    GPIO_ResetBits(MAX_DIN_PORT, MAX_DIN_PIN);

    Delay_ms(50);   /* 等模块上电稳定 */

    /* 寄存器配置（顺序按手册推荐） */
    MAX7219_Write(REG_SHUTDOWN, 0x01);     /* 退出掉电模式，正常工作（手册：0x01=正常） */
    MAX7219_Write(REG_DECODE, 0x00);       /* 不译码，直接控制每个 LED（8x8 矩阵必须不译码） */
    MAX7219_Write(REG_SCAN_LIMIT, 0x07);   /* 扫描全部 8 行（手册：0x07=8 位） */
    MAX7219_Write(REG_TEST, 0x00);         /* 关闭显示测试（手册：0x00=正常模式） */
    MAX7219_Write(REG_INTENSITY, 0x08);    /* 中等亮度（0x00~0x0F，可调） */
    MAX7219_Clear();                       /* 清屏 */
}

/* 清屏：8 行全灭 */
void MAX7219_Clear(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
        MAX7219_Write(REG_DIGIT_0 + i, 0x00);
}

/* 设置亮度（0~15） */
void MAX7219_SetBrightness(uint8_t level)
{
    if (level > 15) level = 15;
    MAX7219_Write(REG_INTENSITY, level);
}

/* 显示测试：1=全亮，0=正常（手册：Display Test Register）
   ⚠️ 部分国产兼容芯片（如 HMW）Test(0) 关不掉测试模式，慎用！ */
void MAX7219_Test(uint8_t on)
{
    MAX7219_Write(REG_TEST, on ? 0x01 : 0x00);
}

/* ============================================================
   点亮/熄灭单个像素（用于诊断 SPI 是否通）

   MAX7219 8x8 矩阵映射：
     行 = Digit 寄存器（REG_DIGIT_0 ~ REG_DIGIT_7，即 0x01~0x08）
     列 = 数据位（bit0=列0, bit7=列7）
   ============================================================ */
void MAX7219_SetPixel(uint8_t row, uint8_t col, uint8_t on)
{
    /* 读回当前行数据 → 修改指定位 → 写回
       但 MAX7219 没有读寄存器功能，所以维护一个显存 */
    static uint8_t vram[8] = {0,0,0,0,0,0,0,0};
    if (row > 7 || col > 7) return;
    if (on)
        vram[row] |=  (1 << col);
    else
        vram[row] &= ~(1 << col);
    MAX7219_Write(REG_DIGIT_0 + row, vram[row]);
}

/* 显示单个字符 */
void MAX7219_ShowChar(char ch)
{
    const uint8_t* font = GetFont(ch);
    uint8_t row;
    /* 直接按行写入：每行的 8 位对应 8 列
       bit7=最左列，所以数据不需要转换 */
    for (row = 0; row < 8; row++)
        MAX7219_Write(REG_DIGIT_0 + row, font[row]);
}

/* 显示两位数 0~99（先十位停 1 秒，再个位停 1 秒） */
void MAX7219_ShowNum2(int8_t num)
{
    if (num < 0) num = 0;
    if (num > 99) num = 99;
    MAX7219_ShowChar((char)('0' + num / 10));
    Delay_ms(1000);
    MAX7219_ShowChar((char)('0' + num % 10));
    Delay_ms(1000);
}

/* 滚动显示字符串（阻塞，滚完一整遍后返回）
   ms_per_col = 每移动一列的停留毫秒（建议 120~200）
   原理：把字符串转成列数据（每列 8 位=8 行），逐列左移显示 */
void MAX7219_Scroll(const char* text, uint16_t ms_per_col)
{
    #define MX_SCROLL_MAX 32
    /* 列缓冲：每列 1 字节，bit N = 第 N 行（1=亮） */
    static uint8_t cols[(MX_SCROLL_MAX + 2) * 8];
    int len, total_cols, idx, c, col, row, off;
    const uint8_t* font;

    len = strlen(text);
    if (len > MX_SCROLL_MAX) len = MX_SCROLL_MAX;
    total_cols = (len + 2) * 8;   /* 首尾各加 8 列空格 */

    idx = 0;
    /* 前导空格（8 列全灭） */
    for (col = 0; col < 8; col++) cols[idx++] = 0;
    /* 每个字符：从行字模提取 8 列数据 */
    for (c = 0; c < len; c++)
    {
        font = GetFont(text[c]);
        for (col = 0; col < 8; col++)
        {
            uint8_t col_data = 0;
            for (row = 0; row < 8; row++)
            {
                /* font[row] 的 bit(7-col) 对应该列该行 */
                if (font[row] & (0x80 >> col))
                    col_data |= (1 << row);
            }
            cols[idx++] = col_data;
        }
    }
    /* 尾随空格（8 列全灭） */
    for (col = 0; col < 8; col++) cols[idx++] = 0;

    /* 逐列左移滚动：取 8 列 → 转置成 8 行 → 写入 digit 寄存器 */
    for (off = 0; off <= total_cols - 8; off++)
    {
        uint8_t row_data[8] = {0,0,0,0,0,0,0,0};
        for (col = 0; col < 8; col++)
        {
            uint8_t cd = cols[off + col];
            for (row = 0; row < 8; row++)
            {
                if (cd & (1 << row))
                    row_data[row] |= (0x80 >> col);  /* bit7=最左列 */
            }
        }
        for (row = 0; row < 8; row++)
            MAX7219_Write(REG_DIGIT_0 + row, row_data[row]);
        Delay_ms(ms_per_col);
    }
}
