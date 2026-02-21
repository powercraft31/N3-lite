#ifndef __LED_H__
#define __LED_H__

#include "driver/gpio.h"
#include "types.h"
#include <stdint.h>

/**
 * @file    led.h
 * @brief   LED控制
 * @details 管理LED控制和按键输入
 */

// ==================== 单色LED控制 ====================
#define LED_MONO_1_PIN      GPIO_NUM_13     // Pin16/GPIO13
#define LED_MONO_2_PIN      GPIO_NUM_21     // Pin33/GPIO21
#define LED_MONO_3_PIN      GPIO_NUM_18     // Pin30/GPIO18
#define LED_MONO_4_PIN      GPIO_NUM_22     // Pin36/GPIO22
#define LED_MONO_5_PIN      GPIO_NUM_19     // Pin31/GPIO19
#define LED_MONO_6_PIN      GPIO_NUM_23     // Pin37/GPIO23

// LED索引定义
typedef enum {
    LED_MONO_1 = 0,
    LED_MONO_2 = 1,
    LED_MONO_3 = 2,
    LED_MONO_4 = 3,
    LED_MONO_5 = 4,
    LED_MONO_6 = 5,
    LED_MONO_COUNT = 6
} led_mono_index_t;

// ==================== RGB LED控制 ====================
#define LED_RGB_R_PIN       GPIO_NUM_33     // Pin8/GPIO33  - 红色
#define LED_RGB_G_PIN       GPIO_NUM_32     // Pin9/GPIO32  - 绿色
#define LED_RGB_B_PIN       GPIO_NUM_27     // Pin12/GPIO27 - 蓝色

// ==================== 按键输入 ====================
#define KEY_CURRENT_SET_PIN GPIO_NUM_4      // Pin26/GPIO4 - 电流档位设置按键

// ==================== 函数声明 ====================

/**
 * @brief 初始化所有GPIO
 * @return RTN_SUCCESS 成功, RTN_FAIL 失败
 */
int gpio_init(void);

/**
 * @brief 设置单色LED状态
 * @param led_index LED索引 (LED_MONO_1 ~ LED_MONO_6)
 * @param state 0=关闭, 1=打开
 */
void led_mono_set(led_mono_index_t led_index, uint8_t state);

/**
 * @brief 设置RGB LED颜色
 * @param red 红色通道 (0=关, 1=开)
 * @param green 绿色通道 (0=关, 1=开)
 * @param blue 蓝色通道 (0=关, 1=开)
 */
void led_rgb_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 读取按键状态
 * @return 0=释放（高电平）, 1=按下（低电平）
 */
uint8_t key_read(void);

// ==================== RGB颜色预设宏 ====================
#define RGB_COLOR_OFF       led_rgb_set_color(0, 0, 0)  // 关闭
#define RGB_COLOR_RED       led_rgb_set_color(1, 0, 0)  // 红色
#define RGB_COLOR_GREEN     led_rgb_set_color(0, 1, 0)  // 绿色
#define RGB_COLOR_BLUE      led_rgb_set_color(0, 0, 1)  // 蓝色
#define RGB_COLOR_YELLOW    led_rgb_set_color(1, 1, 0)  // 黄色
#define RGB_COLOR_CYAN      led_rgb_set_color(0, 1, 1)  // 青色
#define RGB_COLOR_MAGENTA   led_rgb_set_color(1, 0, 1)  // 品红
#define RGB_COLOR_WHITE     led_rgb_set_color(1, 1, 1)  // 白色

#endif // __LED_H__
