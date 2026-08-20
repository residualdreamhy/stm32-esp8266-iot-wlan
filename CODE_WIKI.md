# Code Wiki — STM32 WLAN 温湿度物联网项目

## 1. 项目概述

| 属性 | 说明 |
|------|------|
| **项目名称** | STM32F103 + ESP8266 + DHT11 温湿度物联网系统 |
| **MCU** | STM32F103C8T6 (BluePill), Cortex-M3 @ 72MHz |
| **编译器** | Keil MDK V5.06 (ARMCC V5) |
| **标准库** | STM32 StdPeriph V3.5.0 |
| **编程语言** | C (嵌入式固件) + Python (服务端) + HTML/JS (前端) |
| **功能定位** | 采集温湿度 → 本地 OLED/点阵显示 → WiFi 上传到云端看板 |

---

## 2. 整体架构

### 2.1 系统框图

```
┌─────────────────────────────────────────────────────────────────┐
│                         STM32F103C8T6                           │
│                                                                 │
│  ┌──────────┐  ┌──────────────┐  ┌──────────┐  ┌───────────┐   │
│  │  DHT11   │  │  ESP8266      │  │  OLED    │  │  MAX7219  │   │
│  │  Driver  │  │  Driver      │  │  Driver  │  │  Driver   │   │
│  │  (PA0)   │  │  (PA2/PA3)   │  │  (PB8/9) │  │  (PA4/5/7)│   │
│  └─────┬────┘  └──────┬───────┘  └────┬─────┘  └─────┬─────┘   │
│        │              │               │              │         │
│        └──────────────┴───────────────┴──────────────┘         │
│                               │                                 │
│                          ┌────┴─────┐                          │
│                          │  main.c  │                          │
│                          │ 主循环   │                          │
│                          └──────────┘                          │
└─────────────────────────────────────────────────────────────────┘
          │ HTTP POST JSON
          ▼
┌─────────────────────────────────────────────────────────┐
│              Python HTTP Server (port 8000)              │
│  ┌──────────────┐  ┌──────────────────┐  ┌───────────┐  │
│  │   app.py     │  │   index.html     │  │ data.csv  │  │
│  │  (零依赖)    │  │  (Chart.js)      │  │ (持久化)  │  │
│  └──────────────┘  └──────────────────┘  └───────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 2.2 数据流

```
DHT11 ──读──▶ main.c ──显示──▶ OLED (SSD1306)
                    │
                    ├──显示──▶ MAX7219 (8x8 点阵)
                    │
                    └──POST──▶ ESP8266 ──WiFi──▶ Python Server
                                                      │
                                                      ├──存──▶ data.csv
                                                      │
                                                      └──展示──▶ 浏览器看板
