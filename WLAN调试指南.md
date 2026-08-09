# STM32F103 + ATK-MW8266D WiFi 调试指南

> 本工程基于 **STM32F103C8（标准外设库 StdPeriph）** + **正点原子 ATK-MW8266D V1.4** 模块。
> 模块通过**串口 AT 指令**（LVTTL，非 SPI）与 STM32 通信，默认波特率 **115200 (8N1)**。
> 文档记录「接线 → 工程配置 → 编译下载 → 排障」的完整跑通流程，供后续参考。

---

## 1. 硬件接线

ATK-MW8266D 是 UART-WiFi 模块，**两侧都是 3.3V 逻辑，与 STM32 直接交叉连接即可，无需电平转换**。
板载 LDO，VCC 兼容 **3.3V~5V**。

| STM32F103C8 | ATK-MW8266D | 说明 |
|---|---|---|
| PA2 (USART2_TX) | RXD | 交叉连接：STM32 发 → 模块收 |
| PA3 (USART2_RX) | TXD | 交叉连接：模块发 → STM32 收 |
| 3.3V 或 5V | VCC | 板载 LDO，3.3V/5V 均可 |
| GND | GND | **必须共地** |

### 引脚说明（修正记录）
- **RST**：低电平有效复位，内部已上拉（默认高 = 不复位）。正常使用**可不接**；想用 MCU 控制复位时接一个 GPIO，平时高、拉低 ≥100ms 再拉高。
- **IO_0**：高 = 运行模式（默认），低 = 固件烧录模式，内部已上拉。**正常使用可不接**。
- ⚠️ RST、IO_0 **切勿直接接地**：RST 接地 = 一直复位；IO_0 接地 = 一直进烧录模式，都不跑 AT。
- ⚠️ 若你的具体板子**没有内部上拉**，RST/IO_0 各自加一个 **10k 电阻上拉到 3.3V**。
- ⚠️ 供电：ESP8266 峰值电流约 300mA。若核心板的 3.3V 稳压（如 AMS1117）带不动，模块会反复异常（如 `busy p...`），建议模块单独供 5V 或用外部 3.3V。

---

## 2. 工程文件结构

驱动代码都在 `User/` 目录下，已注册进 Keil 的 User 组：

| 文件 | 作用 |
|---|---|
| `delay.c / delay.h` | SysTick 1ms 时基，供 AT 指令超时判断 |
| `usart.c / usart.h` | USART1(PA9/PA10) 调试输出（`printf` 重定向）；USART2(PA2/PA3) 接模块，开 RXNE 中断 + 512B 环形缓冲 |
| `esp8266.c / esp8266.h` | `ESP8266_Init / JoinAP / ConnectTCP / SendTCP / HttpGet` 等 AT 指令封装 |
| `main.c` | 示例流程：LED 启动指示 → AT 自检 → 连 WiFi → HTTP GET |
| `stm32f10x_it.c` | 已补 `SysTick_Handler` 与 `USART2_IRQHandler` |

> **两组串口不要接混**：
> - **PA2/PA3（USART2）= 连 ESP8266**，给模块用的；
> - **PA9/PA10（USART1）= 调试输出**，接电脑 USB-TTL 看日志。

---

## 3. Keil 工程配置

1. **Device** 选 `STM32F103C8`（Options → Device）。
2. **勾选 Use MicroLIB**：Options → **Target** 选项卡 → Code Generation 区域 → 勾 `Use MicroLIB`。
   - 不勾会导致 `printf` 链接半主机模式，调试串口**完全无输出（空白）**。
3. **Debug** 选调试器（ST-Link / CMSIS-DAP），Settings 里能识别到芯片才能下载。

---

## 4. 使用步骤

1. 打开 `User/main.c`，把第 36 行的 WiFi 账号密码改成真实值（**引号保留，区分大小写**）：
   ```c
   if (ESP8266_JoinAP("你的SSID", "你的密码") == ESP8266_OK)
   ```
   - ⚠️ ESP8266 **只支持 2.4GHz**；双频/5GHz 路由器或手机热点要用 2.4G 那个 SSID。
2. **Build（F7）+ Download（F8）** 编译下载。
3. 用 USB-TTL 接 **PA9(TX1)/PA10(RX1)/GND**，串口助手选对应 COM、**115200 / 8 / N / 1 / 无流控**。
4. 给 STM32 上电或按复位，看调试串口日志。

---

## 5. 日志判读与成功标志

