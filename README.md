# STM32 + ESP8266 + DHT11 + OLED 温湿度物联网项目

> STM32F103C8 通过 USART 驱动 ATK-MW8266D（ESP8266）WiFi 模块，每 5 秒采集 DHT11 温湿度，OLED 本地显示，同时 POST JSON 上传到自建本地服务器看板。

## 硬件清单

| 部件 | 型号 | 说明 |
|---|---|---|
| 主控 | STM32F103C8（BluePill） | StdPeriph 标准外设库，Keil V5.06 编译 |
| WiFi 模块 | ATK-MW8266D V1.4 | ESP8266，串口 AT 指令通信（非 SPI） |
| 温湿度 | DHT11 | 单总线，精度 ±1°C / ±5%RH |
| 显示 | 0.96" OLED 128×64 | SSD1306，I2C 地址 0x78 |
| 调试 | USB-TTL 串口工具 | 接 PA9/PA10，115200 8N1 |

## 接线说明

### 引脚分配总表

```
STM32F103C8 引脚分配
┌──────────┬────────────┬──────────────────────┐
│ STM32    │ 外设        │ 说明                  │
├──────────┼────────────┼──────────────────────┤
│ PA2 (TX) │ ESP8266 RXD│ USART2 → WiFi 模块    │
│ PA3 (RX) │ ESP8266 TXD│ WiFi 模块 → USART2    │
│ PA9 (TX) │ USB-TTL RX │ USART1 调试输出       │
│ PA10(RX) │ USB-TTL TX │ USART1 调试输入       │
│ PA0      │ DHT11 DATA │ 单总线，需 4.7k 上拉  │
│ PB8      │ OLED SCL   │ 软件 I2C 时钟         │
│ PB9      │ OLED SDA   │ 软件 I2C 数据         │
│ PC13     │ 板载 LED   │ 运行指示              │
│ 3.3V     │ ESP8266 VCC│ 模块供电              │
│ GND      │ 共地       │ 所有设备共地           │
└──────────┴────────────┴──────────────────────┘
```

### ESP8266 (ATK-MW8266D) 接线

| STM32F103C8 | ATK-MW8266D | 说明 |
|---|---|---|
| PA2 (USART2_TX) | RXD | 交叉连接：STM32 发 → 模块收 |
| PA3 (USART2_RX) | TXD | 交叉连接：模块发 → STM32 收 |
| 3.3V 或 5V | VCC | 板载 LDO，3.3V/5V 均可 |
| GND | GND | 必须共地 |

> RST / IO_0 内部已上拉，正常使用可不接。切勿直接接地。

### DHT11 接线

| STM32 | DHT11 | 说明 |
|---|---|---|
| PA0 | DATA | 单总线，需 4.7k 上拉到 3.3V |
| 3.3V | VCC | 供电 |
| GND | GND | 共地 |

### OLED 接线

| STM32 | OLED | 说明 |
|---|---|---|
| PB8 | SCL | 软件 I2C 时钟（开漏，靠模块上拉） |
| PB9 | SDA | 软件 I2C 数据 |
| 3.3V | VCC | OLED 3.3V 供电 |
| GND | GND | 共地 |

## 软件架构

### 系统框图

```
┌─────────────────────────────────────────────────────┐
│                    STM32F103C8                       │
│                                                      │
│  ┌─────────┐  ┌──────────┐  ┌─────────┐  ┌────────┐ │
│  │ DHT11   │  │ ESP8266  │  │  OLED   │  │  LED   │ │
│  │ Driver  │  │  Driver  │  │ Driver  │  │ (PC13) │ │
│  │ (PA0)   │  │ (PA2/PA3)│  │(PB8/PB9)│  │        │ │
│  └────┬────┘  └────┬─────┘  └────┬────┘  └───┬────┘ │
│       │            │              │            │      │
│       └────────────┴──────────────┴────────────┘      │
│                         │                             │
│                    ┌────┴────┐                        │
│                    │ main.c  │                        │
│                    │ 主循环  │                        │
│                    └─────────┘                        │
└─────────────────────────────────────────────────────┘
         │ HTTP POST JSON
         ▼
┌─────────────────────────────────┐
│     Python HTTP Server (:8000)   │
│  ┌──────────┐  ┌──────────────┐ │
│  │ app.py   │  │  index.html  │ │
│  │ (零依赖) │  │  (Chart.js)  │ │
│  └────┬─────┘  └──────────────┘ │
│       │                          │
│  ┌────┴─────┐                   │
│  │ data.csv │                   │
│  │ (持久化) │                   │
│  └──────────┘                   │
└─────────────────────────────────┘
```