```

---

## 3. 文件结构

```
3-0 WLAN/
├── User/                           # 用户代码（核心开发区）
│   ├── main.c                      # 主程序：初始化 → 连 WiFi → 循环(采集→显示→上传)
│   ├── delay.c / delay.h           # 时基模块：SysTick 1ms + DWT 微秒延时
│   ├── usart.c / usart.h           # 双串口：USART1(调试) + USART2(WiFi, 环形缓冲)
│   ├── esp8266.c / esp8266.h       # ESP8266 AT 指令驱动
│   ├── dht11.c / dht11.h           # DHT11 单总线温湿度读取
│   ├── oled.c / oled.h             # OLED SSD1306 软件 I2C 驱动（主用）
│   ├── oled_font.h                 # 8x16 ASCII 字库（由 gen_font.py 生成）
│   ├── max7219.c / max7219.h       # MAX7219 8x8 LED 点阵驱动（软件 SPI）
│   ├── ssd1306.c / ssd1306.h       # SSD1306 备用驱动（PB6/PB7，未在主工程使用）
│   ├── gen_font.py                 # OLED 字库生成脚本
│   ├── stm32f10x_it.c / .h         # 中断处理：SysTick + USART2 RXNE
│   └── stm32f10x_conf.h            # StdPeriph 库功能开关配置
│
├── server/                         # Python 服务端
│   ├── app.py                      # HTTP 服务器（标准库，零依赖）
│   └── index.html                  # Chart.js 实时温湿度看板
│
├── Library/                        # STM32 StdPeriph V3.5.0 标准外设库
├── Start/                          # CMSIS 启动文件 + 系统初始化
├── Project.uvprojx                 # Keil 工程文件
├── README.md                       # 项目说明文档
└── WLAN调试指南.md                 # 调试排障指南
```

---

## 4. 模块职责详解

### 4.1 `delay` — 时基模块

| 属性 | 说明 |
|------|------|
| **文件** | [delay.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/delay.c) / [delay.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/delay.h) |
| **职责** | 提供毫秒级和微秒级阻塞延时，以及系统毫秒计数 |
| **依赖** | CMSIS Core (DWT 寄存器) |
| **被依赖** | 几乎所有其他模块都依赖此模块 |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `Delay_Init()` | 初始化 SysTick (1ms) + 使能 DWT 周期计数器 |
| `Delay_ms(ms)` | 毫秒级阻塞延时 |
| `Delay_us(us)` | 微秒级阻塞延时（DWT 实现，用于 DHT11 单总线时序） |
| `Delay_GetTick()` | 获取当前毫秒计数（用于 AT 指令超时判断） |
| `Delay_SysTickHandler()` | SysTick 中断回调，内部累加 `g_sys_tick` |

---

### 4.2 `usart` — 双串口通信模块

| 属性 | 说明 |
|------|------|
| **文件** | [usart.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/usart.c) / [usart.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/usart.h) |
| **职责** | 管理两个 USART：USART1（调试输出）和 USART2（WiFi 模块通信） |
| **依赖** | STM32 StdPeriph (GPIO, USART, NVIC) |
| **被依赖** | `esp8266.c`, `main.c` |
| **特殊设计** | 512 字节环形接收缓冲 + RXNE 中断 |

#### 串口分配

| 串口 | 引脚 | 波特率 | 用途 |
|------|------|--------|------|
| USART1 | PA9(TX) / PA10(RX) | 115200 | 调试输出 → PC |
| USART2 | PA2(TX) / PA3(RX) | 115200 | ESP8266 WiFi 模块 |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `USART1_Init(baud)` | 初始化调试串口 |
| `USART2_Init(baud)` | 初始化 WiFi 串口（开启 RXNE 中断） |
| `USART_SendString(USARTx, str)` | 发送字符串 |
| `WIFI_SendData(data, len)` | 发送指定长度数据（CIPSEND 正文） |
| `WIFI_RecvAvailable()` | 查询环形缓冲中可读字节数 |
| `WIFI_RecvByte()` | 从环形缓冲读取 1 字节 |
| `WIFI_RecvFlush()` | 清空接收缓冲 |
| `WIFI_UART_IRQ()` | USART2 中断回调（在 `stm32f10x_it.c` 中调用） |
| `fputc()` | 重定向 `printf` 到 USART1（需勾选 Keil MicroLIB） |

---

### 4.3 `esp8266` — ESP8266 WiFi 驱动

| 属性 | 说明 |
|------|------|
| **文件** | [esp8266.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/esp8266.c) / [esp8266.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/esp8266.h) |
| **职责** | 通过 AT 指令控制 ESP8266 模块：WiFi 连接、TCP 通信、HTTP 请求 |
| **依赖** | `usart.h`, `delay.h` |
| **被依赖** | `main.c` |
| **状态缓存** | 内部维护 `g_sta_ip[20]` 缓存 DHCP 分配的 IP |

#### 关键数据

```c
#define ESP8266_OK       0    // 成功
#define ESP8266_TIMEOUT -1    // 超时
```

#### 关键函数

| 函数 | 说明 | 超时 |
|------|------|------|
| `ESP8266_Init()` | AT 测试（重试 5 次）+ 关闭回显 + Station 模式 + 单连接 | — |
| `ESP8266_JoinAP(ssid, pwd)` | 连接 WiFi 热点 | 20s |
| `ESP8266_IsConnected()` | 双校验：关联状态 + IP 非 0.0.0.0 | 3s |
| `ESP8266_PrintIP()` | 查询并打印模块 IP（刷新 `g_sta_ip`） | 1s |
| `ESP8266_ConnectTCP(ip, port)` | 建立 TCP 连接（含重试） | 8s × 2 |
| `ESP8266_CloseTCP()` | 关闭 TCP 连接 | 0.5s |
| `ESP8266_SendTCP(data, len)` | 通过 CIPSEND 发送数据 | 3s |
| `ESP8266_HttpGet(host, path, port)` | 简易 HTTP GET | — |
| `ESP8266_HttpPost(host, path, port, body)` | 简易 HTTP POST（用于上传 JSON） | — |
| `ESP8266_SendCmd(cmd, expect, timeout_ms)` | 底层通用 AT 指令发送 | 自定义 |

#### AT 指令流程

```
初始化:
  AT              → OK
  ATE0            → OK          # 关闭回显
  AT+CWMODE=1     → OK          # Station 模式
  AT+CIPMUX=0     → OK          # 单连接模式

