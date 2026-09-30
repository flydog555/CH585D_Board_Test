/********************************** (C) COPYRIGHT *******************************
 * File Name          : usb_fs_dev.c
 * Author             : WCH / Antigravity
 * Version            : V1.0
 * Date               : 2026/09/29
 * Description        : CH585 全速 USB CDC 虚拟串口驱动 (100% 严格依照官方 EXAM COM 示例)
 * Reference          : EVT/EXAM/USB/Device/COM/src/Main.c
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/

#include "CH58x_common.h"
#include "usb_fs_dev.h"
#include "board_test.h"
#include <string.h>

#define dg_log(...)

#define THIS_ENDP0_SIZE         64
#define MAX_PACKET_SIZE         64

#define USB_CDC_MODE            0
#define USB_VENDOR_MODE         1

/* USB 工作模式: 固定为 CDC 虚拟串口 */
UINT8 usb_work_mode = USB_CDC_MODE;

UINT8 usb_cdc_mode_trans_en = 0;
UINT8 usb_ven_mode_trans_en = 0;

/* CDC 相关描述符 (完全来自官方 EXAM) */
const UINT8 TAB_USB_CDC_DEV_DES[18] =
{
  0x12,
  0x01,
  0x10,
  0x01,
  0x02,
  0x00,
  0x00,
  0x40,
  0x86, 0x1a,           // VID: 0x1A86 (WCH)
  0x40, 0x80,           // PID: 0x8040 (CDC COM)
  0x00, 0x30,
  0x01,                 // 厂商字符串
  0x02,                 // 产品字符串
  0x03,                 // 序列号字符串
  0x01                  // 配置数
};

const UINT8 TAB_USB_CDC_CFG_DES[ ] =
{
  0x09,0x02,0x43,0x00,0x02,0x01,0x00,0x80,0x30,

  // 接口0: CDC 控制接口
  0x09, 0x04,0x00,0x00,0x01,0x02,0x02,0x01,0x00,

  0x05,0x24,0x00,0x10,0x01,
  0x04,0x24,0x02,0x02,
  0x05,0x24,0x06,0x00,0x01,
  0x05,0x24,0x01,0x01,0x00,

  0x07,0x05,0x84,0x03,0x08,0x00,0x01,                       // 中断上传端点 4 IN

  // 接口1: 数据接口
  0x09,0x04,0x01,0x00,0x02,0x0a,0x00,0x00,0x00,

  0x07,0x05,0x01,0x02,0x40,0x00,0x00,                       // 批量下传端点 1 OUT
  0x07,0x05,0x81,0x02,0x40,0x00,0x00,                       // 批量上传端点 1 IN
};

/* 设备限定描述符 */
const UINT8 My_QueDescr[ ] = { 0x0A, 0x06, 0x00, 0x02, 0xFF, 0x00, 0xFF, 0x40, 0x01, 0x00 };

UINT8 TAB_CDC_LINE_CODING[ ] =
{
  0x85, /* baud rate: 115200 */
  0x20,
  0x00,
  0x00,
  0x00,   /* stop bits - 1 */
  0x00,   /* parity - none */
  0x08    /* no. of bits 8 */
};

// 语言描述符
const UINT8 TAB_USB_LID_STR_DES[ ] = { 0x04, 0x03, 0x09, 0x04 };

const UINT8 USB_DEV_PARA_CDC_SERIAL_STR[]=      "WCH121212TS1";
const UINT8 USB_DEV_PARA_CDC_PRODUCT_STR[]=     "CH585D FS CDC Port";
const UINT8 USB_DEV_PARA_CDC_MANUFACTURE_STR[]=  "wch.cn";

typedef struct DevInfo
{
  UINT8 UsbConfig;      // USB 配置标志
  UINT8 UsbAddress;     // USB 设备地址
  UINT8 gSetupReq;      // USB 请求
  UINT8 gSetupLen;      // USB 控制传输长度
  UINT8 gUsbInterCfg;   // USB 接口
  UINT8 gUsbFlag;       // 标志
} DevInfo_Parm;

static DevInfo_Parm devinf;
static UINT8 SetupReqCode, SetupLen;

/* 端点硬件缓冲区 (严格按官方分配) */
__aligned(4) UINT8 Ep0Buffer[MAX_PACKET_SIZE];        // 端点0 收发
__aligned(4) UINT8 Ep1Buffer[2*MAX_PACKET_SIZE];      // OUT & IN
__aligned(4) UINT8 Ep2Buffer[2*MAX_PACKET_SIZE];      // OUT & IN
__aligned(4) UINT8 Ep3Buffer[2*MAX_PACKET_SIZE];      // OUT & IN

