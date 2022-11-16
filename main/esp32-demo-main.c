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
#include <mdns.h>
#include <nvs_flash.h>
#include <mqtt_client.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_err.h>
#include <esp_spiffs.h>
#include "appdefs.h"
#include "appmqtt.h"
#include "appwebserver.h"
#include "appota.h"
#include "appstate.h"
#include "appwifi.h"
#include "appfilesystem.h"
#include "appsensors.h"
#include "PmodOLEDrgb.h"
#include "appbme280.h"
#include "appultrasonic.h"
#include "appuart.h"

extern const uint8_t server_cert_pem_start[] asm("_binary_caroot_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_caroot_pem_end");

static char* TAG = "app_main";

char ssid[sizeof(((wifi_sta_config_t*) NULL)->ssid)];
char pwd[sizeof(((wifi_sta_config_t*) NULL)->password)];

void app_main(void)
{
  ESP_LOGI(TAG, "Starting app_main");
  // Otherwise nvs_open will fail.
  assert(strlen(STORAGE_NAMESPACE) <= NVS_KEY_NAME_MAX_SIZE - 1);

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  check_partitions();

#if CONFIG_FLASH_FS_ENABLE
  init_internal_fs();
#endif
#if CONFIG_SD_FS_ENABLE
  init_spi_sd_fs();
#if CONFIG_OLEDRGB_ENABLE  
  displaySPIInit();
#endif
#if CONFIG_BME280_SPI_ENABLE
  initBME280SPI();
#endif
  init_external_sd_fs();
#endif
#if CONFIG_OLEDRGB_ENABLE
  displayInit();
#endif
#if CONFIG_BME280_SPI_ENABLE || CONFIG_BME280_I2C_ENABLE
  initBME280();
#endif
#if CONFIG_ULTRASONIC_ENABLE
  ultrasonicInit();
#endif
#if CONFIG_COPROCESSOR_UART_ENABLE
  init_uart();
#endif

  init_network();

  read_wifi_config();
  ESP_LOGI(TAG, "saved ssid=%s", ssid);
  ESP_LOGI(TAG, "saved pwd=%s", pwd);
  establish_ssid_and_pw();

  start_webserver(false);

  init_time();

  setup_mdns();

  setup_mqtt();

  initSensors(0x00);
  
  while (1) {
    vTaskDelay(portMAX_DELAY / portTICK_PERIOD_MS);
  }
}