连 WiFi:
  AT+CWJAP="SSID","PASSWORD"   → WIFI CONNECTED / WIFI GOT IP / OK

上传数据:
  AT+CIPSTART="TCP","IP","PORT" → CONNECT / OK
  AT+CIPSEND=LENGTH             → >
  <HTTP 报文>                   → SEND OK
  AT+CIPCLOSE                   → CLOSED
```

---

### 4.4 `dht11` — DHT11 温湿度传感器驱动

| 属性 | 说明 |
|------|------|
| **文件** | [dht11.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/dht11.c) / [dht11.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/dht11.h) |
| **职责** | 单总线协议读取 DHT11 温湿度数据 |
| **依赖** | `delay.h`（微秒延时） |
| **被依赖** | `main.c` |
| **接线** | PA0，需外部 4.7kΩ 上拉到 3.3V |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `DHT11_Init()` | 初始化 GPIO 为推挽输出，空闲高电平 |
| `DHT11_Read(temp, humi)` | 读取温湿度，成功返回 0，失败返回负值 |

#### 返回值

| 值 | 含义 |
|----|------|
| `0` | 读取成功 |
| `-1` | 无响应（超时） |
| `-2` | 校验和错误 |

#### 通信时序

```
1. 主机拉低 PA0 ≥ 18ms → 释放 → 转为输入上拉
2. 等待 DHT11 响应: 拉低 80μs → 拉高 80μs
3. 读取 40 位数据（5 字节）:
   Byte0: 湿度整数
   Byte1: 湿度小数
   Byte2: 温度整数
   Byte3: 温度小数
   Byte4: 校验和（前四字节之和低 8 位）