// Line Code 结构
typedef struct __PACKED _LINE_CODE
{
  UINT32 BaudRate;
  UINT8 StopBits;
  UINT8 ParityType;
  UINT8 DataBits;
} LINE_CODE, *PLINE_CODE;

static LINE_CODE Uart0Para;

#define CH341_REG_NUM     10
static UINT8 CH341_Reg_Add[CH341_REG_NUM];
static UINT8 CH341_Reg_val[CH341_REG_NUM];

static UINT8 VENSer0ParaChange = 0;
static UINT8 VENSer0SendFlag = 0;
static UINT8 UART0_RTS_Val = 0;
static UINT8 UART0_DTR_Val = 0;
static UINT8 UART0_OUT_Val = 0;
static UINT8 UART0_DCD_Val = 0;
static UINT8 UART0_RI_Val = 0;
static UINT8 UART0_DSR_Val = 0;
static UINT8 UART0_CTS_Val = 0;

static UINT8 CDCSetSerIdx = 0;
static UINT8 CDCSer0ParaChange = 0;

typedef struct _USB_SETUP_REQ_ {
    UINT8 bRequestType;
    UINT8 bRequest;
    UINT8 wValueL;
    UINT8 wValueH;
    UINT8 wIndexL;
    UINT8 wIndexH;
    UINT8 wLengthL;
    UINT8 wLengthH;
} USB_SETUP_REQ_t;

#define UsbSetupBuf     ((USB_SETUP_REQ_t *)Ep0Buffer)

/* 端点状态 */
volatile UINT8 Ep1DataINFlag = 0;
volatile UINT8 Ep1DataOUTFlag = 0;
volatile UINT8 Ep1DataOUTLen = 0;
__aligned(4) UINT8 Ep1OUTDataBuf[MAX_PACKET_SIZE];

volatile UINT8 Ep2DataINFlag = 0;
volatile UINT8 Ep2DataOUTFlag = 0;
volatile UINT8 Ep2DataOUTLen = 0;
__aligned(4) UINT8 Ep2OUTDataBuf[MAX_PACKET_SIZE];

volatile UINT8 Ep3DataINFlag = 0;
volatile UINT8 Ep4DataINFlag = 0;

/* 中断队列管理 (官方环形队列) */
#define USB_IRQ_FLAG_NUM     4

static volatile UINT8 usb_irq_w_idx = 0;
static volatile UINT8 usb_irq_r_idx = 0;

static volatile UINT8 usb_irq_len[USB_IRQ_FLAG_NUM];
static volatile UINT8 usb_irq_pid[USB_IRQ_FLAG_NUM];
static volatile UINT8 usb_irq_flag[USB_IRQ_FLAG_NUM];

static UINT8 cdc_uart_sta_trans_step = 0;
static UINT8 ven_ep1_trans_step = 0;

/* 端点0枚举上传缓存 */
static UINT8 ep0_send_buf[256];

#define HAL_UART_5_BITS_PER_CHAR             5
#define HAL_UART_6_BITS_PER_CHAR             6
#define HAL_UART_7_BITS_PER_CHAR             7
#define HAL_UART_8_BITS_PER_CHAR             8

#define HAL_UART_ONE_STOP_BIT                1
#define HAL_UART_TWO_STOP_BITS               2

#define HAL_UART_NO_PARITY                   0x00
#define HAL_UART_ODD_PARITY                  0x01
#define HAL_UART_EVEN_PARITY                 0x02
#define HAL_UART_MARK_PARITY                 0x03
#define HAL_UART_SPACE_PARITY                0x04

#define ENDP0                           0x00
#define ENDP1                           0x01
#define ENDP2                           0x02
#define ENDP3                           0x03
#define ENDP4                           0x04

#define ENDP_TYPE_IN                    0x00
#define ENDP_TYPE_OUT                   0x01

#define OUT_ACK                         0
#define OUT_TIMOUT                      1
#define OUT_NAK                         2
#define OUT_STALL                       3

#define IN_ACK                          0
#define IN_NORSP                        1
#define IN_NAK                          2
#define IN_STALL                        3

#define DEF_BIT_USB_RESET               0x01
#define DEF_BIT_USB_DEV_DESC            0x02
#define DEF_BIT_USB_ADDRESS             0x04
#define DEF_BIT_USB_CFG_DESC            0x08
#define DEF_BIT_USB_SET_CFG             0x10
#define DEF_BIT_USB_WAKE                0x20
#define DEF_BIT_USB_SUPD                0x40
#define DEF_BIT_USB_HS                  0x80

