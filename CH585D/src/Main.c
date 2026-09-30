/********************************************************************************
 * File Name          : Main.c
 * Description        : CH585D 自制开发板综合硬件测试固件
 * Features Tested    :
 *   1. BLE RF & Antenna: 广播名称 'CH585D_Test'，100ms周期，0dBm发射功率
 *   2. USB_FS (PB10/11) : 全速 CDC 虚拟串口，支持字符回显与控制台诊断指令
 *   3. USB_HS (PB12/13) : 高速 480Mbps CDC 虚拟串口，支持字符回显与控制台指令
 *   4. LED (PB15)       : 系统 1Hz 心跳闪烁灯
 *   5. GPIOs            : PB22, PB14, PA9, PA12, PA13, PA14, PA15 (方波/输入/高低电平)
 *   6. UART1 (PA9/PA8)  : 115200 波特率同步调试打印
 ********************************************************************************/

#include "CH58x_common.h"
#include "CONFIG.h"
#include "HAL.h"
#include "board_test.h"
#include "ble_dev.h"
#include "usb_fs_dev.h"
#include "usb_hs_cdc.h"

/* BLE 协议栈所需的内存堆缓冲区 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if (defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x05, 0x85, 0xD0};
#endif

/*********************************************************************
 * @fn      Uart1_DebugInit
 * @brief   初始化 UART1 (TX=PA9, RX=PA8) 作为调试串口输出
 *********************************************************************/
void Uart1_DebugInit(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}

/*********************************************************************
 * @fn      main
 * @brief   系统主函数
 *********************************************************************/
int main(void)
{
    /* 1. 时钟配置: 外部 32MHz HSE 晶振 + PLL 62.4MHz 系统主频 */
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    /* 2. 调试串口初始化 (UART1 @ 115200bps) */
    Uart1_DebugInit();

    PRINT("\r\n");
    PRINT("====================================================\r\n");
    PRINT("    CH585D Custom Board Hardware Verification Firm  \r\n");
    PRINT("    System Clock: %d MHz\r\n", (int)(FREQ_SYS / 1000000));
    PRINT("====================================================\r\n");

    /* 3. 板载外设与 GPIO 测试初始化 (LED PB15, GPIOs PA9/12-15/PB14/PB22) */
    Board_Test_Init();
    PRINT("[INIT] LED (PB15) & Test GPIOs initialized.\r\n");

    /* 4. BLE 射频与天线广播初始化 (名称: 'CH585D_Test') */
    PRINT("[INIT] Initializing BLE Stack & Broadcaster...\r\n");
    BLE_Dev_Init();

    /* 5. 全速 USB_FS (PB10/PB11) CDC 虚拟串口初始化 */
    PRINT("[INIT] Initializing USB_FS (Full-Speed CDC on PB10/11)...\r\n");
    USBFS_Device_Init();

    /* 6. 高速 USB_HS (PB12/PB13) CDC 虚拟串口初始化 */
    PRINT("[INIT] Initializing USB_HS (High-Speed CDC on PB12/13)...\r\n");
    USBHS_CDC_Init();

    PRINT("\r\nAll subsystems initialized successfully!\r\n");
    PRINT("--> Look for 'CH585D_Test' in your phone's BLE scanner.\r\n");
    PRINT("--> Connect USB_FS or USB_HS to PC and open COM port at any baudrate.\r\n");
    PRINT("--> PB15 LED should blink at 1Hz (heartbeat).\r\n");
    PRINT("--> Test GPIOs are outputting 2Hz square waves (0V <-> 3.3V).\r\n\r\n");

    /* 7. 主循环调度 */
    while (1)
    {
        /* 运行 TMOS 蓝牙与系统定时任务 */
        TMOS_SystemProcess();

        /* 处理 USB_FS (PB10/PB11) 数据与控制请求 */
        USBFS_Process();

        /* 处理 USB_HS (PB12/PB13) 数据与回显 */
        USBHS_CDC_Process();

        /* 处理 LED 闪烁及 GPIO 方波翻转测试 */
        Board_Test_Process();
    }
}