```

---

### 4.5 `oled` — OLED 显示屏驱动（SSD1306）

| 属性 | 说明 |
|------|------|
| **文件** | [oled.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/oled.c) / [oled.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/oled.h) |
| **职责** | 通过软件 I2C 驱动 SSD1306 OLED（128×64），显示字符/数字/字符串 |
| **依赖** | `oled_font.h`（8×16 字库） |
| **被依赖** | `main.c` |
| **接线** | PB8(SCL) / PB9(SDA)，开漏输出 |
| **I2C 地址** | 0x78 |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `OLED_Init()` | 初始化 I2C + SSD1306 寄存器配置 + 清屏 |
| `OLED_Clear()` | 清屏 |
| `OLED_SetCursor(Y, X)` | 设置光标位置（Y: 0~7, X: 0~127） |
| `OLED_ShowChar(Line, Column, Char)` | 显示单个字符（Line: 1~4, Column: 1~16） |
| `OLED_ShowString(Line, Column, String)` | 显示字符串 |
| `OLED_ShowNum(Line, Column, Number, Length)` | 显示无符号十进制数 |
| `OLED_ShowSignedNum(Line, Column, Number, Length)` | 显示带符号数 |
| `OLED_ShowHexNum(Line, Column, Number, Length)` | 显示十六进制数 |
| `OLED_ShowBinNum(Line, Column, Number, Length)` | 显示二进制数 |

#### SSD1306 初始化序列

```
0xAE → 关闭显示
0xD5, 0x80 → 设置显示时钟分频
0xA8, 0x3F → 多路复用率 1/64
0xD3, 0x00 → 显示偏移
0x40 → 起始行 0
0xA1 → 段重映射（左右）
0xC8 → COM 扫描方向（上下）
0xDA, 0x12 → COM 引脚配置
0x81, 0xCF → 对比度
0xD9, 0xF1 → 预充电周期
0xDB, 0x30 → VCOMH 取消选择
0xA4 → 正常显示模式
0xA6 → 正常显示（非反色）
0x8D, 0x14 → 电荷泵开启
0xAF → 开启显示
```

---

### 4.6 `max7219` — LED 点阵驱动

| 属性 | 说明 |
|------|------|
| **文件** | [max7219.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/max7219.c) / [max7219.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/max7219.h) |
| **职责** | 通过软件 SPI 驱动 MAX7219 8×8 LED 点阵，显示字符/数字/滚动文本 |
| **依赖** | `delay.h` |
| **被依赖** | `main.c` |
| **接线** | PA4(CS/LOAD) / PA5(CLK) / PA7(DIN) |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `MAX7219_Init()` | 初始化：退出掉电、不译码、扫 8 行、关测试、中等亮度、清屏 |
| `MAX7219_Write(reg, data)` | 写寄存器（16 位：8 位地址 + 8 位数据） |
| `MAX7219_Clear()` | 清屏（8 行全灭） |
| `MAX7219_SetBrightness(level)` | 设置亮度（0~15） |
| `MAX7219_Test(on)` | 显示测试（全亮/正常） |
| `MAX7219_SetPixel(row, col, on)` | 点亮/熄灭单个像素 |
| `MAX7219_ShowChar(ch)` | 显示单个字符 |
| `MAX7219_ShowNum2(num)` | 显示 0~99 两位数（十位 1s + 个位 1s） |
| `MAX7219_Scroll(text, ms_per_col)` | 滚动显示字符串 |

#### MAX7219 寄存器映射

| 寄存器 | 地址 | 说明 |
|--------|------|------|
| REG_DIGIT_0 ~ 7 | 0x01 ~ 0x08 | 行数据（第 1~8 行） |
| REG_DECODE | 0x09 | 译码模式（0=不译码） |
| REG_INTENSITY | 0x0A | 亮度（0x00~0x0F） |
| REG_SCAN_LIMIT | 0x0B | 扫描行数 |
| REG_SHUTDOWN | 0x0C | 掉电控制 |
| REG_TEST | 0x0F | 显示测试模式 |

#### 字库

内置 8×8 字模，支持字符：`0-9`, `C`, `H`, `T`, `:`, `-`, ` `

---

### 4.7 `ssd1306` — OLED 备用驱动

| 属性 | 说明 |
|------|------|
| **文件** | [ssd1306.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/ssd1306.c) / [ssd1306.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/ssd1306.h) |
| **职责** | 另一套 SSD1306 驱动实现（带显存缓冲 + 6×8 字库） |
| **依赖** | `delay.h` |
| **接线** | PB6(SCL) / PB7(SDA) |
| **状态** | 备用方案，未在 `main.c` 中使用 |

#### 与 `oled.c` 的对比

| 特性 | oled.c (主用) | ssd1306.c (备用) |
|------|--------------|------------------|
| 字库 | 8×16（更清晰） | 6×8（更紧凑） |
| 显存 | 无（直接写） | 有 1024B 缓冲 |
| 字符支持 | 完整 ASCII | 完整 ASCII |
| 引脚 | PB8/PB9 | PB6/PB7 |

---

### 4.8 `stm32f10x_it` — 中断处理

| 属性 | 说明 |
|------|------|
| **文件** | [stm32f10x_it.c](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/stm32f10x_it.c) / [stm32f10x_it.h](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/stm32f10x_it.h) |
| **职责** | 中断服务函数实现 |

#### 中断清单

| 中断 | 处理函数 | 说明 |
|------|----------|------|
| NMI | `NMI_Handler()` | 非屏蔽中断（空实现） |
| HardFault | `HardFault_Handler()` | 硬件错误（死循环） |
| MemManage | `MemManage_Handler()` | 内存管理错误（死循环） |
| BusFault | `BusFault_Handler()` | 总线错误（死循环） |
| UsageFault | `UsageFault_Handler()` | 使用错误（死循环） |
| SVC | `SVC_Handler()` | 系统服务调用（空实现） |
| DebugMon | `DebugMon_Handler()` | 调试监控（空实现） |
| PendSV | `PendSV_Handler()` | 可挂起系统服务（空实现） |
| **SysTick** | `SysTick_Handler()` | 时基中断 → 调用 `Delay_SysTickHandler()` |
| **USART2** | `USART2_IRQHandler()` | WiFi 接收中断 → 调用 `WIFI_UART_IRQ()` |

---

### 4.9 `gen_font` — OLED 字库生成工具

| 属性 | 说明 |
|------|------|
| **文件** | [gen_font.py](file:///g:/Worktrae/work1uart/3-0%20WLAN/User/gen_font.py) |
| **职责** | 用 Courier New 字体生成 `oled_font.h`（95 字符，8×16 点阵） |
| **依赖** | Python 3 + Pillow |
| **输出** | `oled_font.h` |

#### 运行方式

```bash
pip install Pillow
python gen_font.py
```

---

### 4.10 `server/app.py` — Python HTTP 服务器

| 属性 | 说明 |
|------|------|
| **文件** | [app.py](file:///g:/Worktrae/work1uart/3-0%20WLAN/server/app.py) |
| **职责** | 接收 STM32 上传的温湿度数据，提供看板 API |
| **依赖** | Python 3 标准库（`http.server`, `json`, `csv`, `socket`） |
| **端口** | 8000 |

#### API 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 返回看板 HTML 页面 |
| GET | `/api/history` | 返回最近 720 个数据点（JSON 数组） |
| GET | `/api/latest` | 返回最新一条数据 |
| POST | `/api/data` | 接收 `{"t":温度, "h":湿度}` |

#### 数据处理流程

```
POST /api/data
  → 解析 JSON body
  → 添加时间戳 ts
  → 追加到内存环形缓冲（最多 720 点）
  → 追加写入 data.csv
  → 返回 {"ok": true}