#define DEF_SEND_ENCAPSULATED_COMMAND  0x00
#define DEF_GET_ENCAPSULATED_RESPONSE  0x01
#define DEF_SET_COMM_FEATURE           0x02
#define DEF_GET_COMM_FEATURE           0x03
#define DEF_CLEAR_COMM_FEATURE         0x04
#define DEF_SET_LINE_CODING            0x20
#define DEF_GET_LINE_CODING            0x21
#define DEF_SET_CONTROL_LINE_STATE     0x22
#define DEF_SEND_BREAK                 0x23

#define DEF_NETWORK_CONNECTION         0x00
#define DEF_RESPONSE_AVAILABLE         0x01
#define DEF_SERIAL_STATE               0x20

/* 控制台交互缓冲 */
static char s_fs_cmd[128];
static uint8_t s_fs_cmd_len = 0;
static char s_fs_reply[512];

/* 端点状态设置函数 (官方) */
void USBDevEPnINSetStatus(UINT8 ep_num, UINT8 type, UINT8 sta)
{
  UINT8 *p_UEPn_CTRL;

  p_UEPn_CTRL = (UINT8 *)(USB_BASE_ADDR + 0x22 + ep_num * 4);
  if(type == ENDP_TYPE_IN) *((PUINT8V)p_UEPn_CTRL) = (*((PUINT8V)p_UEPn_CTRL) & (~(0x03))) | sta;
  else *((PUINT8V)p_UEPn_CTRL) = (*((PUINT8V)p_UEPn_CTRL) & (~(0x03<<2))) | (sta<<2);
}

/* 中断处理 (官方严格实现) */
__attribute__((interrupt("WCH-Interrupt-fast")))
__attribute__((section(".highcode")))
void USB_IRQHandler(void)
{
  UINT8   j;

  if(R8_USB_INT_FG & RB_UIF_TRANSFER)
  {
    /* 非setup包 */
    if((R8_USB_INT_ST & MASK_UIS_TOKEN) != MASK_UIS_TOKEN){
      usb_irq_flag[usb_irq_w_idx] = 1;
      usb_irq_pid[usb_irq_w_idx]  = R8_USB_INT_ST;
      usb_irq_len[usb_irq_w_idx]  = R8_USB_RX_LEN;

      switch(usb_irq_pid[usb_irq_w_idx]& 0x3f){
        case UIS_TOKEN_OUT | 2:{
          if( R8_USB_INT_FG & RB_U_TOG_OK ){
            R8_UEP2_CTRL ^=  RB_UEP_R_TOG;
            R8_UEP2_CTRL = (R8_UEP2_CTRL & 0xf3) | 0x08; //OUT_NAK
            for(j=0; j<(MAX_PACKET_SIZE/4); j++)
              ((UINT32 *)Ep2OUTDataBuf)[j] = ((UINT32 *)Ep2Buffer)[j];
          }
          else usb_irq_flag[usb_irq_w_idx] = 0;
        }break;
        case UIS_TOKEN_IN | 2:{
          R8_UEP2_CTRL ^=  RB_UEP_T_TOG;
          R8_UEP2_CTRL = (R8_UEP2_CTRL & 0xfc) | IN_NAK;
        }break;
        case UIS_TOKEN_OUT | 1:{
          if( R8_USB_INT_FG & RB_U_TOG_OK ){
            R8_UEP1_CTRL ^=  RB_UEP_R_TOG;
            R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xf3) | 0x08; //OUT_NAK
            for(j=0; j<(MAX_PACKET_SIZE/4); j++)
              ((UINT32 *)Ep1OUTDataBuf)[j] = ((UINT32 *)Ep1Buffer)[j];
          }
          else usb_irq_flag[usb_irq_w_idx] = 0;
        }break;
        case UIS_TOKEN_IN | 1:{
          R8_UEP1_CTRL ^=  RB_UEP_T_TOG;
          R8_UEP1_CTRL = (R8_UEP1_CTRL & 0xfc) | IN_NAK;
        }break;
        case UIS_TOKEN_OUT | 0:{
          if( R8_USB_INT_FG & RB_U_TOG_OK )
            R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xf3) | 0x08; //OUT_NAK
          else usb_irq_flag[usb_irq_w_idx] = 0;
        }break;
        case UIS_TOKEN_IN | 0:{
          R8_UEP0_CTRL = (R8_UEP0_CTRL & 0xfc) | IN_NAK;
        }break;
      }

      if(usb_irq_flag[usb_irq_w_idx]){
        usb_irq_w_idx++;
        if(usb_irq_w_idx >= USB_IRQ_FLAG_NUM) usb_irq_w_idx = 0;
      }

      R8_USB_INT_FG = RB_UIF_TRANSFER;
    }

    /* setup包 */
    if(R8_USB_INT_ST & RB_UIS_SETUP_ACT){
      usb_irq_flag[usb_irq_w_idx] = 1;
      usb_irq_pid[usb_irq_w_idx]  = UIS_TOKEN_SETUP | 0;
      usb_irq_len[usb_irq_w_idx]  = 8;

      usb_irq_w_idx++;
      if(usb_irq_w_idx >= USB_IRQ_FLAG_NUM) usb_irq_w_idx = 0;

      R8_USB_INT_FG = RB_UIF_TRANSFER;
    }
  }
}