### 成功时依次出现
```
STM32F103 + ATK-MW8266D start     // 程序跑起来了
OK                                // 模块回 AT（STM32↔模块串口通）
[OK] module ready (AT / CWMODE)   // 初始化成功
WIFI CONNECTED
WIFI GOT IP
OK
[OK] WiFi connected               // 已连路由器并拿到 IP
> / Recv xx bytes / SEND OK       // TCP 数据发送成功
+IPD,xxx:HTTP/1.1 200 OK ...       // 收到服务器响应（能上网的铁证）
CLOSED                            // 连接正常关闭
```

### 现象对照表
| 现象 | 结论 | 处理 |
|---|---|---|
| 串口**完全空白** | 调试串口链路断 | 查 USB-TTL 接线、COM 口、波特率 115200、MicroLIB 是否勾选、程序是否下载 |
| 有 start 但无 `OK` | ESP8266 没回 AT | 查 VCC/GND、IO_0 是否悬空（高=运行）、波特率、模块是否上电 |
| `busy p...` 连续出现 | 模块卡在忙态 | **给模块断电 5 秒再上电**；确认供电电流够（300mA） |
| `AT OK` 但 WiFi 失败 | 模块活着，WiFi 没连上 | 查 SSID/密码、5GHz、信号 |
| WiFi 连上但 HTTP 无回显 | 服务器/域名问题 | 换目标或 `AT+PING="8.8.8.8"` 测外网 |

---

## 6. 已知问题与解决（实战记录）

### 6.1 `busy p...` 模块卡死
- **现象**：AT 指令模块不回 `OK`，连续打印 `busy p...`，随后 `[ERR] no response`。
- **根因**：模块进入异常忙态（未正常 boot 完、或状态乱）。
- **解决**：
  1. 给模块 **VCC 断电 5 秒** 再上电（RST/IO_0 未接 MCU 时唯一硬复位方式）；
  2. 代码侧已加容错：`ESP8266_Init()` 上电等 **3 秒** + AT **重试 5 次**，避免启动瞬间 busy 导致偶发失败。
- 实测：断电复位后同样的 WiFi 账号一次就连上，之前的 `+CWJAP:3` 是 busy 态假象。

### 6.2 `+CWJAP` 错误码速查
| 代码 | 含义 | 排查 |
|---|---|---|
| 1 | 连接超时 | 信号弱 / 离路由器远 |
| 2 | 密码错误 | 核对密码 |
| **3** | 找不到该 AP | SSID 拼错（区分大小写）/ 5GHz 不支持 / 不在范围 |
| 4 | 连接失败（其他） | 路由器 MAC 过滤等 |

### 6.3 中文编译警告 `#870-D`
- ARMCC V5 默认按本地代码页解析源文件，UTF-8 字符串字面量里的中文会报 `#870-D: invalid multibyte character sequence`。
- **处理**：`printf` 等字符串字面量里的**中文改成英文**；注释里的中文编译器会跳过、不受影响。
- 若要保留中文串口输出：Keil `Edit → Configuration → Editor → Encoding` 选 `UTF-8 with Signature` 重新保存，或在 `Options → C/C++ → Misc Controls` 加 `--locale=english` 并确保文件带 BOM。

---

## 7. 单独测试模块（定位问题用）
把 ESP8266 从 STM32 拆下，直接用 USB-TTL 接模块 **TXD/RXD/GND**（交叉 + VCC 供电），串口助手 115200 打开，手动发：
- `AT` → 回 `OK`（不通 = 模块/波特率问题）
- `AT+CWLAP` → 列出附近 WiFi（确认能搜到目标）
- `AT+CWJAP="SSID","密码"` → 回 `WIFI CONNECTED` + `OK`
- `AT+CIFSR` → 显示分配到的 IP（连上路由器的铁证）

这一步能 100% 区分「模块/接线问题」还是「STM32 程序问题」。

---

## 8. 后续可扩展方向
1. **MQTT 上报**（接阿里云/OneNET/EMQX）——物联网最实用。
2. **TCP 透传模式**（`AT+CIPMODE=1` + `AT+CIPSEND`）——持续通信不用每次带长度。
3. **GET 自己的服务器/API**——把 `httpbin.org` 换成后端。
4. **心跳 / 断线重连**——7×24 稳定运行。

---

## 9. 扩展：DHT11 温湿度 + OLED 显示 + WiFi 上传

在已调通的 WiFi 基础上，新增 **DHT11（PA0 单总线）** 与 **0.96" OLED SSD1306（PB8/PB9 软件 I2C，江协科技驱动）**，实现「采集温湿度 → OLED 显示 → POST 上传」联动。

