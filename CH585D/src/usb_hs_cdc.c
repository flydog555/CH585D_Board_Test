#include "usb_hs_cdc.h"
#include "board_test.h"
#include <string.h>

__attribute__((aligned(4))) uint8_t UART2_Tx_Buf[DEF_USB_HS_RX_BUF_LEN];
volatile CDC_CTL CDC;

static char s_hs_cmd_line[128];
static uint8_t s_hs_cmd_len = 0;
static char s_hs_reply_buf[512];

void UART2_USB_Init(void)
{
    // PC 设置串口参数的回调，无需操作物理串口
}

void USBHS_CDC_Init(void)
{
    memset((void *)&CDC, 0, sizeof(CDC));
    s_hs_cmd_len = 0;

    USBHS_Device_Init(ENABLE);
    PFIC_EnableIRQ(USB2_DEVICE_IRQn);
}

uint8_t USBHS_CDC_SendData(const uint8_t *buf, uint16_t len)
{
    uint16_t send_len;

    while (len > 0)
    {
        while (USBHS_Endp_Busy[DEF_UEP2]); // 等待前包发送完成

        send_len = (len > DEF_USB_EP2_HS_SIZE) ? DEF_USB_EP2_HS_SIZE : len;
        memcpy((void *)USBHS_EP2_Tx_Buf, buf, send_len);

        USBHS_Endp_Busy[DEF_UEP2] = DEF_UEP_BUSY;
        R16_U2EP2_T_LEN = send_len;
        R8_U2EP2_TX_CTRL = (R8_U2EP2_TX_CTRL & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;

        buf += send_len;
        len -= send_len;
    }
    return 0;
}

void USBHS_CDC_Process(void)
{
    uint16_t rx_len;
    uint8_t ch;
    uint16_t i;

    // 检查端点 2 是否有下发数据
    if (CDC.DownloadPoint_Busy == 0 && CDC.USB_RecLen > 0)
    {
        rx_len = CDC.USB_RecLen;
        CDC.USB_RecLen = 0;

        for (i = 0; i < rx_len; i++)
        {
            ch = UART2_Tx_Buf[i];

            // 1. 自动字符回显
            USBHS_CDC_SendData(&ch, 1);

            // 2. 命令行识别
            if (ch == '\r' || ch == '\n')
            {
                if (s_hs_cmd_len > 0)
                {
                    s_hs_cmd_line[s_hs_cmd_len] = '\0';
                    Board_Process_Command(s_hs_cmd_line, s_hs_reply_buf, sizeof(s_hs_reply_buf));
                    USBHS_CDC_SendData((const uint8_t *)s_hs_reply_buf, strlen(s_hs_reply_buf));
                    s_hs_cmd_len = 0;
                }
            }
            else if (ch == '\b' || ch == 127)
            {
                if (s_hs_cmd_len > 0) s_hs_cmd_len--;
            }
            else
            {
                if (s_hs_cmd_len < sizeof(s_hs_cmd_line) - 1)
                    s_hs_cmd_line[s_hs_cmd_len++] = ch;
            }
        }

        // 重新开启 EP2 接收 (ACK)
        R8_U2EP2_RX_CTRL = (R8_U2EP2_RX_CTRL & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
        CDC.DownloadPoint_Busy = 1;
    }
}