### 主程序流程

```
上电
 │
 ├─ PC13 LED 闪烁（确认程序运行）
 ├─ ESP8266_Init()          # AT 测试 ×5 + CWMODE=1 + CIPMUX=0
 ├─ 连接 WiFi（重试 2 次 + IsConnected 复检）
 ├─ ESP8266_PrintIP()       # 打印模块 IP
 │
 └─ 主循环（每 5 秒）
     ├─ DHT11_Read()        # 读取温湿度
     ├─ OLED 显示 TEMP/HUMI
     ├─ 自检 WiFi 连接状态
     ├─ ESP8266_HttpPost()  # POST JSON 到本地服务器
     │   ├─ 失败 → CIPCLOSE 关半开 socket → 重试 1 次
     │   └─ 成功 → 打印 [OK] uploaded
     └─ 转发 WiFi 回显到调试串口
```

### 文件结构

```
3-0 WLAN/
├── User/
│   ├── main.c              # 主程序：初始化→连WiFi→循环(采集→显示→上传)
│   ├── delay.c/.h          # SysTick 1ms 时基 + Delay_us（DWT微秒延时）
│   ├── usart.c/.h          # USART1(调试) + USART2(WiFi, 512B环形缓冲+RXNE中断)
│   ├── esp8266.c/.h        # ESP8266 AT 指令驱动
│   ├── dht11.c/.h          # DHT11 单总线温湿度读取
│   ├── oled.c/.h           # OLED SSD1306 软件 I2C 驱动（江协科技版）
│   ├── oled_font.h         # 8x16 ASCII 字库（95 字符，gen_font.py 生成）
│   ├── ssd1306.c/.h        # SSD1306 备用驱动（PB6/PB7，未在主工程使用）
│   ├── gen_font.py         # OLED 字库生成脚本（需 Pillow）
│   ├── stm32f10x_it.c/.h   # 中断：SysTick_Handler + USART2_IRQHandler
│   └── stm32f10x_conf.h    # StdPeriph 库配置
├── server/
│   ├── app.py              # Python 标准库 HTTP 服务器（零依赖）
│   └── index.html          # Chart.js 双轴温湿度看板
├── docs/
│   ├── STM32-ESP8266-温湿度物联网项目笔记.md
│   └── ESP8266开发踩坑笔记.md
├── Library/                # StdPeriph 标准外设库
├── Start/                  # 启动文件 + CMSIS
├── Project.uvprojx         # Keil 工程文件
└── WLAN调试指南.md          # 调试排障指南
```

## 关键配置

| 配置项 | 值 | 位置 |
|---|---|---|
| WiFi SSID | `TP-LINK_64BD` | main.c |
| WiFi 密码 | `88888888` | main.c |
| 服务器 IP | `192.168.0.105` | main.c `#define SERVER_IP` |
| 服务器端口 | `8000` | main.c `#define SERVER_PORT` |
| 服务器路径 | `/api/data` | main.c `#define SERVER_PATH` |
| 上传间隔 | 5 秒 | main.c `Delay_ms(5000)` |
| 调试波特率 | 115200 | usart.c |
| ESP8266 波特率 | 115200 | usart.c |

> SERVER_IP 必须是电脑真实局域网 IP（`ipconfig` 查看），不能用 localhost/127.0.0.1。若开了 VPN 先关掉。

## ESP8266 驱动 API

| 函数 | 说明 | 关键超时 |
|---|---|---|
| `ESP8266_Init()` | 初始化模块（AT/CWMODE/CIPMUX） | AT 重试 5 次 |
| `ESP8266_JoinAP(ssid, pwd)` | 连接路由器 | 20 秒 |
| `ESP8266_IsConnected()` | 查询是否已连 AP | 2 秒 |
| `ESP8266_PrintIP()` | 打印模块 IP（AT+CIFSR） | 1 秒 |
| `ESP8266_ConnectTCP(ip, port)` | 建立 TCP 连接 | 8 秒 × 2 重试 |
| `ESP8266_SendTCP(data, len)` | 发送数据（CIPSEND） | 2 秒等 `>` |
| `ESP8266_HttpPost(host, path, port, body)` | POST JSON | 含 ConnectTCP + SendTCP |
| `ESP8266_HttpGet(host, path, port)` | HTTP GET | 含 ConnectTCP + SendTCP |
| `ESP8266_CloseTCP()` | 关闭 TCP 连接 | 2 秒 |

