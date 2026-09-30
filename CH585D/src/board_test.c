#include "board_test.h"
#include <stdio.h>
#include <string.h>

const TestPin_t g_test_pins[TEST_PIN_COUNT] = {
    {GPIO_Pin_12, "PA12", 0},
    {GPIO_Pin_13, "PA13", 0},
    {GPIO_Pin_14, "PA14", 0},
    {GPIO_Pin_15, "PA15", 0},
    {GPIO_Pin_9,  "PA9",  0},
    {GPIO_Pin_14, "PB14", 1},
    {GPIO_Pin_22, "PB22", 1},
};

static GpioTestMode_e s_gpio_mode = GPIO_MODE_WAVE;
static uint32_t s_last_led_tick = 0;
static uint32_t s_last_wave_tick = 0;
static uint8_t s_wave_state = 0;
static uint8_t s_led_blink_en = 1;

void Board_Test_Init(void)
{
    uint8_t i;

    /* 1. 初始化 PB15 LED */
    GPIOB_ResetBits(LED_PIN);
    GPIOB_ModeCfg(LED_PIN, GPIO_ModeOut_PP_5mA);

    /* 2. 默认将测试引脚配置为推挽输出 */
    for (i = 0; i < TEST_PIN_COUNT; i++)
    {
        if (g_test_pins[i].is_port_b)
        {
            GPIOB_ResetBits(g_test_pins[i].pin);
            GPIOB_ModeCfg(g_test_pins[i].pin, GPIO_ModeOut_PP_5mA);
        }
        else
        {
            GPIOA_ResetBits(g_test_pins[i].pin);
            GPIOA_ModeCfg(g_test_pins[i].pin, GPIO_ModeOut_PP_5mA);
        }
    }

    s_gpio_mode = GPIO_MODE_WAVE;
    s_wave_state = 0;
}

void Board_LED_Toggle(void)
{
    GPIOB_InverseBits(LED_PIN);
}

void Board_LED_Set(uint8_t on)
{
    if (on)
    {
        GPIOB_SetBits(LED_PIN);
    }
    else
    {
        GPIOB_ResetBits(LED_PIN);
    }
}

void Board_GPIO_Set_Mode(GpioTestMode_e mode)
{
    uint8_t i;
    s_gpio_mode = mode;

    if (mode == GPIO_MODE_INPUT)
    {
        /* 设置为上拉输入 */
        for (i = 0; i < TEST_PIN_COUNT; i++)
        {
            if (g_test_pins[i].is_port_b)
            {
                GPIOB_ModeCfg(g_test_pins[i].pin, GPIO_ModeIN_PU);
            }
            else
            {
                GPIOA_ModeCfg(g_test_pins[i].pin, GPIO_ModeIN_PU);
            }
        }
    }
    else
    {
        /* 恢复为推挽输出 */
        for (i = 0; i < TEST_PIN_COUNT; i++)
        {
            if (g_test_pins[i].is_port_b)
            {
                GPIOB_ModeCfg(g_test_pins[i].pin, GPIO_ModeOut_PP_5mA);
            }
            else
            {
                GPIOA_ModeCfg(g_test_pins[i].pin, GPIO_ModeOut_PP_5mA);
            }
        }
    }
}

void Board_GPIO_Set_All(uint8_t high)
{
    uint8_t i;
    Board_GPIO_Set_Mode(GPIO_MODE_MANUAL);

    for (i = 0; i < TEST_PIN_COUNT; i++)
    {
        if (g_test_pins[i].is_port_b)
        {
            if (high)
                GPIOB_SetBits(g_test_pins[i].pin);
            else
                GPIOB_ResetBits(g_test_pins[i].pin);
        }
        else
        {
            if (high)
                GPIOA_SetBits(g_test_pins[i].pin);
            else
                GPIOA_ResetBits(g_test_pins[i].pin);
        }
    }
}

void Board_GPIO_Read_All(char *out_buf, uint16_t max_len)
{
    uint8_t i;
    uint32_t val_a, val_b;
    uint8_t pin_val;
    char temp[40];

    Board_GPIO_Set_Mode(GPIO_MODE_INPUT);
    mDelaymS(2); // 延时等待上拉稳定

    val_a = GPIOA_ReadPortPin(GPIO_Pin_All);
    val_b = GPIOB_ReadPortPin(GPIO_Pin_All);

    snprintf(out_buf, max_len, "\r\n=== GPIO Input Status (Pull-Up) ===\r\n");

    for (i = 0; i < TEST_PIN_COUNT; i++)
    {
        if (g_test_pins[i].is_port_b)
            pin_val = (val_b & g_test_pins[i].pin) ? 1 : 0;
        else
            pin_val = (val_a & g_test_pins[i].pin) ? 1 : 0;

        snprintf(temp, sizeof(temp), " %s : %s (%d)\r\n",
                 g_test_pins[i].name,
                 pin_val ? "HIGH" : "LOW ",
                 pin_val);
        strncat(out_buf, temp, max_len - strlen(out_buf) - 1);
    }
    strncat(out_buf, "Tip: Ground any pin to see it change to LOW(0).\r\n", max_len - strlen(out_buf) - 1);
}