```

#### 运行方式

```bash
cd server
python app.py
```

启动后自动打印本机局域网 IP（填入 `main.c` 的 `SERVER_IP`）。

---

### 4.11 `server/index.html` — 实时看板前端

| 属性 | 说明 |
|------|------|
| **文件** | [index.html](file:///g:/Worktrae/work1uart/3-0%20WLAN/server/index.html) |
| **职责** | 浏览器端实时展示温湿度曲线 |
| **依赖** | Chart.js 4.4.1（CDN 加载） |
| **刷新频率** | 每 2 秒 |

#### 功能特性

- 双 Y 轴折线图（左轴温度 °C，右轴湿度 %）
- 大数字卡片显示当前温度和湿度
- 自动显示数据点数和最后更新时间
- 深色主题（适合长时间显示）

---

## 5. 主程序流程

### 5.1 初始化阶段

```
main()
  │
  ├─ Delay_Init()                    // SysTick + DWT
  ├─ PC13 LED 闪烁 6 次               // 运行指示
  ├─ USART1_Init(115200)             // 调试串口
  ├─ USART2_Init(115200)             // WiFi 串口（开中断）
  ├─ MAX7219_Init()                  // 8x8 点阵初始化
  │   └─ 单像素诊断 (0,0) 点亮 3s
  ├─ DHT11_Init()                    // 温湿度传感器
  ├─ OLED_Init()                     // OLED 显示屏
  ├─ ESP8266_Init()                  // AT 测试 + Station 模式
  ├─ ESP8266_JoinAP()                // 连 WiFi（重试 2 次）
  ├─ ESP8266_IsConnected()          // 复检连接状态
  └─ ESP8266_PrintIP()               // 打印模块 IP
```

### 5.2 主循环流程

```
while(1):
  │
  ├─ DHT11_Read(&temp, &humi)        // 读取温湿度
  │   │
  │   ├─ 成功:
  │   │   ├─ OLED 显示 TEMP/HUMI
  │   │   ├─ MAX7219 显示温度数字
  │   │   └─ ESP8266_HttpPost()     // 上传 JSON
  │   │       ├─ 成功: ForwardWifi(2000) 等响应
  │   │       └─ 失败: CloseTCP → 重试 1 次（含重连 WiFi）
  │   │
  │   └─ 失败:
  │       └─ OLED 显示 "DHT11 ERR"
  │
  └─ Delay_ms(2000)                  // 等待 2 秒（加上传时间约 5s 周期）
