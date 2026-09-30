# CH585D 自制开发板综合硬件测试固件

基于 **沁恒微 (WCH) CH585D** 无线 MCU 的自制开发板硬件验证固件。
固件在一套代码中同时点亮并自检 **BLE 射频与天线、USB 全速/高速双路 CDC 虚拟串口、
GPIO 方波与电平测试、LED 心跳指示、UART 调试打印**，并通过串口控制台提供交互式诊断指令。

> 本仓库只包含自研工程 `CH585D/`。沁恒官方 **CH585 评估板例程包** 可保留在本地
> `EXAM/` 目录作为开发参考，该目录已在 `.gitignore` 中排除，**不纳入本仓库**。

---

## 目录

- [功能特性](#功能特性)
- [硬件资源与引脚分配](#硬件资源与引脚分配)
- [系统参数](#系统参数)
- [目录结构](#目录结构)
- [开发环境](#开发环境)
- [编译方法](#编译方法)
- [烧录方法](#烧录方法)
- [串口控制台指令](#串口控制台指令)
- [注意事项与已知问题](#注意事项与已知问题)
- [第三方代码声明](#第三方代码声明)
- [License](#license)

---

## 功能特性

| # | 功能模块 | 说明 |
|---|----------|------|
| 1 | **BLE 射频与天线** | 广播名称 `CH585D_Test`，广播周期 100 ms，发射功率 0 dBm，可扫描（Scan Response） |
| 2 | **USB_FS 全速串口** | PB10 / PB11 上的 CDC 虚拟串口，支持字符回显与控制台指令 |
| 3 | **USB_HS 高速串口** | PB12 / PB13 上的 480 Mbps CDC 虚拟串口，支持字符回显与控制台指令 |
| 4 | **LED 心跳灯** | PB15，1 Hz 闪烁（500 ms 翻转） |
| 5 | **GPIO 测试** | PA9、PA12、PA13、PA14、PA15、PB14、PB22 输出 2 Hz 方波 / 上拉输入读取 / 全部置高低电平 |
| 6 | **UART 调试打印** | UART1，TX = PA9 / RX = PA8，波特率 115200，打印启动自检信息 |

上电后固件自动完成全部子系统初始化，并打印：

```
====================================================
    CH585D Custom Board Hardware Verification Firm
    System Clock: 62 MHz
====================================================
[INIT] LED (PB15) & Test GPIOs initialized.
[INIT] Initializing BLE Stack & Broadcaster...
[INIT] Initializing USB_FS (Full-Speed CDC on PB10/11)...
[INIT] Initializing USB_HS (High-Speed CDC on PB12/13)...

All subsystems initialized successfully!
--> Look for 'CH585D_Test' in your phone's BLE scanner.
--> Connect USB_FS or USB_HS to PC and open COM port at any baudrate.
--> PB15 LED should blink at 1Hz (heartbeat).
--> Test GPIOs are outputting 2Hz square waves (0V <-> 3.3V).
```

---

## 硬件资源与引脚分配

| 功能 | 引脚 | 方向 / 模式 | 备注 |
|------|------|-------------|------|
| 系统心跳 LED | **PB15** | 推挽输出 5 mA | 1 Hz 闪烁，可用控制台指令改为常亮/常灭 |
| USB_FS D− / D+ | **PB10 / PB11** | 复用 USB | 全速 USB CDC 虚拟串口 |
| USB_HS D− / D+ | **PB12 / PB13** | 复用 USB | 高速 480 Mbps USB CDC 虚拟串口 |
| 调试串口 UART1 | **PA9 (TX) / PA8 (RX)** | 复用 UART | 115200 bps，8N1 |
| 测试 GPIO | **PA9, PA12, PA13, PA14, PA15, PB14, PB22** | 推挽输出 / 上拉输入 | 默认输出 2 Hz 方波（0 V ↔ 3.3 V） |

> ⚠️ **引脚复用提醒**：`PA9` 同时被 UART1 的 TX 与测试 GPIO 列表占用。
> 若需使用 UART1 打印调试信息，请从测试 GPIO 中剔除 PA9，或改用其他引脚。
> 该冲突不影响 USB / BLE 功能，但会使 PA9 上的方波波形被串口数据干扰。
>
> 注意 `PA14`（端口 A 的 bit14）与 `PB14`（端口 B 的 bit14）是两个**不同**的物理引脚，均已被占用。

---

## 系统参数

| 项目 | 取值 | 来源 |
|------|------|------|
| 芯片 | CH585D (RISC-V QingKe V3C) | `CONFIG.h` |
| 系统主频 | **62.4 MHz**（HSE 32 MHz + PLL） | `CLK_SOURCE_HSE_PLL_62_4MHz` |
| 外部晶振 | 32 MHz HSE，负载电容 18 pF | `HSECFG_Capacitance(HSECap_18p)` |
| Flash | 448 KB @ `0x00000000` | `Ld/Link.ld` |
| RAM | 128 KB @ `0x20000000` | `Ld/Link.ld` |
| BLE 协议栈堆 | 6 KB | `BLE_MEMHEAP_SIZE` |
| BLE 广播间隔 | 100 ms (160 × 0.625 ms) | `ble_dev.c` |
| BLE 发射功率 | 0 dBm | `BLE_TX_POWER` |
| BLE MAC | 使用芯片出厂 MAC（未启用自定义） | `BLE_MAC = FALSE` |
| RTC 时钟源 | 内部 32 kHz | `CLK_OSC32K = 1` |
| DCDC / 低功耗休眠 | 均未启用 | `DCDC_ENABLE = FALSE`, `HAL_SLEEP = FALSE` |

> 说明：`Main.c` 中预留了自定义 BLE MAC 地址常量 `MacAddr`，但 `BLE_MAC` 默认为 `FALSE`，
> 因此编译时不会生效，芯片使用出厂 MAC 地址。

---

## 目录结构

```
.
├── CH585D/                     # ★ 自制开发板测试固件（主工程）
│   ├── src/                    # 应用层源码（UTF-8 编码）
│   │   ├── Main.c              # 主入口：时钟、外设初始化与主循环调度
│   │   ├── ble_dev.c/.h        # BLE 协议栈初始化与广播
│   │   ├── board_test.c/.h     # LED 心跳、GPIO 方波/电平、控制台指令解析
│   │   ├── usb_fs_dev.c/.h     # 全速 USB CDC
│   │   ├── usb_hs_cdc.c/.h     # 高速 USB CDC
│   │   ├── usb_desc.c/.h       # USB 描述符
│   │   └── ch585_usbhs_device.c/.h
│   ├── HAL/                    # 板级硬件抽象层（GBK 编码）
│   │   ├── KEY.c / LED.c / MCU.c / RTC.c / SLEEP.c
│   │   └── include/            # CONFIG.h、HAL.h 等
│   ├── StdPeriphDriver/        # WCH 标准外设驱动库（GBK 编码）
│   │   ├── inc/                # 寄存器与外设头文件
│   │   └── libISP585.a         # ISP 库
│   ├── LIB/                    # BLE 协议栈库与 ROM 固件镜像
│   │   ├── libCH58xBLE.a
│   │   ├── libCH58xBLE_PERI.a
│   │   ├── CH585BLE_ROMx.hex   # ← 必需，勿删
│   │   └── ble_task_scheduler.S
│   ├── Startup/                # 启动汇编 startup_CH585.S
│   ├── Ld/Link.ld              # 链接脚本
│   ├── RVMSIS/core_riscv.h     # RISC-V 内核头文件
│   ├── Makefile                # 命令行构建脚本（UTF-8 编码）
│   ├── .cproject / .project    # Eclipse CDT 工程配置（MounRiver Studio）
│   ├── CH585D.wvproj           # MounRiver Studio 工程文件
│   └── CH585D.launch           # 调试配置
│
├── .gitignore
├── LICENSE
└── README.md
```

`obj/` 编译输出目录及 MounRiver 会话状态已在 `.gitignore` 中排除，克隆后可直接重新编译。

若本地保留了沁恒官方例程包，它位于同级 `EXAM/` 目录，同样已被 `.gitignore` 排除，不会进入版本控制。

---

## 开发环境

| 项目 | 版本 / 路径 |
|------|-------------|
| IDE | **MounRiver Studio 2**（MRS2） |
| 工具链 | RISC-V Embedded GCC 12 (`riscv-wch-elf-gcc`) |
| 默认工具链路径 | `C:/MounRiver/MounRiver_Studio2/resources/app/resources/win32/components/WCH/Toolchain/RISC-V Embedded GCC12/bin` |
| 目标架构 | `rv32imac` / ABI `ilp32` / `-mcmodel=medany` |
| 烧录工具 | WCH-LinkE + MounRiver Studio 内置下载器（或 WCHISPTool） |

固件使用标准 WCH 外设库，**不依赖任何第三方中间件**（EXAM 中的 FreeRTOS / RT-Thread / HarmonyOS 例程不参与本工程编译）。

---

## 编译方法

### 方式一：MounRiver Studio 2（推荐）

1. 打开 MounRiver Studio 2 → `File` → `Import` → `Existing Projects into Workspace`。
2. 选择 `CH585D` 目录导入工程。
3. 确认工具链路径与本机安装一致（默认已指向 `C:/MounRiver/...`）。
4. 点击工具栏 **Build**（锤子图标）编译。
5. 生成物位于 `CH585D/obj/`：`CH585D.elf`、`CH585D.hex`、`CH585D.bin`。

### 方式二：命令行 Makefile

`CH585D/Makefile` 是一份可独立使用的构建脚本（同时保留 Eclipse 工程文件）：

```bash
cd CH585D
make            # 构建 elf / hex / bin
make clean      # 清除 obj/ 输出
```

若工具链未安装在默认路径，请修改 `Makefile` 顶部的 `TOOLCHAIN_DIR` 变量。

> ⚠️ **`make clean` 的 Windows 兼容性**：该目标使用 `rm -rf`，在 Windows `cmd` 下不可用。
> 请在 Git Bash / MSYS2 / WSL 中执行，或手动删除 `obj/` 目录，也可改用 `mingw32-make` 并配套的 Unix 工具集。

---

## 烧录方法

### 使用 MounRiver Studio（推荐）

连接 **WCH-LinkE** 调试器到开发板的 SWD 接口，然后在 MRS2 中点击 **Download** 即可。

### 使用 WCHISPTool（串口 / USB 下载）

将芯片置为 ISP 下载模式（拉低 BOOT 引脚后复位），打开 WCHISPTool：

1. 选择芯片型号 **CH585**；
2. 选择生成的 `CH585D.hex`；
3. 点击 **下载**。

> 注意：`CH585D/LIB/CH585BLE_ROMx.hex` 是 BLE 协议栈的 ROM 固件镜像，
> 由链接脚本与下载配置引用，**请勿删除或修改**。

---

## 串口控制台指令

通过 **USB_FS (PB10/11)** 或 **USB_HS (PB12/13)** 的 CDC 虚拟串口连接，
使用任意串口助手（波特率任意，CDC 不受波特率限制）即可发送指令。

| 指令 | 功能说明 |
|------|----------|
| `help` 或 `?` | 显示控制台帮助菜单 |
| `status` | 显示当前板级测试状态（LED 模式、GPIO 模式、BLE/USB 状态） |
| `read` | 将测试 GPIO 配置为**上拉输入**并读取各引脚电平 |
| `wave` | 使所有测试 GPIO 输出 **2 Hz 方波** |
| `high` | 将所有测试 GPIO 置为 **3.3 V 高电平** |
| `low` | 将所有测试 GPIO 置为 **0 V 低电平** |
| `led on` | PB15 LED **常亮** |
| `led off` | PB15 LED **熄灭** |
| `led blink` | PB15 LED 恢复 **1 Hz 心跳闪烁** |
| `ble` | 显示 BLE 广播信息（名称、间隔、功率） |

示例：

```
> read
=== GPIO Input Status (Pull-Up) ===
 PA12 : HIGH (1)
 PA13 : HIGH (1)
 ...
Tip: Ground any pin to see it change to LOW(0).
```

---

## 注意事项与已知问题

1. **PA9 引脚冲突** — UART1 TX 与测试 GPIO 共用 PA9（详见[硬件资源](#硬件资源与引脚分配)）。
2. **源码编码不统一** — 本仓库中：
   - 自研应用层代码 `CH585D/src/*.c/.h` 与 `CH585D/Makefile` 为 **UTF-8**；
   - WCH 官方驱动与 `EXAM/` 例程为 **GBK**（中文注释）。
   为保持与 MounRiver Studio 的兼容性，官方代码的编码**未做转换**。在 GitHub 网页上浏览
   `StdPeriphDriver/`、`HAL/` 与 `EXAM/` 下的中文注释时可能出现乱码，
   建议 `git clone` 后使用支持 GBK 的编辑器（如 MRS2、VS Code 切换编码）查看。
3. **USB 与 BLE 共存** — 全速 USB、高速 USB 与 BLE 射频同时工作，属该芯片的典型并发场景；
   若遇到枚举失败，请检查 PB10–PB13 的 USB 走线与 33 Ω 匹配电阻。
4. **未启用低功耗** — 当前固件为硬件测试用途，`HAL_SLEEP` 与 `DCDC_ENABLE` 均关闭。

---

## 第三方代码声明

本仓库包含沁恒微电子（Nanjing Qinheng Microelectronics Co., Ltd.）的版权代码：

- `CH585D/StdPeriphDriver/`、`CH585D/RVMSIS/`、`CH585D/Startup/`、`CH585D/HAL/`、`CH585D/LIB/`

这些文件的版权归沁恒微电子所有，并**未**按本仓库的 MIT 许可证授权，
仅可用于沁恒微控制器产品的开发。详见 [LICENSE](LICENSE) 中的说明。

`CH585D/src/` 下的应用层代码为本项目原创部分，采用 MIT 许可证。

> 沁恒官方 CH585 评估板例程包（`EXAM/`）不包含在本仓库中。
> 如需参考，请从沁恒官网获取，或保留在本地 `EXAM/` 目录（已被 `.gitignore` 排除）。

---

## License

`CH585D/src/` 原创应用代码采用 **MIT License**，详见 [LICENSE](LICENSE)。
第三方（沁恒微电子）代码遵循其原有版权与使用条款。