void Board_Test_Process(void)
{
    uint32_t cur_tick = RTC_GetCycle32k(); // 约 32768Hz
    uint8_t i;

    /* 1. LED 500ms 心跳闪烁 (32768 / 2 = 16384) */
    if (s_led_blink_en && (cur_tick - s_last_led_tick >= 16384))
    {
        s_last_led_tick = cur_tick;
        Board_LED_Toggle();
    }

    /* 2. 方波自动翻转测试 (约 250ms 翻转一次，即 2Hz 方波: 8192 ticks) */
    if (s_gpio_mode == GPIO_MODE_WAVE && (cur_tick - s_last_wave_tick >= 8192))
    {
        s_last_wave_tick = cur_tick;
        s_wave_state = !s_wave_state;

        for (i = 0; i < TEST_PIN_COUNT; i++)
        {
            if (g_test_pins[i].is_port_b)
            {
                if (s_wave_state)
                    GPIOB_SetBits(g_test_pins[i].pin);
                else
                    GPIOB_ResetBits(g_test_pins[i].pin);
            }
            else
            {
                if (s_wave_state)
                    GPIOA_SetBits(g_test_pins[i].pin);
                else
                    GPIOA_ResetBits(g_test_pins[i].pin);
            }
        }
    }
}

void Board_Process_Command(const char *cmd, char *reply, uint16_t max_len)
{
    if (strncmp(cmd, "help", 4) == 0 || strncmp(cmd, "?", 1) == 0)
    {
        snprintf(reply, max_len,
            "\r\n================ CH585D Hardware Test Menu ================\r\n"
            " Commands:\r\n"
            "  help / ?      : Show this help menu\r\n"
            "  status        : Show current board test status\r\n"
            "  read          : Configure GPIO as Pull-up IN and read values\r\n"
            "  wave          : Enable 2Hz square wave output on all test GPIOs\r\n"
            "  high          : Set all test GPIOs to 3.3V HIGH\r\n"
            "  low           : Set all test GPIOs to 0V LOW\r\n"
            "  led on        : Turn on PB15 LED constant\r\n"
            "  led off       : Turn off PB15 LED\r\n"
            "  led blink     : Enable PB15 LED 1Hz heartbeat blink\r\n"
            "  ble           : Show BLE advertisement info\r\n"
            "===========================================================\r\n");
    }
    else if (strncmp(cmd, "read", 4) == 0)
    {
        Board_GPIO_Read_All(reply, max_len);
    }
    else if (strncmp(cmd, "wave", 4) == 0)
    {
        Board_GPIO_Set_Mode(GPIO_MODE_WAVE);
        snprintf(reply, max_len, "OK: GPIO 2Hz Square Wave mode enabled on PA9,PA12-15,PB14,PB22.\r\n");
    }
    else if (strncmp(cmd, "high", 4) == 0)
    {
        Board_GPIO_Set_All(1);
        snprintf(reply, max_len, "OK: All test GPIOs set to HIGH (3.3V).\r\n");
    }
    else if (strncmp(cmd, "low", 3) == 0)
    {
        Board_GPIO_Set_All(0);
        snprintf(reply, max_len, "OK: All test GPIOs set to LOW (0V).\r\n");
    }
    else if (strncmp(cmd, "led on", 6) == 0)
    {
        s_led_blink_en = 0;
        Board_LED_Set(1);
        snprintf(reply, max_len, "OK: PB15 LED ON.\r\n");
    }
    else if (strncmp(cmd, "led off", 7) == 0)
    {
        s_led_blink_en = 0;
        Board_LED_Set(0);
        snprintf(reply, max_len, "OK: PB15 LED OFF.\r\n");
    }
    else if (strncmp(cmd, "led blink", 9) == 0)
    {
        s_led_blink_en = 1;
        snprintf(reply, max_len, "OK: PB15 LED Heartbeat Blink enabled.\r\n");
    }
    else if (strncmp(cmd, "status", 6) == 0)
    {
        snprintf(reply, max_len,
            "\r\n--- Status ---\r\n"
            " LED (PB15)   : %s\r\n"
            " GPIO Mode    : %s\r\n"
            " BLE Adv Name : CH585D_Test (100ms interval, 0dBm)\r\n"
            " USB_FS (PB10/11): Active CDC\r\n"
            " USB_HS (PB12/13): Active 480M CDC\r\n",
            s_led_blink_en ? "Heartbeat Blinking" : "Manual",
            s_gpio_mode == GPIO_MODE_WAVE ? "Square Wave (2Hz)" :
            (s_gpio_mode == GPIO_MODE_INPUT ? "Input (Pull-Up)" : "Manual Level"));
    }
    else if (strncmp(cmd, "ble", 3) == 0)
    {
        snprintf(reply, max_len,
            "BLE Device: CH585D_Test\r\n"
            "Adv Interval: 100ms | RF Power: 0dBm\r\n"
            "Use smartphone BLE App (nRF Connect) to scan and check RSSI.\r\n");
    }
    else
    {
        snprintf(reply, max_len, "Unknown command: '%s'. Type 'help' for instructions.\r\n", cmd);
    }
}
