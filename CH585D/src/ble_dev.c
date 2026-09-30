#include "ble_dev.h"
#include "HAL.h"

#define DEFAULT_ADVERTISING_INTERVAL    160   // 160 * 0.625ms = 100ms
#define SBP_START_DEVICE_EVT            0x0001

static uint8_t s_ble_task_id = INVALID_TASK_ID;

/* 广播扫描响应数据包 (Scan Response: 设备名称 'CH585D_Test') */
static uint8_t scanRspData[] = {
    // 完整设备名称 (0x09: GAP_ADTYPE_LOCAL_NAME_COMPLETE)
    12, // 长度: 1 (类型) + 11 (字符)
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '5', 'D', '_', 'T', 'e', 's', 't',

    // 发射功率 (0x0A: GAP_ADTYPE_POWER_LEVEL)
    0x02,
    GAP_ADTYPE_POWER_LEVEL,
    0   // 0dBm
};

/* 广播主动广播数据包 (Advertising Data) */
static uint8_t advertData[] = {
    // Flags (0x01: GAP_ADTYPE_FLAGS, 常规发现 + 不支持传统蓝牙)
    0x02,
    GAP_ADTYPE_FLAGS,
    GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // 缩写名称或自定义厂商数据
    12,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '5', 'D', '_', 'T', 'e', 's', 't'
};

static void BLE_Dev_StateNotificationCB(gapRole_States_t newState)
{
    switch (newState)
    {
        case GAPROLE_STARTED:
            PRINT("[BLE] GAP Role Started\r\n");
            break;
        case GAPROLE_ADVERTISING:
            PRINT("[BLE] Advertising Started: Name='CH585D_Test', Int=100ms, Pwr=0dBm\r\n");
            break;
        case GAPROLE_WAITING:
            PRINT("[BLE] Waiting for advertising\r\n");
            break;
        case GAPROLE_ERROR:
            PRINT("[BLE] GAP Role Error\r\n");
            break;
        default:
            break;
    }
}

static gapRolesBroadcasterCBs_t s_broadcaster_cbs = {
    BLE_Dev_StateNotificationCB,
    NULL
};

static uint16_t BLE_Dev_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if (events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;
        if ((pMsg = tmos_msg_receive(s_ble_task_id)) != NULL)
        {
            tmos_msg_deallocate(pMsg);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if (events & SBP_START_DEVICE_EVT)
    {
        GAPRole_BroadcasterStartDevice(&s_broadcaster_cbs);
        return (events ^ SBP_START_DEVICE_EVT);
    }

    return 0;
}

void BLE_Dev_Init(void)
{
    CH58x_BLEInit();
    HAL_Init();
    GAPRole_BroadcasterInit();

    s_ble_task_id = TMOS_ProcessEventRegister(BLE_Dev_ProcessEvent);

    // 广播参数设置
    {
        uint8_t initial_advertising_enable = TRUE;
        uint8_t initial_adv_event_type = GAP_ADTYPE_ADV_SCAN_IND; // 或 GAP_ADTYPE_ADV_NONCONN_IND
        uint16_t advInt = DEFAULT_ADVERTISING_INTERVAL;

        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_ADV_EVENT_TYPE, sizeof(uint8_t), &initial_adv_event_type);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);

        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
    }

    tmos_set_event(s_ble_task_id, SBP_START_DEVICE_EVT);
}