```

### 5.3 错误处理策略

| 场景 | 处理方式 |
|------|----------|
| WiFi 连接失败 | 重试 2 次 + `IsConnected()` 兜底 |
| HTTP POST 失败 | `CloseTCP` 释放半开连接 → 重试 1 次 |
| 第二次重试前自动重连 WiFi | 应对掉线场景 |
| DHT11 读取失败 | OLED 显示错误信息，继续循环 |

---

## 6. 依赖关系图

```
                    ┌─────────┐
                    │  main.c │
                    └────┬────┘
           ┌─────────────┼──────────────┐
           │             │              │
    ┌──────┴──────┐     │       ┌──────┴──────┐
    │  esp8266.c  │     │       │  max7219.c  │
    └──────┬──────┘     │       └──────┬──────┘
           │            │              │
    ┌──────┴──────┐     │       ┌──────┴──────┐
    │  usart.c    │     │       │  delay.c    │
    └──────┬──────┘     │       └──────┬──────┘
           │            │              │
    ┌──────┴──────┐     │              │
    │  delay.c    │     │              │
    └─────────────┘     │              │
                        │              │
           ┌────────────┼──────────────┤
           │            │              │
    ┌──────┴──────┐     │       ┌──────┴──────┐
    │  dht11.c    │     │       │   oled.c    │
    └──────┬──────┘     │       └──────┬──────┘
           │            │              │
    ┌──────┴──────┐     │       ┌──────┴──────┐
    │  delay.c    │     │       │  oled_font  │
    └─────────────┘     │       └─────────────┘
                        │
                 ┌──────┴──────┐
                 │stm32f10x_it │
                 └──────┬──────┘
                        │
           ┌────────────┼────────────┐
           │                         │
    ┌──────┴──────┐           ┌──────┴──────┐
    │  delay.c    │           │  usart.c    │
    └─────────────┘           └─────────────┘
```

### 依赖说明

| 模块 | 依赖 | 说明 |
|------|------|------|
| `delay.c` | CMSIS DWT | 微秒延时使用 Cortex-M3 DWT 计数器 |
| `usart.c` | StdPeriph GPIO/USART/NVIC | 串口初始化和中断配置 |
| `esp8266.c` | `usart.h`, `delay.h` | AT 指令发送/接收 + 超时控制 |
| `dht11.c` | `delay.h` | DHT11 单总线时序需要微秒延时 |
| `oled.c` | `oled_font.h` | 字模数据 |
| `max7219.c` | `delay.h` | SPI 时序延时 |
| `stm32f10x_it.c` | `delay.h`, `usart.h` | 中断回调转发 |
| `main.c` | 所有 above | 统一调度 |

---

## 7. 引脚分配总表

### 7.1 GPIO 引脚分配

| 引脚 | 功能 | 方向 | 模块 |
|------|------|------|------|
| PA0 | DHT11 DATA | 双向（推挽输出/上拉输入切换） | DHT11 |
| PA2 | USART2_TX → ESP8266 RXD | 复用推挽输出 | WiFi |
| PA3 | USART2_RX ← ESP8266 TXD | 浮空输入 | WiFi |
| PA4 | MAX7219 CS/LOAD | 推挽输出 | 点阵 |
| PA5 | MAX7219 CLK | 推挽输出 | 点阵 |
| PA7 | MAX7219 DIN | 推挽输出 | 点阵 |
| PA9 | USART1_TX → USB-TTL RX | 复用推挽输出 | 调试 |
| PA10 | USART1_RX ← USB-TTL TX | 浮空输入 | 调试 |
| PB6 | SSD1306 SCL（备用） | 开漏输出 | OLED 备用 |
| PB7 | SSD1306 SDA（备用） | 开漏输出 | OLED 备用 |
| **PB8** | **OLED SCL（主用）** | **开漏输出** | OLED |
| **PB9** | **OLED SDA（主用）** | **开漏输出** | OLED |
| PC13 | 板载 LED | 推挽输出 | 指示 |

### 7.2 外设资源分配

| 外设 | 用途 | 优先级 |
|------|------|--------|
| SysTick | 1ms 系统时基 | 最高 |
| USART1 | 调试串口 | — |
| USART2 | WiFi 模块通信 | 1 (抢占) / 1 (子) |
| DWT | 微秒延时 | — |
| GPIOA | DHT11, USART2, MAX7219 | — |
| GPIOB | OLED I2C | — |
| GPIOC | 板载 LED | — |

---

## 8. 硬件接线

### 8.1 ESP8266 (ATK-MW8266D)

| STM32F103C8 | ATK-MW8266D | 说明 |
|-------------|-------------|------|
| PA2 (USART2_TX) | RXD | 交叉连接 |
| PA3 (USART2_RX) | TXD | 交叉连接 |
| 3.3V | VCC | 模块供电（板载 LDO） |
| GND | GND | 必须共地 |

### 8.2 DHT11

| STM32 | DHT11 | 说明 |
|-------|-------|------|
| PA0 | DATA | 需 4.7kΩ 上拉到 3.3V |
| 3.3V | VCC | 供电 |
| GND | GND | 共地 |

### 8.3 OLED (SSD1306)

| STM32 | OLED | 说明 |
|-------|------|------|
| PB8 | SCL | 软件 I2C 时钟 |
| PB9 | SDA | 软件 I2C 数据 |
| 3.3V | VCC | 供电 |
| GND | GND | 共地 |

### 8.4 MAX7219 (8×8 点阵)

| STM32 | MAX7219 | 说明 |
|-------|---------|------|
| PA4 | CS/LOAD | 芯片选择/数据锁存 |
| PA5 | CLK | 串行时钟 |
| PA7 | DIN | 串行数据输入 |
| 5V | VCC | 推荐 5V（3.3V 偏暗） |
| GND | GND | 共地 |

---

## 9. 编译与运行

### 9.1 固件编译（Keil）

1. 打开 `Project.uvprojx`
2. **Options → Target** 勾选 **Use MicroLIB**
3. 修改 `main.c` 中的 WiFi 和服务器配置：
   ```c
   #define SERVER_IP   "YOUR_LOCAL_IP"   // 电脑局域网 IP
   #define SERVER_PORT "8000"
   ```
   ```c
   ESP8266_JoinAP("YOUR_SSID", "YOUR_PASSWORD");
   ```
4. **Rebuild all target files**（全量重建）
5. 通过 ST-Link 下载到 STM32

### 9.2 服务器启动

```bash
cd server
python app.py
```

服务器输出示例：
```
====================================================
 温湿度本地服务器已启动
 监听端口: 8000
 本机局域网 IP（填到 STM32 的 SERVER_IP）：
   -> YOUR_LOCAL_IP
 浏览器看板: http://localhost:8000/
 STM32 上传地址: http://<本机IP>:8000/api/data
