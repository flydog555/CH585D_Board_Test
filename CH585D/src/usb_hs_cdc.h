#ifndef __USB_HS_CDC_H__
#define __USB_HS_CDC_H__

#include "CH58x_common.h"
#include <ch585_usbhs_device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEF_USB_HS_RX_BUF_LEN      512
#define DEF_USB_HS_TX_BUF_LEN      512

/* 外部引用的缓冲区与结构 */
extern __attribute__((aligned(4))) uint8_t UART2_Tx_Buf[DEF_USB_HS_RX_BUF_LEN];

typedef struct __attribute__((packed)) _CDC_CTL
{
    volatile uint16_t USB_Output_Ptr;
    volatile uint16_t Uart_Output_Ptr;
    volatile uint16_t Uart_Input_Ptr;
    volatile uint16_t Uart_RecLen;
    volatile uint16_t USB_RecLen;
    volatile uint8_t  UploadPoint_Busy;
    volatile uint8_t  DownloadPoint_Busy;
    volatile uint16_t Uart_Timeout_Count;
    volatile uint8_t  Com_Cfg[8];
} CDC_CTL, *PCDC_CTL;

extern volatile CDC_CTL CDC;

/* USB_HS (PB12/PB13) CDC 函数 */
void UART2_USB_Init(void);
void USBHS_CDC_Init(void);
void USBHS_CDC_Process(void);
uint8_t USBHS_CDC_SendData(const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USB_HS_CDC_H__ */
