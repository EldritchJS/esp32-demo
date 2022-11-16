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
#include "appsensors.h"
#if CONFIG_BME280_I2C_ENABLE
#include "appbme280.h" 
#endif

static TimerHandle_t s_tmr;  /* handle of acquisition timer */
static int s_sensors_state = SENSORS_STATE_OFF; /* Sensors state */
static QueueHandle_t s_sensors_task_queue = NULL;
static TaskHandle_t s_sensors_task_handle = NULL;
#define SENSORS_TAG "appsensors"

static void sensors_task_handler(void *arg)
{
  sensors_msg_t msg;

  for (;;) {
      /* receive message from work queue and handle it */
      if (pdTRUE == xQueueReceive(s_sensors_task_queue, &msg, (TickType_t)portMAX_DELAY)) {
          ESP_LOGD(SENSORS_TAG, "%s, signal: 0x%x", __func__, msg.sig);

        switch (msg.sig) {
        case SENSORS_START_SIG:
          // Start sensors
	  // Start timer
	  if(s_sensors_state == SENSORS_STATE_OFF) {
	    s_sensors_state = SENSORS_STATE_ON;
	    xTimerStart(s_tmr, portMAX_DELAY);
	    ESP_LOGI(SENSORS_TAG, "Starting sensors");
#if CONFIG_BME280_I2C_ENABLE
	    initBME280();
#endif
	  }
	  else
	  {
	    ESP_LOGI(SENSORS_TAG, "Sensors already on");
	  }
          break;
        case SENSORS_STOP_SIG:
          // Stop sensors
	  if(s_sensors_state == SENSORS_STATE_ON) {
	    s_sensors_state = SENSORS_STATE_OFF;
	    xTimerStop(s_tmr, portMAX_DELAY);
	    ESP_LOGI(SENSORS_TAG, "Stopping sensors");
	  }
	  else
	  {
	    ESP_LOGI(SENSORS_TAG, "Sensors already off");
	  }
          break;
        case SENSORS_ACQUIRE_SIG:
	  if(s_sensors_state == SENSORS_STATE_ON) {
	    ESP_LOGI(SENSORS_TAG, "Acquiring sensors sample");

	  }
	  else {
	    ESP_LOGI(SENSORS_TAG, "Sensors off, cannot acquire sample");
	  }
	  break;
        default:
          ESP_LOGW(SENSORS_TAG, "%s, unhandled signal: %d", __func__, msg.sig);
          break;
      }
      if (msg.param) {
          free(msg.param);
      }
    }
  }
}

bool startSensors(void)
{
  sensors_msg_t msg;
  memset(&msg, 0, sizeof(sensors_msg_t));

  msg.sig = SENSORS_START_SIG;

  if (pdTRUE != xQueueSend(s_sensors_task_queue, &msg, 10 / portTICK_PERIOD_MS)) {
    ESP_LOGE(SENSORS_TAG, "%s xQueue send failed", __func__);
    return false;
  }

  return true;
}

bool stopSensors(void)
{
  sensors_msg_t msg;
  memset(&msg, 0, sizeof(sensors_msg_t));

  msg.sig = SENSORS_STOP_SIG;

  if (pdTRUE != xQueueSend(s_sensors_task_queue, &msg, 10 / portTICK_PERIOD_MS)) {
    ESP_LOGE(SENSORS_TAG, "%s xQueue send failed", __func__);
    return false;
  }

  return true;
}

static void sensors_acquisition_cb(TimerHandle_t arg);
static void sensors_acquisition_cb(TimerHandle_t arg)
{
  sensors_msg_t msg;
  memset(&msg, 0, sizeof(sensors_msg_t));
  
  msg.sig = SENSORS_ACQUIRE_SIG;

  if (pdTRUE != xQueueSend(s_sensors_task_queue, &msg, 10 / portTICK_PERIOD_MS)) {
    ESP_LOGE(SENSORS_TAG, "%s xQueue send failed", __func__);
  }
}

void initSensors(uint8_t settings)
{
  int tmr_id = 0;
  s_tmr = xTimerCreate("sensorsTmr", (5000 / portTICK_PERIOD_MS), pdTRUE, (void *) &tmr_id, sensors_acquisition_cb); 
  s_sensors_task_queue = xQueueCreate(10, sizeof(sensors_msg_t));
  xTaskCreate(sensors_task_handler, "SensorsTask", 2048, NULL, configMAX_PRIORITIES - 3, &s_sensors_task_handle);
}




