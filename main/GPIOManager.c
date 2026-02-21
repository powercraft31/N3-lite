#include "GPIOManager.h"
#include "DeBug.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "ConfigManager.h"
#include "ChargingStationManager.h"
#include "BL0942Meter.h"
#include "PlcModule3121.h"

/**
 * @file    GPIOManager.c
 * @brief   GPIO管理模块实现
 */

static uint8_t InletCurrent = 32;           // 定义默认入户电流为32A
static bool g_setting_mode = false;         // 是否处于设置模式

// 电流档位数组（按顺序排列，用于循环切换）
static const uint8_t current_levels[] = {
    LEVEL_20A,
    LEVEL_25A,
    LEVEL_32A,
    LEVEL_40A,
    LEVEL_50A,
    LEVEL_63A
};
#define CURRENT_LEVEL_COUNT (sizeof(current_levels) / sizeof(current_levels[0]))

/**
 * @brief 初始化GPIO管理模块
 */
int GPIOManager_Init(void)
{
    // 读取或初始化入户电流配置
    ConfigStatus_t status = GetConfigUInt8("InletCurrent", &InletCurrent);
    if (status == CONFIG_ERROR_NOT_FOUND) {
        // Flash中没有该配置，写入默认值
        dPrint(INFO, "InletCurrent not found in Flash, writing default value: %u A\r\n", InletCurrent);
        status = SetConfigUInt8("InletCurrent", InletCurrent);
        if (status != CONFIG_OK) {
            dPrint(DERROR, "Failed to save InletCurrent to Flash\r\n");
            return -1;
        }
        dPrint(INFO, "InletCurrent saved to Flash successfully\r\n");
    } else if (status == CONFIG_OK) {
        // 成功读取到配置值
        dPrint(INFO, "InletCurrent loaded from Flash: %u A\r\n", InletCurrent);
    } else {
        // 读取配置失败
        dPrint(DERROR, "Failed to read InletCurrent from Flash, error code: %d\r\n", status);
        dPrint(WARN, "Using default InletCurrent: %u A\r\n", InletCurrent);
    }

    dPrint(INFO, "GPIO Manager initialized successfully\r\n");
    xTaskCreate(&GPIOManager_Control_Task, "GPIOManager_Task", 4096, NULL, 5, NULL);
    return 0;
}

/**
 * @brief 获取入户电流值
 */
uint8_t GPIOManager_GetInletCurrent(void)
{
    uint8_t InflowCurrent = 0;
    ConfigStatus_t status = GetConfigUInt8("InletCurrent", &InflowCurrent);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "Failed to read InletCurrent from Flash, error code: %d\r\n", status);
        return 0;
    }
    return InflowCurrent;
}

/**
 * @brief 设置入户电流值并保存到Flash
 */
int GPIOManager_SetInletCurrent(uint8_t current)
{
    if (current == 0 || current > 100) {
        dPrint(DERROR, "Invalid inlet current value: %u A (range: 1-100)\r\n", current);
        return -1;
    }

    // 保存到Flash
    ConfigStatus_t status = SetConfigUInt8("InletCurrent", current);
    if (status != CONFIG_OK) {
        dPrint(DERROR, "Failed to save InletCurrent to Flash, error code: %d\r\n", status);
        return -1;
    }

    // 更新内存中的值
    InletCurrent = current;
    dPrint(INFO, "InletCurrent updated to %u A and saved to Flash\r\n", current);
    return 0;
}

/**
 * @brief 获取当前电流档位对应的LED索引
 * @param current 电流值
 * @return LED索引 (0-5)，如果找不到返回-1
 */
static int get_led_index_by_current(uint8_t current)
{
    switch (current)
    {
        case LEVEL_20A: return LED_MONO_1;
        case LEVEL_25A: return LED_MONO_2;
        case LEVEL_32A: return LED_MONO_3;
        case LEVEL_40A: return LED_MONO_4;
        case LEVEL_50A: return LED_MONO_5;
        case LEVEL_63A: return LED_MONO_6;
        default: return -1;
    }
}

/**
 * @brief 获取当前电流档位在数组中的索引
 * @param current 电流值
 * @return 档位索引，如果找不到返回-1
 */
