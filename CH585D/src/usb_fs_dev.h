#ifndef __USB_FS_DEV_H__
#define __USB_FS_DEV_H__

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 全速 USB (PB10/PB11) 初始化与任务处理 */
void USBFS_Device_Init(void);
void USBFS_Process(void);
uint8_t USBFS_SendData(const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __USB_FS_DEV_H__ */