### 9.1 新增接线
| 外设 | STM32 引脚 | 说明 |
|---|---|---|
| DHT11 DATA | PA0 | 单总线，需 4.7k 上拉到 3.3V（多数模块板载已带） |
| OLED SCL | PB8 | 软件 I2C 时钟（开漏，靠模块上拉） |
| OLED SDA | PB9 | 软件 I2C 数据 |
| OLED VCC/GND | 3.3V / GND | OLED 一般 3.3V 供电 |

> 新增文件已加入 Keil User 组：`dht11.c/.h`、`oled.c/.h`、`oled_font.h`。
> 现有引脚占用：PA0=DHT11，PA2/PA3=ESP8266，PB8/PB9=OLED(I2C)，PA9/PA10=调试串口，PC13=LED，互不冲突。
> 注意：OLED 驱动用的是 **PB8/PB9**（不是早期版本的 PB6/PB7）；若你手上的旧 `ssd1306.c/.h` 残留，已从工程中移除、可手动删除。

### 9.2 新增驱动能力
- `dht11.c`：`DHT11_Init()` / `DHT11_Read(int8_t *temp, int8_t *humi)`，整数温湿度，含起始信号、响应握手、40bit 读取与校验。
- `oled.c`（江协科技 OLED 例程）：`OLED_Init()` / `OLED_Clear()` / `OLED_ShowString(Line,Column,str)`（8x16 字体，Line 1~4、Column 1~16）/ `OLED_ShowNum()` / `OLED_ShowChar()` 等，软件 I2C 驱动 128x64；字库在 `oled_font.h` 的 `OLED_F8x16`。
- `oled_font.h`：**完整 8x16 ASCII 字库（32~126 共 95 字符，全覆盖）**，由 `gen_font.py` 用 Windows 自带 Courier New 字体自动渲染生成；取模规则与 `oled.c` 的 `OLED_ShowChar` 完全匹配（列优先、byte[0..7] 上半 / byte[8..15] 下半、bit7 为顶）。早期手填的字母字模错乱已替换。若需重生成：在 Python 环境运行 `python User/gen_font.py` 即可。
- `esp8266.c`：`ESP8266_HttpPost()`（POST JSON）、`ESP8266_ConnectTCP()`（先关旧连接、接受 OK/ALREADY CONNECTED）、`ESP8266_IsConnected()`、`ESP8266_PrintIP()`、`ESP8266_HttpGet()`。
- `delay.c` 新增 `Delay_us()`（基于 DWT 周期计数器），供 DHT11 微秒时序使用。

### 9.3 main.c 联动逻辑
启动 → LED 闪烁 → 初始化串口/ESP8266/DHT11/OLED → 连 WiFi（SSID/密码在 main.c 中配置）→ **打印模块 IP** → **每 5 秒**：读 DHT11 → OLED 显示 `TEMP:xx C` / `HUMI:xx %` → 上传前**先确认 WiFi 仍连着（掉了就自动重连）** → `ESP8266_HttpPost(SERVER_IP,SERVER_PATH,...)` 上传 `{"t":..,"h":..}` → 调试串口打印结果并回显服务器响应。

### 9.4 常见问题
- **OLED 不亮**：检查 PB8/PB9 接线、I2C 地址 0x78；若模块无板上拉，需在 SCL/SDA 各加 4.7k 上拉到 3.3V；确认 OLED 是 3.3V 版本。
- **OLED 字母乱码、数字正常**：字库 `oled_font.h` 不完整或取模规则不匹配。现版为脚本生成的完整 95 字符字库，重新编译即可；若仍乱码，检查 `oled.c` 包含的是 `oled_font.h` 且 `OLED_F8x16[ch-' ']` 索引正确。
- **DHT11 读失败（DHT11 ERR）**：检查 PA0 接线与 4.7k 上拉；DHT11 每次读取间隔需 ≥1s（代码已用 5s 循环满足）；首次上电先等 1s 再读。
- **上传失败（日志 `ERROR` 紧跟 `Temp=...` 后，`[ERR] upload failed`）**：`AT+CIPSTART` 立即回 `ERROR`，说明 TCP 建连失败，根因在「模块没真正拿到 IP / 该网络到目标服务器不通」，不是数据问题。定位步骤：
  1. 看启动时的 `IP: ...` 打印——若为空或无 IP，说明 JoinAP 没成功（密码错 / 非 2.4GHz / 信号弱）。
  2. 若日志出现 `[INFO] WiFi lost, reconnecting...` 反复，说明 WiFi 在掉线，检查路由器/距离。
  3. 用 USB-TTL 单独对模块发 `AT+CIFSR`（看 IP）、`AT+PING="8.8.8.8"`（测外网）、`AT+CWJAP?`（看是否连着 AP）确认网络层。
  4. httpbin.org 偶尔不稳定，可换成自己的服务器或 `AT+PING` 能通的其他地址。
