#ifndef __BOARD_TEST_H__
#define __BOARD_TEST_H__

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* LED Pin */
#define LED_PIN             GPIO_Pin_15
#define LED_PORT            GPIOB

/* 测试 GPIO 数量与结构 */
typedef struct {
    uint32_t pin;
    char name[8];
    uint8_t is_port_b; // 0: Port A, 1: Port B
} TestPin_t;

#define TEST_PIN_COUNT      7

extern const TestPin_t g_test_pins[TEST_PIN_COUNT];

/* GPIO 测试模式枚举 */
typedef enum {
    GPIO_MODE_WAVE = 0,    // 自动周期翻转输出 (方波)
    GPIO_MODE_MANUAL,      // 手动控制高/低
    GPIO_MODE_INPUT        // 输入读取模式 (上拉输入)
} GpioTestMode_e;

/* 初始化与控制函数 */
void Board_Test_Init(void);
void Board_Test_Process(void);

void Board_LED_Toggle(void);
void Board_LED_Set(uint8_t on);

void Board_GPIO_Set_Mode(GpioTestMode_e mode);
void Board_GPIO_Set_All(uint8_t high);
void Board_GPIO_Read_All(char *out_buf, uint16_t max_len);

/* 命令解析函数 (用于 USB 或串口终端输入) */
void Board_Process_Command(const char *cmd, char *reply, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_TEST_H__ */