/* 官方主循环处理函数 */
void USB_IRQProcessHandler( void )
{
  static  const UINT8  *pDescr;
  UINT8 len;
  UINT8   data_dir = 0;
  UINT8   i;

  i = usb_irq_r_idx;

  if(usb_irq_flag[i])
  {
    usb_irq_r_idx++;
    if(usb_irq_r_idx >= USB_IRQ_FLAG_NUM) usb_irq_r_idx = 0;

    switch ( usb_irq_pid[i] & 0x3f )
    {
      case UIS_TOKEN_IN | 4:
      {
        Ep4DataINFlag = ~0;
        break;
      }
      case UIS_TOKEN_IN | 3:
      {
        Ep3DataINFlag = ~0;
        break;
      }
      case UIS_TOKEN_OUT | 2:
      {
        len = usb_irq_len[i];
        Ep2DataOUTFlag = 1;
        Ep2DataOUTLen = len;
        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP2_CTRL = R8_UEP2_CTRL & 0xf3; //OUT_ACK
        PFIC_EnableIRQ(USB_IRQn);
        break;
      }
      case UIS_TOKEN_IN | 2:
      {
        Ep2DataINFlag = 1;
        break;
      }
      case UIS_TOKEN_OUT | 1:    // endpoint 1# 接收 PC 批量下传
      {
        len = usb_irq_len[i];
        Ep1DataOUTFlag = 1;
        Ep1DataOUTLen = len;

        PFIC_DisableIRQ(USB_IRQn);
        R8_UEP1_CTRL = R8_UEP1_CTRL & 0xf3; //OUT_ACK
        PFIC_EnableIRQ(USB_IRQn);

        /* 终端回显与命令行交互 */
        for (uint8_t k = 0; k < len; k++)
        {
            uint8_t ch = Ep1OUTDataBuf[k];
            USBFS_SendData(&ch, 1);

            if (ch == '\r' || ch == '\n')
            {
                if (s_fs_cmd_len > 0)
                {
                    s_fs_cmd[s_fs_cmd_len] = '\0';
                    Board_Process_Command(s_fs_cmd, s_fs_reply, sizeof(s_fs_reply));
                    USBFS_SendData((const uint8_t *)s_fs_reply, strlen(s_fs_reply));
                    s_fs_cmd_len = 0;
                }
            }
            else if (ch == '\b' || ch == 127)
            {
                if (s_fs_cmd_len > 0) s_fs_cmd_len--;
            }
            else
            {
                if (s_fs_cmd_len < sizeof(s_fs_cmd) - 1)
                    s_fs_cmd[s_fs_cmd_len++] = ch;
            }
        }
        break;
      }
      case UIS_TOKEN_IN | 1:   // endpoint 1# 上传完成
      {
        Ep1DataINFlag = 1;
        break;
      }
      case UIS_TOKEN_SETUP | 0:    // endpoint 0# SETUP
      {
        len = usb_irq_len[i];
        if(len == sizeof(USB_SETUP_REQ_t))
        {
          SetupLen = UsbSetupBuf->wLengthL;
          if(UsbSetupBuf->wLengthH) SetupLen = 0xff;

          len = 0;
          SetupReqCode = UsbSetupBuf->bRequest;

          data_dir = USB_REQ_TYP_OUT;
          if(UsbSetupBuf->bRequestType & USB_REQ_TYP_IN) data_dir = USB_REQ_TYP_IN;

          // 标准请求
          if((UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK) == USB_REQ_TYP_STANDARD)
          {
            switch( SetupReqCode )
            {
              case USB_GET_DESCRIPTOR:
              {
                switch( UsbSetupBuf->wValueH )
                {
                  case 1: // 设备描述符
                  {
                    memcpy(ep0_send_buf, &TAB_USB_CDC_DEV_DES[0], sizeof( TAB_USB_CDC_DEV_DES ));
                    pDescr = ep0_send_buf;
                    len = sizeof( TAB_USB_CDC_DEV_DES );
                    break;
                  }
                  case 2:  // 配置描述符
                  {
                    memcpy(ep0_send_buf, &TAB_USB_CDC_CFG_DES[0], sizeof( TAB_USB_CDC_CFG_DES ));
                    pDescr = ep0_send_buf;
                    len = sizeof( TAB_USB_CDC_CFG_DES );
                    break;
                  }
                  case 3:  // 字符串描述符
                  {
                    switch(UsbSetupBuf->wValueL)
                    {
                      case 0:
                      {
                        pDescr = (const UINT8 *)( &TAB_USB_LID_STR_DES[0] );
                        len = sizeof( TAB_USB_LID_STR_DES );
                        break;
                      }
                      case 1:  // iManufacturer
                      case 2:  // iProduct
                      case 3:  // iSerialNumber
                      {
                        UINT8 ep0_str_len;
                        UINT8 *p_send;
                        const UINT8 *manu_str;
                        UINT8 tmp;

                        if(UsbSetupBuf->wValueL == 1)
                          manu_str = (const UINT8 *)USB_DEV_PARA_CDC_MANUFACTURE_STR;
                        else if(UsbSetupBuf->wValueL == 2)
                          manu_str = (const UINT8 *)USB_DEV_PARA_CDC_PRODUCT_STR;
                        else
                          manu_str = (const UINT8 *)USB_DEV_PARA_CDC_SERIAL_STR;

                        ep0_str_len = (UINT8)strlen((const char *)manu_str);
                        p_send = ep0_send_buf;
                        *p_send++ = ep0_str_len*2 + 2;
                        *p_send++ = 0x03;
                        for(tmp = 0; tmp<ep0_str_len; tmp++)
                        {
                          *p_send++ = manu_str[tmp];
                          *p_send++ = 0x00;
                        }

                        pDescr = ep0_send_buf;
                        len = ep0_send_buf[0];
                        break;
                      }
                      default:
                        len = 0xFF;
                        break;
                    }
                    break;
                  }
                  case 6:  // 设备限定描述符
                  {
                    pDescr = (const UINT8 *)( &My_QueDescr[0] );
                    len = sizeof( My_QueDescr );
                    break;
                  }
                  default:
                    len = 0xFF;
                    break;
                }
                if ( SetupLen > len ) SetupLen = len;
                len = (SetupLen >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : SetupLen;
                memcpy( Ep0Buffer, pDescr, len );
                SetupLen -= len;
                pDescr += len;
                break;
              }
              case USB_SET_ADDRESS:
              {
                devinf.gUsbFlag |= DEF_BIT_USB_ADDRESS;
                devinf.UsbAddress = UsbSetupBuf->wValueL;
                break;
              }
              case USB_GET_CONFIGURATION:
              {
                Ep0Buffer[0] = devinf.UsbConfig;
                if ( SetupLen >= 1 ) len = 1;
                break;
              }
              case USB_SET_CONFIGURATION:
              {
                devinf.gUsbFlag |= DEF_BIT_USB_SET_CFG;
                devinf.UsbConfig = UsbSetupBuf->wValueL;
                break;
              }
              case USB_CLEAR_FEATURE:
              {
                len = 0;
                if( ( UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_DEVICE )
                {
                  R8_UEP1_CTRL = (R8_UEP1_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK;
                  R8_UEP2_CTRL = (R8_UEP2_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK;
                  R8_UEP3_CTRL = (R8_UEP3_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK;
                  R8_UEP4_CTRL = (R8_UEP4_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK;

                  Ep1DataINFlag = 1;
                  Ep2DataINFlag = 1;
                  Ep3DataINFlag = 1;
                  Ep4DataINFlag = 1;

                  Ep1DataOUTFlag = 0;
                  Ep2DataOUTFlag = 0;
                  cdc_uart_sta_trans_step = 0;
                  ven_ep1_trans_step = 0;
                }
                else if ( ( UsbSetupBuf->bRequestType & USB_REQ_RECIP_MASK ) == USB_REQ_RECIP_ENDP )
                {
                  switch( UsbSetupBuf->wIndexL )
                  {
                    case 0x84: R8_UEP4_CTRL = (R8_UEP4_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK; break;
                    case 0x04: R8_UEP4_CTRL = (R8_UEP4_CTRL & (~ ( RB_UEP_R_TOG | MASK_UEP_R_RES ))) | UEP_R_RES_ACK; break;
                    case 0x83: R8_UEP3_CTRL = (R8_UEP3_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK; break;
                    case 0x03: R8_UEP3_CTRL = (R8_UEP3_CTRL & (~ ( RB_UEP_R_TOG | MASK_UEP_R_RES ))) | UEP_R_RES_ACK; break;
                    case 0x82: R8_UEP2_CTRL = (R8_UEP2_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK; break;
                    case 0x02: R8_UEP2_CTRL = (R8_UEP2_CTRL & (~ ( RB_UEP_R_TOG | MASK_UEP_R_RES ))) | UEP_R_RES_ACK; break;
                    case 0x81: R8_UEP1_CTRL = (R8_UEP1_CTRL & (~ ( RB_UEP_T_TOG | MASK_UEP_T_RES ))) | UEP_T_RES_NAK; break;
                    case 0x01: R8_UEP1_CTRL = (R8_UEP1_CTRL & (~ ( RB_UEP_R_TOG | MASK_UEP_R_RES ))) | UEP_R_RES_ACK; break;
                    default: len = 0xFF;  break;
                  }
                }
                else len = 0xFF;
                break;
              }
              case USB_GET_INTERFACE:
              {
                Ep0Buffer[0] = 0x00;
                if ( SetupLen >= 1 ) len = 1;
                break;
              }
              case USB_GET_STATUS:
              {
                Ep0Buffer[0] = 0x00;
                Ep0Buffer[1] = 0x00;
                if ( SetupLen >= 2 ) len = 2;
                else len = SetupLen;
                break;
              }
              default:
                len = 0xFF;
                break;
            }
          }
          // 类请求
          else if( ( UsbSetupBuf->bRequestType & USB_REQ_TYP_MASK ) == USB_REQ_TYP_CLASS )
          {
            if(data_dir == USB_REQ_TYP_OUT)
            {
              switch( SetupReqCode )
              {
                case DEF_SET_LINE_CODING:
                {
                  if( Ep0Buffer[ 4 ] == 0x00 )
                  {
                    CDCSetSerIdx = 0;
                    len = 0x00;
                  }
                  else if( Ep0Buffer[ 4 ] == 0x02 )
                  {
                    CDCSetSerIdx = 1;
                    len = 0x00;
                  }
                  else len = 0xFF;
                  break;
                }
                case DEF_SET_CONTROL_LINE_STATE:
                {
                  len = 0;
                  break;
                }
                default:
                {
                  len = 0xFF;
                  break;
                }
              }
            }
            else
            {
              switch( SetupReqCode )
              {
                case DEF_GET_LINE_CODING:
                {
                  pDescr = Ep0Buffer;
                  len = sizeof( LINE_CODE );
                  ( ( PLINE_CODE )Ep0Buffer )->BaudRate   = Uart0Para.BaudRate;
                  ( ( PLINE_CODE )Ep0Buffer )->StopBits   = Uart0Para.StopBits;
                  ( ( PLINE_CODE )Ep0Buffer )->ParityType = Uart0Para.ParityType;
                  ( ( PLINE_CODE )Ep0Buffer )->DataBits   = Uart0Para.DataBits;
                  break;
                }
                case DEF_SERIAL_STATE:
                {
                  len = 2;
                  CDCSetSerIdx = 0;
                  Ep0Buffer[0] = 0;
                  Ep0Buffer[1] = 0;
                  break;
                }
                default:
                {
                  len = 0xFF;
                  break;
                }
              }
            }
          }
          else len = 0xFF;
        }
        else
        {
          len = 0xFF;
        }

        if ( len == 0xFF )
        {
          SetupReqCode = 0xFF;
          PFIC_DisableIRQ(USB_IRQn);
          R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_STALL | UEP_T_RES_STALL;
          PFIC_EnableIRQ(USB_IRQn);
        }
        else if ( len <= THIS_ENDP0_SIZE )
        {
          if( SetupReqCode == USB_SET_ADDRESS || SetupReqCode == USB_SET_CONFIGURATION ||
              SetupReqCode == DEF_SET_CONTROL_LINE_STATE || SetupReqCode == USB_CLEAR_FEATURE )
          {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_T_LEN = len;
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
            PFIC_EnableIRQ(USB_IRQn);
          }
          else if( SetupReqCode == USB_GET_DESCRIPTOR )
          {
            R8_UEP0_T_LEN = len;
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
            PFIC_EnableIRQ(USB_IRQn);
          }
          else
          {
            if(data_dir == USB_REQ_TYP_IN)
            {
              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_T_LEN = len;
              R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_NAK | UEP_T_RES_ACK;
              PFIC_EnableIRQ(USB_IRQn);
            }
            else
            {
              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_T_LEN = len;
              R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
              PFIC_EnableIRQ(USB_IRQn);
            }
          }
        }
        else
        {
          R8_UEP0_T_LEN = 0;
          PFIC_DisableIRQ(USB_IRQn);
          R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
          PFIC_EnableIRQ(USB_IRQn);
        }
        break;
      }
      case UIS_TOKEN_IN | 0:      // endpoint 0# IN
      {
        switch( SetupReqCode )
        {
          case USB_GET_DESCRIPTOR:
          {
            len = (SetupLen >= THIS_ENDP0_SIZE) ? THIS_ENDP0_SIZE : SetupLen;
            memcpy( Ep0Buffer, pDescr, len );
            SetupLen -= len;
            pDescr += len;

            if(len)
            {
              R8_UEP0_T_LEN = len;
              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_CTRL ^=  RB_UEP_T_TOG;
              USBDevEPnINSetStatus(ENDP0, ENDP_TYPE_IN, IN_ACK);
              PFIC_EnableIRQ(USB_IRQn);
            }
            else
            {
              R8_UEP0_T_LEN = len;
              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_ACK | UEP_T_RES_NAK;
              PFIC_EnableIRQ(USB_IRQn);
            }
            break;
          }
          case USB_SET_ADDRESS:
          {
            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | (devinf.UsbAddress);
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK | UEP_T_RES_NAK;
            PFIC_EnableIRQ(USB_IRQn);
            break;
          }
          case DEF_GET_LINE_CODING:
          {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_ACK | UEP_T_RES_NAK;
            PFIC_EnableIRQ(USB_IRQn);
            break;
          }
          case DEF_SET_LINE_CODING:
          {
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK | UEP_T_RES_NAK;
            PFIC_EnableIRQ(USB_IRQn);
            break;
          }
          default:
          {
            R8_UEP0_T_LEN = 0;
            PFIC_DisableIRQ(USB_IRQn);
            R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK | UEP_T_RES_NAK;
            PFIC_EnableIRQ(USB_IRQn);
            break;
          }
        }
        break;
      }
      case UIS_TOKEN_OUT | 0:      // endpoint 0# OUT
      {
        len = usb_irq_len[i];
        if(len)
        {
          switch(SetupReqCode)
          {
            case DEF_SET_LINE_CODING:
            {
              UINT32 set_bps;
              UINT8  data_bit;
              UINT8  stop_bit;
              UINT8  ver_bit;

              memcpy(&set_bps,Ep0Buffer,4);
              stop_bit = Ep0Buffer[4];
              ver_bit = Ep0Buffer[5];
              data_bit = Ep0Buffer[6];

              Uart0Para.BaudRate = set_bps;
              Uart0Para.StopBits = stop_bit;
              Uart0Para.ParityType = ver_bit;
              Uart0Para.DataBits = data_bit;
              CDCSer0ParaChange = 1;

              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK|UEP_T_RES_ACK;
              PFIC_EnableIRQ(USB_IRQn);
              break;
            }
            default:
              PFIC_DisableIRQ(USB_IRQn);
              R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK | UEP_T_RES_NAK;
              PFIC_EnableIRQ(USB_IRQn);
              break;
          }
        }
        else
        {
          PFIC_DisableIRQ(USB_IRQn);
          R8_UEP0_CTRL = RB_UEP_R_TOG|RB_UEP_T_TOG|UEP_R_RES_NAK|UEP_T_RES_NAK;
          PFIC_EnableIRQ(USB_IRQn);
        }
        break;
      }
      default:
        break;
    }

    PFIC_DisableIRQ(USB_IRQn);
    usb_irq_flag[i] = 0;
    PFIC_EnableIRQ(USB_IRQn);
  }

  // USB 总线复位处理 (官方逻辑)
  if ( R8_USB_INT_FG & RB_UIF_BUS_RST )
  {
    R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP3_CTRL = UEP_T_RES_NAK;
    R8_UEP4_CTRL = UEP_T_RES_NAK;

    cdc_uart_sta_trans_step = 0;
    ven_ep1_trans_step = 0;

    R8_USB_DEV_AD = 0x00;
    devinf.UsbAddress = 0;

    R8_USB_INT_FG = RB_UIF_BUS_RST;
  }
  else if ( R8_USB_INT_FG & RB_UIF_SUSPEND )
  {
    if ( R8_USB_MIS_ST & RB_UMS_SUSPEND )
    {
      CDCSer0ParaChange = 1;

      Ep1DataINFlag = 0;
      Ep2DataINFlag = 0;
      Ep3DataINFlag = 0;
      Ep4DataINFlag = 0;

      Ep1DataOUTFlag = 0;
      Ep2DataOUTFlag = 0;
    }
    else
    {
      Ep1DataINFlag = 1;
      Ep2DataINFlag = 1;
      Ep3DataINFlag = 1;
      Ep4DataINFlag = 1;

      Ep1DataOUTFlag = 0;
      Ep2DataOUTFlag = 0;
    }

    cdc_uart_sta_trans_step = 0;
    ven_ep1_trans_step = 0;

    R8_USB_INT_FG = RB_UIF_SUSPEND;
  }
}

/* 初始化 CDC 变量 */
static void USBParaInit(void)
{
  Ep1DataINFlag = 1;
  Ep1DataOUTFlag = 0;
  Ep2DataINFlag = 1;
  Ep2DataOUTFlag = 0;
  Ep3DataINFlag = 1;
  Ep4DataINFlag = 1;
}

/* 初始化 CDC 设备寄存器 (完全来自官方 EXAM) */
static void InitCDCDevice(void)
{
  USBParaInit();

  R8_USB_CTRL = 0x00;

  R8_UEP4_1_MOD = RB_UEP4_TX_EN | RB_UEP1_TX_EN | RB_UEP1_RX_EN;
  R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_TX_EN;

  R32_UEP0_DMA = (UINT32)&Ep0Buffer[0];
  R32_UEP1_DMA = (UINT32)&Ep1Buffer[0];
  R32_UEP2_DMA = (UINT32)&Ep2Buffer[0];
  R32_UEP3_DMA = (UINT32)&Ep3Buffer[0];

  R8_UEP0_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
  R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
  R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
  R8_UEP3_CTRL = UEP_T_RES_NAK;
  R8_UEP4_CTRL = UEP_T_RES_NAK;

  R8_USB_DEV_AD = 0x00;

  // 禁止 DP/DM 下拉
  R8_UDEV_CTRL = RB_UD_PD_DIS;

  // 启用内部上拉、中断 Busy、DMA
  R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;

  // 清除并使能中断
  R8_USB_INT_FG = 0xFF;
  R8_USB_INT_EN = RB_UIE_TRANSFER;
  PFIC_EnableIRQ(USB_IRQn);

  // 使能 USB 端口
  R8_UDEV_CTRL |= RB_UD_PORT_EN;

  devinf.UsbConfig = 0;
  devinf.UsbAddress = 0;
}

static void InitUSBDevPara(void)
{
  UINT8 i;

  Uart0Para.BaudRate = 115200;
  Uart0Para.DataBits = HAL_UART_8_BITS_PER_CHAR;
  Uart0Para.ParityType = HAL_UART_NO_PARITY;
  Uart0Para.StopBits = HAL_UART_ONE_STOP_BIT;

  VENSer0ParaChange = 0;
  VENSer0SendFlag = 0;
  CDCSetSerIdx = 0;
  CDCSer0ParaChange = 0;

  for(i=0; i<CH341_REG_NUM; i++)
  {
    CH341_Reg_Add[i] = 0xff;
    CH341_Reg_val[i] = 0x00;
  }

  UART0_DCD_Val = 0;
  UART0_RI_Val = 0;
  UART0_DSR_Val = 0;
  UART0_CTS_Val = 0;

  UART0_RTS_Val = 0;
  UART0_DTR_Val = 0;
  UART0_OUT_Val = 0;

  for(i=0; i<USB_IRQ_FLAG_NUM; i++)
  {
    usb_irq_flag[i] = 0;
  }
}

/* 对外暴露初始化接口 */
void USBFS_Device_Init(void)
{
  InitUSBDevPara();
  InitCDCDevice();
}

/* 对外暴露主循环任务处理接口 */
void USBFS_Process(void)
{
  USB_IRQProcessHandler();
}

/* 对外暴露发送数据接口 (官方 SendUSBData) */
uint8_t USBFS_SendData(const uint8_t *p_send_dat, uint16_t send_len)
{
  while (send_len > 0)
  {
    uint16_t this_len = (send_len > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : send_len;
    uint32_t timeout = 50000;

    while (!Ep1DataINFlag && --timeout)
    {
      USB_IRQProcessHandler();
    }

    memcpy(&Ep1Buffer[MAX_PACKET_SIZE], p_send_dat, this_len);

    Ep1DataINFlag = 0;
    R8_UEP1_T_LEN = (UINT8)this_len;
    PFIC_DisableIRQ(USB_IRQn);
    R8_UEP1_CTRL = R8_UEP1_CTRL & 0xfc; // IN_ACK
    PFIC_EnableIRQ(USB_IRQn);

    p_send_dat += this_len;
    send_len -= this_len;
  }
  return 0;
}
