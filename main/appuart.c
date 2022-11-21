#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <esp_event.h>
#include <esp_ota_ops.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_https_server.h>
#include <esp_http_client.h>
#include <esp_sntp.h>
#include <esp_random.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include <mdns.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include "appdefs.h"
#include "appmqtt.h"
#include "appwebserver.h"
#include "appota.h"
#include "appuart.h"

static char* TAG = "appuart";

static const int RX_BUF_SIZE = 1024;

#define APP_UART UART_NUM_2
#define TXD_PIN 17
#define RXD_PIN 16

#ifndef UART_SCLK_DEFAULT 
#define UART_SCLK_DEFAULT 4
#endif

static void configureUart(void);

static void configureUart(void) 
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(APP_UART, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(APP_UART, &uart_config);
    uart_set_pin(APP_UART, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

}

int sendData(const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(APP_UART, data, len);
    ESP_LOGI(TAG, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(APP_UART, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
	    postSensorsReport((char *)data);
        }
    }
    free(data);
}

void init_uart(void)
{	
    configureUart();
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
}
