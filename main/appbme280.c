#include <string.h>

#include "freertos/FreeRTOSConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
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
#include "appfilesystem.h"
#include "appdefs.h"
#include "appmqtt.h"
#include "appota.h"
#include "appstate.h"
#include "frozen.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
//#include "appbme280.h"

#define BME_TAG "appbe280"
#if CONFIG_BME280_SPI_ENABLE
#include "bme280.h"

static struct bme280_dev dev;

#define BME280_PIN_NUM_CS 1 

static spi_device_handle_t hSPI;

static struct bme280_data comp_data;

struct bme280_data * getBME280Data(void)
{
  bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
  return &comp_data; 
}

void user_delay_ms(uint32_t period)
{
    /*
     * Return control or wait,
     * for a period amount of milliseconds
     */
    vTaskDelay(period / portTICK_PERIOD_MS);
}

int8_t user_spi_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{rasonic_sensor_t sensor = {
        .trigger_pin = TRIGGER_GPIO,
        .echo_pin = ECHO_GPIO
    };


   spi_transaction_t t = {
	   .length = 8,
	   .tx_data = {reg_addr},
	   .flags = SPI_TRANS_USE_TXDATA,
   };
   spi_device_polling_transmit(hSPI, &t);
   spi_transaction_t td = {
      .tx_buffer = NULL,
      .length = 8*len,
      .rxlength = 8*len,
      .rx_buffer = reg_data,
   };
   spi_device_polling_transmit(hSPI, &td);

  return 0;
}

int8_t user_spi_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{

   spi_transaction_t t = {
      .length = 8,
      .tx_data = {reg_addr},
      .flags = SPI_TRANS_USE_TXDATA,
   };
   spi_device_polling_transmit(hSPI, &t);

   spi_transaction_t td = {
      .length = 8*len,
      .tx_buffer = reg_data,
   };
   spi_device_polling_transmit(hSPI, &td);

  return 0;
}

int8_t setBME280Mode(void)
{
    int8_t rslt;
    uint8_t settings_sel;

    /* Recommended mode of operation: Indoor navigation */
    dev.settings.osr_h = BME280_OVERSAMPLING_1X;
    dev.settings.osr_p = BME280_OVERSAMPLING_16X;
    dev.settings.osr_t = BME280_OVERSAMPLING_2X;
    dev.settings.filter = BME280_FILTER_COEFF_16;
    dev.settings.standby_time = BME280_STANDBY_TIME_62_5_MS;

    settings_sel = BME280_OSR_PRESS_SEL;
    settings_sel |= BME280_OSR_TEMP_SEL;
    settings_sel |= BME280_OSR_HUM_SEL;
    settings_sel |= BME280_STANDBY_SEL;
    settings_sel |= BME280_FILTER_SEL;
    rslt = bme280_set_sensor_settings(settings_sel, &dev);
    rslt = bme280_set_sensor_mode(BME280_NORMAL_MODE, &dev);

    return rslt;
}

void initBME280SPI(void)
{
  static bool isInited = false;

  if(isInited == true)
    return;

  spi_device_interface_config_t devcfg={
      .clock_speed_hz=SPI_MASTER_FREQ_10M,     
      .mode=0,                                
      .spics_io_num=BME280_PIN_NUM_CS,            
      .queue_size=1,                         
  };
  spi_bus_add_device(SPI2_HOST, &devcfg, &hSPI);

  isInited = true;
}

void initBME280(void)
{
  int8_t rslt = BME280_OK;

  initBME280SPI();
  dev.dev_id = BME280_PIN_NUM_CS;
  dev.intf = BME280_SPI_INTF;
  dev.read = user_spi_read;
  dev.write = user_spi_write;
  dev.delay_ms = user_delay_ms;

  rslt = bme280_init(&dev);
  if (rslt != BME280_OK)
  {
    ESP_LOGI(BME_TAG, "BME280 INIT FAIL rslt:%d\n", rslt);
    return;
  }
  setBME280Mode();  
}
#elif CONFIG_BME280_I2C_ENABLE
#include <esp_system.h>
#include <bmp280.h>

static bmp280_t dev;

void getBME280Data(float *temperature, float *pressure, float *humidity)
{
#if 1 
  if (bmp280_read_float(&dev, temperature, pressure, humidity) != ESP_OK)
  {
    ESP_LOGI(BME_TAG,"Temperature/pressure/humidity reading failed");
  }
#endif
}

esp_err_t initBME280I2C(void)
{
  int i2c_master_port = 0;
  i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = CONFIG_BME280_I2C_MASTER_SDA,         // select GPIO specific to your project
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = CONFIG_BME280_I2C_MASTER_SCL,         // select GPIO specific to your project
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,  // select frequency specific to your project
  };
  i2c_param_config(i2c_master_port, &conf);
  return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

void initBME280(void)
{
    ESP_ERROR_CHECK(i2cdev_init());    
    ESP_LOGI(BME_TAG, "Initializing BME PARAMS");
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    memset(&dev, 0, sizeof(bmp280_t));

    ESP_LOGI(BME_TAG, "About to init the BME280 desc with SDA %d and SCL %d", CONFIG_BME280_I2C_MASTER_SDA, CONFIG_BME280_I2C_MASTER_SCL);
    ESP_ERROR_CHECK(bmp280_init_desc(&dev, BMP280_I2C_ADDRESS_0, 0, CONFIG_BME280_I2C_MASTER_SDA, CONFIG_BME280_I2C_MASTER_SCL));
    ESP_LOGI(BME_TAG, "About to init the BME280");
    ESP_ERROR_CHECK(bmp280_init(&dev, &params));

    bool bme280p = dev.id == BME280_CHIP_ID;
    ESP_LOGI(BME_TAG, "BMP280: found %s\n", bme280p ? "BME280" : "BMP280");
}
#endif