====================================================
```

### 9.3 查看数据

浏览器打开 `http://localhost:8000/` 查看实时温湿度看板。

### 9.4 调试工具

| 工具 | 用途 |
|------|------|
| ST-Link V2 | 程序下载 + SWD 调试 |
| USB-TTL 模块 | 串口调试（接 PA9/PA10） |
| 串口助手 | 115200 波特率观察调试日志 |
| `ipconfig` | 查看本机局域网 IP |

---

## 10. 关键配置参数

### 10.1 main.c 中的宏定义

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `SERVER_IP` | `"YOUR_LOCAL_IP"` | 服务器局域网 IP |
| `SERVER_PORT` | `"8000"` | 服务器端口 |
| `SERVER_PATH` | `"/api/data"` | API 路径 |
| WiFi SSID | `"YOUR_SSID"` | WiFi 名称 |
| WiFi 密码 | `"YOUR_PASSWORD"` | WiFi 密码 |
| 上传周期 | 约 5 秒 | `Delay_ms(2000)` + 上传耗时 |

### 10.2 usart.c 中的缓冲配置

| 参数 | 值 | 说明 |
|------|-----|------|
| `RX_BUF_SIZE` | 512 | WiFi 串口环形缓冲大小 |

### 10.3 server/app.py 中的配置

| 参数 | 值 | 说明 |
|------|-----|------|
| `HOST` | `"0.0.0.0"` | 监听所有网卡 |
| `PORT` | `8000` | 服务端口 |
| `MAX_POINTS` | `720` | 内存保留数据点数（约 1 小时） |

---

## 11. 调试日志判读

