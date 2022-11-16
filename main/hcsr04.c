#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
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
#include "appfilesystem.h"
#include "appdefs.h"
#include "appmqtt.h"
#include "appota.h"
#include "appstate.h"
#include "frozen.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "appsensors.h"
#include "hcsr04.h"


#define HCSR04_TAG "hcsr04"

#define TRIGGER_PIN GPIO_NUM_5
#define ECHO_PIN    GPIO_NUM_6

void hcsr04Init(void) 
{
  gpio_set_direction(TRIGGER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en(ECHO_PIN);
}

long hcsr04GetEcho(void) 
{
  uint32_t timeout = 1000000L;
  uint64_t check_ticks;
  int64_t pulse_start;

  // Make sure that trigger pin is LOW.
  gpio_set_level(TRIGGER_PIN, 0);
  int64_t t1 = esp_timer_get_time();
  while((esp_timer_get_time()-t1)<5);

  // Hold trigger for 10 microseconds, which is signal for sensor to measure distance.
  gpio_set_level(TRIGGER_PIN, 1);
  t1 = esp_timer_get_time();
  while((esp_timer_get_time()-t1)<10);
  gpio_set_level(TRIGGER_PIN, 0);
  
  // wait for any previous pulse to end
  pulse_start = esp_timer_get_time();
  check_ticks = UINT64_MAX;
  while (1 == gpio_get_level(ECHO_PIN)) 
  {
    --check_ticks;
    if (check_ticks == 0) 
    {
      if ((esp_timer_get_time() - pulse_start) > timeout) 
      {
        ESP_LOGI(HCSR04_TAG, "Error awaiting previous pulse to end");  
        return -1;
      }
      check_ticks = UINT64_MAX;
    }
  }

  // wait for the pulse to start
  pulse_start = esp_timer_get_time();
  check_ticks = UINT64_MAX;
  while (1 != gpio_get_level(ECHO_PIN)) 
  {
    --check_ticks;
    if (check_ticks == 0) {
      if ((esp_timer_get_time() - pulse_start) > timeout) {
        ESP_LOGI(HCSR04_TAG, "Error awaiting pulse to start");  
        return -1;
      }
      check_ticks = UINT64_MAX;
    }
  }
  
  pulse_start = esp_timer_get_time();

  // wait for the pulse to stop
  check_ticks = UINT64_MAX;
  while (1 == gpio_get_level(ECHO_PIN))
  {
    --check_ticks;
    if (check_ticks == 0) {
      if ((esp_timer_get_time() - pulse_start) > timeout) {
        ESP_LOGI(HCSR04_TAG, "Error awaiting pulse to stop");  
        return -1;
      }
      check_ticks = UINT64_MAX;
    }
  };
  
  return (esp_timer_get_time() - pulse_start);
}

float hcsr04GetDistance() 
{ 
  return hcsr04GetDistanceTemp(DEFAULT_TEMPERATURE);
}

float hcsr04GetDistanceTemp(float temperature) 
{ 
  long duration = hcsr04GetEcho();
  if (duration == -1) return NAN;
    
  float sound_speed = 0.3313 + 0.000606 * temperature; // Cair ≈ (331.3 + 0.606 ⋅ ϑ) m/s
  float distance = (duration / 2) * sound_speed;
  if (distance > 4003) return NAN;
  if (distance < 19.7) return NAN;
  return distance;
}

float hcsr04GetDistanceAvg(int attempts_count, int attempts_delay) 
{
  return hcsr04GetDistanceAvgTemp(attempts_count, attempts_delay, DEFAULT_TEMPERATURE);
}

float hcsr04GetDistanceAvgTemp(int attempts_count, int attempts_delay, float temperature) 
{
  int not_nan_count = 0;
  float result = 0;
  if (attempts_delay == -1) attempts_delay = DEFAULT_AVG_ATTEMPTS_DELAY;
  for (int i = 0; i < attempts_count; ++i) 
  {
    if (i > 0) vTaskDelay(attempts_delay / portTICK_PERIOD_MS);
    float m = hcsr04GetDistanceTemp(temperature);
    if (!isnan(m)) 
    {
      result += m;
      ++not_nan_count;
    }
  }
  return (not_nan_count == 0 ? NAN : (result / not_nan_count));
}


