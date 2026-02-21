#include "led.h"
#include "DeBug.h"

/**
 * @file    led.c
 * @brief   GPIO引脚配置和管理实现
 */

// 单色LED引脚数组
static const gpio_num_t led_mono_pins[LED_MONO_COUNT] = {
    LED_MONO_1_PIN,
    LED_MONO_2_PIN,
    LED_MONO_3_PIN,
    LED_MONO_4_PIN,
    LED_MONO_5_PIN,
    LED_MONO_6_PIN
};

/**
 * @brief 初始化所有GPIO
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t gpio_init(void)
{
    dPrint(INFO, "Initializing GPIOs...\r\n");

    // ==================== 配置单色LED (输出) ====================
    gpio_config_t led_mono_conf = {
        .pin_bit_mask = (1ULL << LED_MONO_1_PIN) |
                       (1ULL << LED_MONO_2_PIN) |
                       (1ULL << LED_MONO_3_PIN) |
                       (1ULL << LED_MONO_4_PIN) |
                       (1ULL << LED_MONO_5_PIN) |
                       (1ULL << LED_MONO_6_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&led_mono_conf);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to configure mono LED GPIOs: %s\r\n", esp_err_to_name(ret));
        return ret;
    }

    // 初始化所有单色LED为关闭状态
    for (int i = 0; i < LED_MONO_COUNT; i++) {
        gpio_set_level(led_mono_pins[i], 0);
    }
    dPrint(INFO, "Mono LED GPIOs initialized (GPIO17,18,19,21,22,23)\r\n");

    // ==================== 配置RGB LED (输出) ====================
    gpio_config_t led_rgb_conf = {
        .pin_bit_mask = (1ULL << LED_RGB_R_PIN) |
                       (1ULL << LED_RGB_G_PIN) |
                       (1ULL << LED_RGB_B_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ret = gpio_config(&led_rgb_conf);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to configure RGB LED GPIOs: %s\r\n", esp_err_to_name(ret));
        return ret;
    }

    // 初始化RGB LED为关闭状态
    gpio_set_level(LED_RGB_R_PIN, 0);
    gpio_set_level(LED_RGB_G_PIN, 0);
    gpio_set_level(LED_RGB_B_PIN, 0);
    dPrint(INFO, "RGB LED GPIOs initialized (GPIO32,33,27)\r\n");

    gpio_config_t key_conf = {
        .pin_bit_mask = (1ULL << KEY_CURRENT_SET_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用内部上拉，使用外部硬件上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ret = gpio_config(&key_conf);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to configure key GPIO: %s\r\n", esp_err_to_name(ret));
        return ret;
    }
    dPrint(INFO, "Key GPIO initialized (GPIO4) with external pull-up\r\n");

    dPrint(INFO, "All GPIOs initialized successfully\r\n");
    return ESP_OK;
}

/**
 * @brief 设置单色LED状态
 * @param led_index LED索引 (0~5)
 * @param state 0=关闭, 1=打开
 */
void led_mono_set(led_mono_index_t led_index, uint8_t state)
{
    if (led_index >= LED_MONO_COUNT) {
        dPrint(DERROR, "Invalid LED index: %d\r\n", led_index);
        return;
    }

    gpio_set_level(led_mono_pins[led_index], state ? 1 : 0);
}

/**
 * @brief 设置RGB LED颜色
 * @param red 红色通道 (0=关, 1=开)
 * @param green 绿色通道 (0=关, 1=开)
 * @param blue 蓝色通道 (0=关, 1=开)
 */
void led_rgb_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    gpio_set_level(LED_RGB_R_PIN, red ? 1 : 0);
    gpio_set_level(LED_RGB_G_PIN, green ? 1 : 0);
    gpio_set_level(LED_RGB_B_PIN, blue ? 1 : 0);
}

/**
 * @brief 读取按键状态
 * @return 0=释放（高电平）, 1=按下（低电平）
 */
uint8_t key_read(void)
{
    // 按键按下时GPIO为低电平
    return (gpio_get_level(KEY_CURRENT_SET_PIN) == 0) ? 1 : 0;
}
