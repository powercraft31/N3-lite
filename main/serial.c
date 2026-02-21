#include "serial.h"
#include <string.h>
#include "DeBug.h"
#include "types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file    serial.c
 * @brief   ESP32 UART串口管理模块实现 - 简化版
 * @details 提供UART初始化和数据收发功能
 */


/**
 * @brief 初始化UART串口
 * @param uart_num UART端口号 (UART_NUM_0/1/2)
 * @param tx_pin TX引脚GPIO编号
 * @param rx_pin RX引脚GPIO编号
 * @param baud_rate 波特率
 * @param data_bits 数据位
 * @param parity 校验位
 * @param stop_bits 停止位
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t serial_init(uart_port_t uart_num, int tx_pin, int rx_pin, uint32_t baud_rate,
                     uart_word_length_t data_bits, uart_parity_t parity, uart_stop_bits_t stop_bits)
{
    if (uart_num >= UART_NUM_MAX) {
        dPrint(DERROR, "Invalid UART number: %d\r\n", uart_num);
        return ESP_FAIL;
    }

    dPrint(INFO, "Initializing UART%d: TX=%d, RX=%d, Baud=%lu, Data=%d, Parity=%d, Stop=%d\r\n",
           uart_num, tx_pin, rx_pin, baud_rate, data_bits, parity, stop_bits);

    // 配置UART参数
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = data_bits,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 1. 先配置UART参数（ESP32官方推荐顺序）
    esp_err_t ret = uart_param_config(uart_num, &uart_config);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to configure UART%d parameters: %s\r\n", uart_num, esp_err_to_name(ret));
        return ret;
    }

    // 2. 设置UART引脚
    ret = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to set UART%d pins: %s\r\n", uart_num, esp_err_to_name(ret));
        return ret;
    }

    // 3. 最后安装UART驱动
    ret = uart_driver_install(uart_num,
                             SERIAL_RX_BUFFER_SIZE,
                             SERIAL_TX_BUFFER_SIZE,
                             0, NULL, 0);
    if (ret != ESP_OK) {
        dPrint(DERROR, "Failed to install UART%d driver: %s\r\n", uart_num, esp_err_to_name(ret));
        return ret;
    }

    // 清空接收缓冲区（避免初始化时的噪声数据）
    uart_flush_input(uart_num);

    dPrint(INFO, "UART%d initialized successfully\r\n", uart_num);
    return ESP_OK;
}

/**
 * @brief 通过UART发送数据
 * @param uart_num UART端口号
 * @param data 发送数据缓冲区指针
 * @param length 发送数据长度
 * @return 实际发送的字节数, -1表示失败
 */
int serial_write(uart_port_t uart_num, const uint8_t *data, size_t length)
{
    if (uart_num >= UART_NUM_MAX) {
        dPrint(DERROR, "Invalid UART number: %d\r\n", uart_num);
        return -1;
    }

    if (data == NULL || length == 0) {
        dPrint(DERROR, "Invalid data pointer or length\r\n");
        return -1;
    }

    int written = uart_write_bytes(uart_num, data, length);
    if (written < 0) {
        dPrint(DERROR, "Failed to write to UART%d\r\n", uart_num);
        return -1;
    }

    return written;
}

/**
 * @brief 通过UART读取数据
 * @param uart_num UART端口号
 * @param buffer 接收数据缓冲区指针
 * @param length 期望读取的字节数
 * @param timeout_ms 超时时间(毫秒), 0表示非阻塞
 * @return 实际读取的字节数, -1表示失败
 */
int serial_read(uart_port_t uart_num, uint8_t *buffer, size_t length, uint32_t timeout_ms)
{
    if (uart_num >= UART_NUM_MAX) {
        dPrint(DERROR, "Invalid UART number: %d\r\n", uart_num);
        return -1;
    }

    if (buffer == NULL || length == 0) {
        dPrint(DERROR, "Invalid buffer pointer or length\r\n");
        return -1;
    }

    // 将毫秒转换为FreeRTOS tick
    TickType_t ticks_to_wait = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);

    int read_bytes = uart_read_bytes(uart_num, buffer, length, ticks_to_wait);
    if (read_bytes < 0) {
        dPrint(DERROR, "Failed to read from UART%d\r\n", uart_num);
        return -1;
    }

    return read_bytes;
}

/**
 * @brief 通过UART发送数据并等待接收响应
 * @param uart_num UART端口号
 * @param txDataBuff 发送数据缓冲区指针
 * @param sendLen 发送数据长度
 * @param rxDataBuff 接收数据缓冲区指针
 * @param recvLen 期望接收的字节数
 * @param timeout 超时时间(毫秒)
 * @return 实际接收的字节数, -1表示错误
 * @note 此函数先发送数据，然后等待接收指定长度的数据
 */