| 日志关键词 | 含义 | 状态 |
|-----------|------|------|
| `OK` | 模块响应正常 | ✅ |
| `WIFI CONNECTED` | 模块连上路由 | ✅ |
| `WIFI GOT IP` | DHCP 分配到 IP | ✅ |
| `[OK] WiFi connected` | 程序确认连接成功 | ✅ |
| `IP: 192.168.x.x` | 模块 IP 正常 | ✅ |
| `Temp=30C Humi=60%` | DHT11 读取成功 | ✅ |
| `[OK] uploaded (try 1)` | HTTP POST 成功 | ✅ |
| `[ERR] upload failed (try 1)` | 第一次失败，重试中 | ⚠️ |
| `busy p...` | 模块忙，需断电重启 | ❌ |
| `+CWJAP:3` | 找不到 AP | ❌ |
| `ERROR` | AT 指令出错 | ❌ |
| `[ERR] DHT11 read failed` | 传感器读取失败 | ❌ |

---

## 12. 技术栈总结

| 层次 | 技术 | 版本 |
|------|------|------|
| **MCU** | STM32F103C8T6 (Cortex-M3) | — |
| **外设库** | STM32 StdPeriph | V3.5.0 |
| **IDE** | Keil MDK | V5.06 |
| **编译器** | ARMCC | V5 |
| **WiFi** | ESP8266 (AT 指令) | ATK-MW8266D V1.4 |
| **传感器** | DHT11 | — |
| **显示 1** | SSD1306 OLED (软件 I2C) | 0.96", 128×64 |
| **显示 2** | MAX7219 8×8 LED 点阵 | — |
| **服务端** | Python `http.server` | 标准库，零依赖 |
| **前端** | Chart.js | 4.4.1 (CDN) |
| **数据存储** | CSV 文件 | `data.csv` |

---

## 13. 扩展方向

| 方向 | 说明 |
|------|------|
| **OTA 升级** | 通过 WiFi 实现远程固件更新 |
| **MQTT 协议** | 替换 HTTP，接入 IoT 平台（阿里云/OneNET） |
| **更多传感器** | 增加 BMP280（气压）、BH1750（光照）等 |
| **本地存储** | 使用 SD 卡模块记录历史数据 |
| **Web 控制台** | 增加设备远程控制和参数配置功能 |
| **报警通知** | 温度/湿度超阈值时发送邮件或微信通知 |
| **低功耗** | 加入睡眠模式，电池供电场景 |

---

## 附录 A：关键源文件索引

| 文件路径 | 说明 |
|----------|------|
| `User/main.c` | 主程序入口 |
| `User/delay.c` | 延时模块 |
| `User/usart.c` | 双串口通信 |
| `User/esp8266.c` | ESP8266 WiFi 驱动 |
| `User/dht11.c` | DHT11 传感器驱动 |
| `User/oled.c` | OLED 显示屏驱动 |
| `User/max7219.c` | MAX7219 LED 点阵驱动 |
| `User/ssd1306.c` | SSD1306 备用驱动 |
| `User/stm32f10x_it.c` | 中断处理 |
| `User/gen_font.py` | 字库生成工具 |
| `server/app.py` | Python HTTP 服务器 |
| `server/index.html` | 实时看板前端 |
| `Library/` | StdPeriph 标准外设库 |
| `Start/` | CMSIS 启动文件 |

---

## 附录 B：AT 指令速查表

| 指令 | 功能 | 响应 |
|------|------|------|
| `AT` | 测试通信 | `OK` |
| `ATE0` | 关闭回显 | `OK` |
| `ATE1` | 开启回显 | `OK` |
| `AT+CWMODE=1` | Station 模式 | `OK` |
| `AT+CWMODE=2` | AP 模式 | `OK` |
| `AT+CWJAP="ssid","pwd"` | 连接 AP | `OK` / `ERROR` |
| `AT+CWJAP?` | 查询连接状态 | `+CWJAP:...OK` |
| `AT+CIFSR` | 查询 IP | `+CIFSR:STAIP,"x.x.x.x"` |
| `AT+CIPMUX=0` | 单连接模式 | `OK` |
| `AT+CIPMUX=1` | 多连接模式 | `OK` |
| `AT+CIPSTART="TCP","ip","port"` | 建立 TCP | `CONNECT` / `ERROR` |
| `AT+CIPSEND=len` | 设置发送长度 | `>` |
| `<data>` | 发送数据 | `SEND OK` |
| `AT+CIPCLOSE` | 关闭 TCP | `CLOSED` |

---

*文档生成时间：2026-08-16*
*基于代码版本：当前仓库最新状态*
