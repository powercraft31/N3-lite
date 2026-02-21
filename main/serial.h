#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdint.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"

/**
 * @file    serial.h
 * @brief   ESP32 UART串口管理模块 - 简化版
 * @details 提供UART初始化和数据收发功能
 */

// UART端口定义
#define SERIAL_UART0    UART_NUM_0      // UART0 (烧录/DEBUG)
#define SERIAL_UART1    UART_NUM_1      // UART1 (PLC通信)
#define SERIAL_UART2    UART_NUM_2      // UART2 (BL0942电能计量芯片通信)

// 项目GPIO引脚定义
// UART0 - 烧录/Debug (默认引脚)
#define SERIAL_UART0_TX_PIN     GPIO_NUM_1      // Pin35/U0TXD
#define SERIAL_UART0_RX_PIN     GPIO_NUM_3      // Pin34/U0RXD

// UART1 - PLC模块通信
#define SERIAL_UART1_TX_PIN     GPIO_NUM_25     // Pin10/GPIO25
#define SERIAL_UART1_RX_PIN     GPIO_NUM_26     // Pin11/GPIO26

// UART2 - BL0942电能计量芯片通信
#define SERIAL_UART2_TX_PIN     GPIO_NUM_14     // Pin13/GPIO14
#define SERIAL_UART2_RX_PIN     GPIO_NUM_35     // Pin7/GPIO35

// 缓冲区大小定义
#define SERIAL_RX_BUFFER_SIZE       1024    // 接收缓冲区大小
#define SERIAL_TX_BUFFER_SIZE       1024    // 发送缓冲区大小

/**
 * @brief 初始化UART串口
 * @param uart_num UART端口号 (UART_NUM_0/1/2)
 * @param tx_pin TX引脚GPIO编号
 * @param rx_pin RX引脚GPIO编号
 * @param baud_rate 波特率
 * @param data_bits 数据位 (UART_DATA_5_BITS ~ UART_DATA_8_BITS)
 * @param parity 校验位 (UART_PARITY_DISABLE/UART_PARITY_EVEN/UART_PARITY_ODD)
 * @param stop_bits 停止位 (UART_STOP_BITS_1/UART_STOP_BITS_1_5/UART_STOP_BITS_2)
 * @return ESP_OK 成功, ESP_FAIL 失败
 */
esp_err_t serial_init(uart_port_t uart_num, int tx_pin, int rx_pin, uint32_t baud_rate,
                     uart_word_length_t data_bits, uart_parity_t parity, uart_stop_bits_t stop_bits);

/**
 * @brief 通过UART发送数据
 * @param uart_num UART端口号
 * @param data 发送数据缓冲区指针
 * @param length 发送数据长度
 * @return 实际发送的字节数, -1表示失败
 */
int serial_write(uart_port_t uart_num, const uint8_t *data, size_t length);

/**
 * @brief 通过UART读取数据
 * @param uart_num UART端口号
 * @param buffer 接收数据缓冲区指针
 * @param length 期望读取的字节数
 * @param timeout_ms 超时时间(毫秒), 0表示非阻塞
 * @return 实际读取的字节数, -1表示失败
 */
int serial_read(uart_port_t uart_num, uint8_t *buffer, size_t length, uint32_t timeout_ms);

/**
 * @brief 通过UART发送数据并等待接收响应（同步收发）
 * @param uart_num UART端口号
 * @param txDataBuff 发送数据缓冲区指针
 * @param sendLen 发送数据长度
 * @param rxDataBuff 接收数据缓冲区指针
 * @param recvLen 期望接收的字节数
 * @param timeout 超时时间(毫秒)
 * @return 实际接收的字节数, -1表示错误
 */
int serial_read_with_result(uart_port_t uart_num, uint8_t* txDataBuff, size_t sendLen,
                            uint8_t* rxDataBuff, size_t recvLen, int timeout);

#endif // __SERIAL_H__