### AT 指令流程

```
AT              → OK
AT+CWMODE=1     → OK          # Station 模式
AT+CIPMUX=0     → OK          # 单连接模式
AT+CWJAP="SSID","PASSWORD"    # 连 WiFi（最慢 20 秒）
                 → WIFI CONNECTED / WIFI GOT IP / OK

# 每次上传：
AT+CIPSTART="TCP","192.168.0.105","8000"  → CONNECT / OK
AT+CIPSEND=128                            → >
<HTTP POST 报文>                           → SEND OK
AT+CIPCLOSE                               → CLOSED
```

## 本地服务器看板

### 服务器端（server/app.py）

- 零依赖：纯 Python 标准库 `http.server`，不用 Flask
- 接口：
  - `POST /api/data` — 收 `{"t":30,"h":60}`，返回 `{"ok":true}`
  - `GET /api/history` — 返回最近 720 个数据点
  - `GET /api/latest` — 返回最新一条
  - `GET /` — 返回看板页面
- 存储：内存环形缓冲（720 点）+ 追加写 `data.csv`
- 启动：`python app.py`，自动打印本机局域网 IP

### 前端看板（server/index.html）

- Chart.js 走 CDN（只在浏览器加载，不落盘）
- 双 Y 轴折线（左轴温度 / 右轴湿度）
- 大数字卡片显示当前值
- 每 2 秒自动刷新

## 快速开始

### 1. 编译下载固件

1. 用 Keil 打开 `Project.uvprojx`
2. Options → Target → 勾选 **Use MicroLIB**（否则 printf 无输出）
3. 修改 `main.c` 中的 WiFi SSID/密码和 `SERVER_IP`
4. **Rebuild all target files**（全量重建）
5. 下载到 STM32

### 2. 启动本地服务器

```bash
cd server
python app.py
```

服务器会自动打印局域网 IP，将该 IP 填入 `main.c` 的 `SERVER_IP`。

### 3. 查看数据

浏览器打开 `http://localhost:8000/` 即可看到实时温湿度曲线看板。

## Keil 工程配置要点

| 配置项 | 值 | 位置 |
|---|---|---|
| Use MicroLIB | 勾选 | Options → Target |
| 编译器 | ARMCC V5.06 | 默认 |
| Device | STM32F103C8 | Options → Device |
| 全量重建 | Project → Rebuild | 改完头文件后必须全量重建 |

## 调试串口日志判读

| 日志关键词 | 含义 | 是否正常 |
|---|---|---|
| `OK` | 模块响应正常 | ✅ |
| `WIFI CONNECTED` / `WIFI GOT IP` | 模块连上路由并拿到 IP | ✅ |
| `[OK] WiFi connected` | 程序确认 WiFi 就绪 | ✅ |
| `IP: 192.168.x.x` | 模块自身 IP | ✅ |
| `Temp=30C Humi=60%` | DHT11 读取成功 | ✅ |
| `[OK] uploaded (try 1)` | POST 成功 | ✅ |
| `[ERR] upload failed (try 1)` | 第一次失败，正在重试 | ⚠️ 看 try 2 |
| `busy p...` | 模块忙，断电重启 | ❌ |
| `+CWJAP:3` | 找不到 AP | ❌ 检查 SSID/频段 |
| `ERROR` | AT 指令出错 | ❌ |

## 技术文档

- [WLAN 调试指南](WLAN调试指南.md) — 接线、工程配置、编译下载、排障全流程
- [项目笔记](docs/STM32-ESP8266-温湿度物联网项目笔记.md) — 硬件清单、引脚分配、软件架构、API 文档
- [ESP8266 踩坑笔记](docs/ESP8266开发踩坑笔记.md) — 14 个实战踩坑记录与解决方案

## 技术栈

- **MCU**: STM32F103C8（Cortex-M3, 72MHz）
- **库**: StdPeriph V3.5.0 标准外设库
- **IDE**: Keil MDK V5.06（ARMCC V5）
- **WiFi**: ESP8266 AT 指令驱动
- **传感器**: DHT11 单总线温湿度
- **显示**: SSD1306 OLED（软件 I2C）
- **服务器**: Python 标准库 http.server（零依赖）
- **前端**: Chart.js 双轴折线图

## 许可

STM32 StdPeriph 库文件遵循 STMicroelectronics 许可。项目自有代码可自由使用。
