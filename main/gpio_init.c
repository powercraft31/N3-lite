#include "gpio_init.h"
#include "PortDeBugService.h"

esp_err_t gpio_init_all()
{
    dPrint(INFO, "Initializing GPIOs...\r\n");
    ESP_ERROR_CHECK(gpio_init());

    //UART0 init 烧录Debuug
    //ESP_ERROR_CHECK(serial_init(SERIAL_UART0, SERIAL_UART0_TX_PIN, SERIAL_UART0_RX_PIN, 115200, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1));
    xTaskCreate(&uart0_interaction_task, "uart0_debug_task", 4096, NULL, 5, NULL);

    //UART1 init PLC通信
    ESP_ERROR_CHECK(serial_init(SERIAL_UART1, SERIAL_UART1_TX_PIN, SERIAL_UART1_RX_PIN, 115200, UART_DATA_8_BITS, UART_PARITY_EVEN, UART_STOP_BITS_1));

    //UART2 init BL0942电能计量芯片通信
    ESP_ERROR_CHECK(serial_init(SERIAL_UART2, SERIAL_UART2_TX_PIN, SERIAL_UART2_RX_PIN, 4800, UART_DATA_8_BITS, UART_PARITY_DISABLE, UART_STOP_BITS_1));

    dPrint(INFO, "All GPIOs initialized successfully\r\n");
    return ESP_OK;
}