int serial_read_with_result(uart_port_t uart_num, uint8_t* txDataBuff, size_t sendLen,
                            uint8_t* rxDataBuff, size_t recvLen, int timeout)
{
    //dPrint(DEBUG, "serial_read_with_result: 进入函数, uart=%d, sendLen=%zu, recvLen=%zu, timeout=%d\r\n",
    //       uart_num, sendLen, recvLen, timeout);

    if (uart_num >= UART_NUM_MAX) {
        dPrint(DERROR, "Invalid UART number: %d\r\n", uart_num);
        return -1;
    }

    if (txDataBuff == NULL || sendLen == 0) {
        dPrint(DERROR, "Invalid TX buffer or length\r\n");
        return -1;
    }

    if (rxDataBuff == NULL || recvLen == 0) {
        dPrint(DERROR, "Invalid RX buffer or length\r\n");
        return -1;
    }

    //dPrint(DEBUG, "serial_read_with_result: 参数检查通过，准备检查缓冲区\r\n");

    // 调试：检查发送前RX缓冲区中是否有残留数据
    size_t buffered_bytes = 0;
    //dPrint(DEBUG, "serial_read_with_result: 调用 uart_get_buffered_data_len...\r\n");
    uart_get_buffered_data_len(uart_num, &buffered_bytes);
    //dPrint(DEBUG, "serial_read_with_result: uart_get_buffered_data_len 返回: %zu 字节\r\n", buffered_bytes);
    if (buffered_bytes > 0) {
        dPrint(DEBUG, "UART%d has %zu bytes in RX buffer before send, flushing...\r\n",
               uart_num, buffered_bytes);
    }

    // 1. 清空接收缓冲区（避免读取到旧数据）
    //dPrint(DEBUG, "serial_read_with_result: 调用 uart_flush_input...\r\n");
    esp_err_t flush_ret = uart_flush_input(uart_num);
    //dPrint(DEBUG, "serial_read_with_result: uart_flush_input 返回: %s\r\n", esp_err_to_name(flush_ret));

    // 2. 发送数据
    //dPrint(DEBUG, "UART%d sending %zu bytes...\r\n", uart_num, sendLen);
    int written = uart_write_bytes(uart_num, txDataBuff, sendLen);
    //dPrint(DEBUG, "serial_read_with_result: uart_write_bytes 返回: %d\r\n", written);
    if (written != (int)sendLen) {
        dPrint(DERROR, "UART%d write failed: sent %d/%zu bytes\r\n", uart_num, written, sendLen);
        return -1;
    }

    // 等待发送完成
    esp_err_t ret = uart_wait_tx_done(uart_num, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        dPrint(DERROR, "UART%d wait tx done failed: %s\r\n", uart_num, esp_err_to_name(ret));
        return -1;
    }
    //dPrint(DEBUG, "UART%d TX done\r\n", uart_num);

    // 添加小延时，给对端设备响应时间
    vTaskDelay(pdMS_TO_TICKS(10));

    // 调试：检查RX缓冲区中是否有数据
    uart_get_buffered_data_len(uart_num, &buffered_bytes);
    //dPrint(DEBUG, "UART%d RX buffer has %zu bytes before read (expecting %zu)\r\n",
    //       uart_num, buffered_bytes, recvLen);

    // 3. 接收数据
    TickType_t ticks_to_wait = pdMS_TO_TICKS(timeout);
    //dPrint(DEBUG, "UART%d reading up to %zu bytes (timeout=%dms)...\r\n",
    //       uart_num, recvLen, timeout);

    int read_bytes = uart_read_bytes(uart_num, rxDataBuff, recvLen, ticks_to_wait);

    if (read_bytes < 0) {
        dPrint(DERROR, "UART%d read failed: %d\r\n", uart_num, read_bytes);
        return -1;
    }

    if (read_bytes == 0) {
        // 再次检查缓冲区
        uart_get_buffered_data_len(uart_num, &buffered_bytes);
        dPrint(WARN, "UART%d read timeout: no data received (buffer has %zu bytes)\r\n",
               uart_num, buffered_bytes);

        // 检查UART错误状态
        uint32_t uart_status = 0;
        // 注意：这里可能需要使用底层寄存器读取来检查错误
        dPrint(WARN, "UART%d status: 0x%lx\r\n", uart_num, uart_status);

        return 0;
    }

    if (read_bytes != (int)recvLen) {
        dPrint(WARN, "UART%d read incomplete: expected %zu, got %d bytes\r\n",
               uart_num, recvLen, read_bytes);
    } else {
        //dPrint(DEBUG, "UART%d successfully read %d bytes\r\n", uart_num, read_bytes);
    }

    return read_bytes;
}