static int get_current_level_index(uint8_t current)
{
    for (int i = 0; i < CURRENT_LEVEL_COUNT; i++)
    {
        if (current_levels[i] == current)
        {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 切换到下一个电流档位
 * @return 新的电流值
 */
static uint8_t switch_to_next_level(void)
{
    int current_index = get_current_level_index(InletCurrent);
    if (current_index < 0)
    {
        // 当前值不在预定义档位中，设置为第一个档位
        current_index = 0;
    }
    else
    {
        // 切换到下一个档位（循环）
        current_index = (current_index + 1) % CURRENT_LEVEL_COUNT;
    }

    InletCurrent = current_levels[current_index];
    return InletCurrent;
}

/**
 * @brief LED闪烁效果
 * @param led_index LED索引
 * @param count 闪烁次数
 */
static void led_blink(led_mono_index_t led_index, int count)
{
    for (int i = 0; i < count; i++)
    {
        led_mono_set(led_index, 0);  // 关闭
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
        led_mono_set(led_index, 1);  // 打开
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
    }
}

/**
 * @brief 更新LED显示状态
 * @param current 当前电流值
 */
static void update_led_display(uint8_t current)
{
    // 先关闭所有LED
    for (int i = 0; i < LED_MONO_COUNT; i++)
    {
        led_mono_set(i, 0);
    }

    // 根据当前电流档位点亮对应的LED
    int led_index = get_led_index_by_current(current);
    if (led_index >= 0)
    {
        led_mono_set(led_index, 1);
        dPrint(INFO, "Current level: %u A - LED%d ON\r\n", current, led_index + 1);
    }
    else
    {
        dPrint(WARN, "Unknown current level: %u A - All LEDs OFF\r\n", current);
    }
}

/**
 * @brief GPIO控制任务 - 根据电流档位控制LED显示，处理按键输入
 * @note LED1-6分别对应电流档位：20A, 25A, 32A, 40A, 50A, 63A
 * @note 按键控制：长按3秒进入/退出设置模式，短按切换档位
 */
void GPIOManager_Control_Task(void *pvParameter)
{
    uint8_t last_current = 0xFF;            // 上一次的电流值（0xFF表示未初始化）
    KeyState_t key_state = KEY_STATE_IDLE;  // 按键状态
    uint32_t key_press_start_time = 0;      // 按键按下开始时间
    bool long_press_handled = false;        // 长按事件是否已处理
    uint32_t last_check_time = 0;           // 上次检查充电桩绑定状态的时间

    //设置为黄色为初始化状态
    led_rgb_set_color(1, 1, 0);
    // 初始显示当前档位
    update_led_display(InletCurrent);
    last_current = InletCurrent;

    while (1)
    {
        uint8_t key_pressed = key_read();  // 读取按键状态（1=按下，0=释放）
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // ==================== 检查系统状态（每1秒检查一次）====================
        if (current_time - last_check_time >= 1000)
        {
            last_check_time = current_time;

            // 1. 检查电表通信异常
            bool meter_error = !bl0942_is_connected();

            // 2. 检查PLC通信异常（通过自身MAC地址判断）
            const char* self_mac = GetSelfMac();
            bool plc_error = (strlen(self_mac) == 0);

            // 3. 检查设备绑定状态
            int station_count = 0;
            ChargingStation *stations = SelectAllChargeStation(&station_count);

            bool has_added_station = false;
            if (stations != NULL && station_count > 0)
            {
                // 遍历所有充电桩，检查是否有已添加的设备
                for (int i = 0; i < station_count; i++)
                {
                    if (stations[i].isAdded == TRUE)
                    {
                        has_added_station = true;
                        break;
                    }
                }
            }

            // 4. 根据优先级设置LED颜色（每次都更新，不做状态比较）
            // 优先级：红灯（异常）> 绿灯（有设备绑定）> 黄灯（无设备绑定）
            if (meter_error || plc_error)
            {
                // 异常 - 红灯
                led_rgb_set_color(1, 0, 0);
                dPrint(DEBUG, "LED: RED (Meter:%d, PLC:%d)\r\n", meter_error, plc_error);
            }
            else if (has_added_station)
            {
                // 有设备绑定且无异常 - 绿灯
                led_rgb_set_color(0, 1, 0);
                //dPrint(DEBUG, "LED: GREEN (Device added)\r\n");
            }
            else
            {
                // 无设备绑定且无异常 - 黄灯
                led_rgb_set_color(1, 1, 0);
                static unsigned char s_count = 0;
                if(s_count%60 == 0)
                {
                    dPrint(DEBUG, "LED: YELLOW (No device)\r\n");
                    s_count++;
                }
                else
                {
                    s_count++;    
                }
            }
        }

        // ==================== 按键状态机 ====================
        switch (key_state)
        {
            case KEY_STATE_IDLE:
                if (key_pressed)
                {
                    // 按键按下
                    key_press_start_time = current_time;
                    key_state = KEY_STATE_PRESSED;
                    long_press_handled = false;
                    dPrint(DEBUG, "Key pressed\r\n");
                }
                break;

            case KEY_STATE_PRESSED:
                if (!key_pressed)
                {
                    // 按键释放 - 短按
                    uint32_t press_duration = current_time - key_press_start_time;
                    if (press_duration >= KEY_SHORT_PRESS_TIME_MS && !long_press_handled)
                    {
                        // 短按：只在设置模式下切换档位
                        if (g_setting_mode)
                        {
                            uint8_t new_current = switch_to_next_level();
                            update_led_display(new_current);
                            dPrint(INFO, "Switched to next level: %u A\r\n", new_current);
                        }
                    }
                    key_state = KEY_STATE_IDLE;
                }
                else if (current_time - key_press_start_time >= KEY_LONG_PRESS_TIME_MS && !long_press_handled)
                {
                    // 长按3秒 - 进入/退出设置模式
                    key_state = KEY_STATE_LONG_PRESS;
                    long_press_handled = true;

                    if (!g_setting_mode)
                    {
                        // 进入设置模式
                        g_setting_mode = true;
                        dPrint(INFO, "Entering setting mode\r\n");

                        // 当前档位LED闪烁
                        int led_index = get_led_index_by_current(InletCurrent);
                        if (led_index >= 0)
                        {
                            led_blink(led_index, LED_BLINK_COUNT);
                        }
                    }
                    else
                    {
                        // 退出设置模式
                        g_setting_mode = false;
                        dPrint(INFO, "Exiting setting mode\r\n");

                        // 当前档位LED闪烁
                        int led_index = get_led_index_by_current(InletCurrent);
                        if (led_index >= 0)
                        {
                            led_blink(led_index, LED_BLINK_COUNT);
                        }

                        // 保存到Flash
                        GPIOManager_SetInletCurrent(InletCurrent);
                    }
                }
                break;

            case KEY_STATE_LONG_PRESS:
                if (!key_pressed)
                {
                    // 长按后释放
                    key_state = KEY_STATE_IDLE;
                }
                break;

            default:
                key_state = KEY_STATE_IDLE;
                break;
        }

        // ==================== 非设置模式下的LED显示更新 ====================
        if (!g_setting_mode && InletCurrent != last_current)
        {
            update_led_display(InletCurrent);
            last_current = InletCurrent;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 每10ms检查一次，提高响应速度
    }
}
