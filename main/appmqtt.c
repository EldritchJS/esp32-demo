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
#include "appfilesystem.h"
#include "appdefs.h"
#include "appmqtt.h"
#include "appota.h"
#include "appstate.h"
#include "frozen.h"
#include "appsensors.h"
#include "PmodOLEDrgb.h"
#include "appultrasonic.h"
#include "appbme280.h"
#include "appuart.h"

static bool mqtt_running;
static bool mqtt_connected;
static char* update;
static size_t update_len;
static char* command_topic;
static size_t command_topic_len;
static char* heartbeat_topic;
static size_t heartbeat_topic_len;
static char* sensors_topic;
static size_t sensors_topic_len;

static char* TAG = "appmqtt";

static TimerHandle_t s_tmr;
static unsigned cycle = 0;

static esp_mqtt_client_handle_t client = NULL;

static void appmqtt_heartbeat_cb(TimerHandle_t arg);
static void appmqtt_heartbeat_cb(TimerHandle_t arg)
{
  if(mqtt_connected)
  {
    char heartbeat[255];
    ++cycle;
    ssize_t n = snprintf(heartbeat, sizeof(heartbeat), "{\"host\": \"%s\", \"connect seconds\": %u}", getHostname(), cycle*5);
    esp_mqtt_client_publish(client, heartbeat_topic, heartbeat, n, 0, 0);
#if CONFIG_OLEDRGB_ENABLE
    displayPutString(""); // EldritchJS Check this, would prefer to displayClear()
    displaySetCursor(0, 0);
    sprintf(heartbeat, "Host: %s", getHostname());
    displayPutString(heartbeat);
    displaySetCursor(0, 3);
    sprintf(heartbeat, "Connect seconds: %u", cycle*5);
    displayPutString(heartbeat);

#endif
  }
}

void postSensorsReport(char *report)
{
    esp_mqtt_client_publish(client, sensors_topic, report, strlen(report), 0, 0);
}

bool isMQTTRunning(void)
{
  return mqtt_running;
}

bool isMQTTConnected(void)
{
  return mqtt_connected;
}



static void getFileFromURL(char *str, size_t len)
{
  char* url = NULL;
  FILE* fid = NULL;
  const int read_buffer_size = 1024;
  char* read_buffer = NULL;

  if((json_scanf(str, len, "{url: %Q}", &url))!=1)
  {
    ESP_LOGE(TAG, "No URL field");
    return;
  }

  esp_http_client_config_t config = {
    .url = url,
    .cert_pem = (char *) server_cert_pem_start,
    .timeout_ms = CONFIG_ESP32_DEMO_OTA_RECV_TIMEOUT,
#ifdef CONFIG_ESP32_DEMO_SKIP_COMMON_NAME_CHECK
    .skip_cert_common_name_check = CONFIG_ESP32_DEMO_SKIP_COMMON_NAME_CHECK,
#else
    .skip_cert_common_name_check = 0,
#endif
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL) {
    ESP_LOGE(TAG, "cannot create HTTP client to download file");
    goto cleanup;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "cannot connect to server to download file: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    goto cleanup;
  }
  esp_http_client_fetch_headers(client);

  read_buffer = malloc(read_buffer_size);

  fid = fopen(BITSTREAM_PACKAGE_FILENAME, "wb");
  if (fid == NULL) {
      ESP_LOGE(TAG, "Failed to open file for writing");
      goto cleanup;
  }  
  // Read the data from the server and store it in the file 
  int binary_file_length = 0;
  int nread;

  while (true) {
    nread = esp_http_client_read(client, read_buffer, read_buffer_size);
    if (nread < 0) {
      ESP_LOGE(TAG, "cannot read file");
      goto fail;
    }

    if (nread == 0)
      break;
	
    fwrite(read_buffer, sizeof(char), nread, fid);
    binary_file_length += nread;
    ESP_LOGD(TAG, "Written image length %d", binary_file_length);
  }

cleanup:
  if(fid!=NULL)
    fclose(fid);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  if(read_buffer!=NULL)
    free(read_buffer);
  free(url);
  return;
fail:
  if(fid!=NULL)
    fclose(fid);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  if(read_buffer!=NULL)
    free(read_buffer);
  free(url);
  ESP_LOGE(TAG, "Exiting task due to fatal error...");
  (void) vTaskDelete(NULL);
  while (true)
    ;
}

static void commandInterpreter(char* str, size_t len)
{
  char *command = NULL;

  if((json_scanf(str, len, "{command: %Q}", &command))==1)
  {
    if(strcmp(command, "GetVersion")==0)
    {
      ESP_LOGI(TAG, "ESP Demo Ver: %s", APP_VERSION); 
    }
    else if(strcmp(command, "GetReport")==0)
    {
	sendData("ReqReport");
    }
#if CONFIG_SD_FS_ENABLE
    else if(strcmp(command, "GetFileFromURL")==0)
    {
      getFileFromURL(str, len);	    
    }
    else if(strcmp(command, "ListSDCardFiles")==0)
    {
      list_dir(CONFIG_SD_FS_MOUNT_POINT);	    
    }
#endif
#if CONFIG_FLASH_FS_ENABLE
    else if(strcmp(command, "ListInternalFiles")==0)
    {
      list_dir(CONFIG_FLASH_FS_MOUNT_POINT);	    
    }   
#endif
#if CONFIG_SD_FS_ENABLE || CONFIG_FLASH_FS_ENABLE
    else if(strcmp(command, "RemoveFile")==0)
    {
      char *filename = NULL;
      if((json_scanf(str, len, "{filename: %Q}", &filename))==1)
      {
        remove_file(filename);
      }	      
      if(filename != NULL)
        free(filename);
    }
#endif
    else if(strcmp(command, "StartSensors")==0)
    {
      startSensors();	    
    }
    else if(strcmp(command, "StopSensors")==0)
    {
      stopSensors();
    }
#if CONFIG_OLEDRGB_ENABLE    
    else if(strcmp(command, "DisplayClear")==0)
    {
      displayClear();
    }
    else if(strcmp(command, "DisplayString")==0)
    {
	char *value = NULL;
	int x = 1;
	int y = 1;

        if((json_scanf(str, len, "{value: %Q, x: %d, y: %d}", &value, &x, &y))>=1)
	{
	  displayClear();
          displaySetCursor(x, y);
	  displayPutString(value);
	}
	else
	{
	  ESP_LOGI(TAG, "No value field");
	}
	if(str != NULL)
	  free(value);
    }
#endif
#if CONFIG_ULTRASONIC_ENABLE
    else if(strcmp(command, "GetUltrasonicDistance")==0)
    {
      float distance=0;
      esp_err_t res = ultrasonicMeasure(&distance);
      ESP_LOGI(TAG, "Ultrasonic Distance: %f", distance);
    }
#endif
#if CONFIG_BME280_SPI_ENABLE
    else if(strcmp(command, "GetBME280Data")==0)
    {
      float temp, press, hum;
      struct bme280_data *bdata;
      bdata = getBME280Data();
      temp = bdata->temperature;
      press = 0.01 * bdata->pressure;
      hum = bdata->humidity;
      ESP_LOGI(TAG, "%0.2lf deg C, %0.2lf hPa, %0.2lf", temp, press, hum);
    }
#elif CONFIG_BME280_I2C_ENABLE
    else if(strcmp(command, "GetBME280Data")==0)
    {
      float temperature, pressure, humidity;
      getBME280Data(&temperature, &pressure, &humidity);
      ESP_LOGI(TAG,"In MQTT: Pressure: %.2f Pa, Temperature: %.2f C, Humidity: %.2f %%", pressure, temperature, humidity);
    }
#endif    
    else
    {
      ESP_LOGI(TAG, "Unknown command: %s", command);	    
    } 
  }
  else
  {
    ESP_LOGI(TAG, "No command field");	  
  }  

  if(command != NULL)
    free(command);
}

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = event_data;
  switch (event->event_id) {
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGI(TAG, "MQTT initialized and about to start connecting to broker");
    mqtt_running = true;
    break;
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT connected");
    mqtt_connected = true;
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT disconnected");
    mqtt_connected = false;
    break;
  case MQTT_EVENT_ERROR: 
    ESP_LOGI(TAG, "MQTT error: %d", event->error_handle->connect_return_code);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "topic=%.*s data=%.*s", event->topic_len, event->topic, event->data_len, event->data);
    if (event->topic_len == update_len && strncmp(event->topic, update, update_len) == 0) {
      do_update(event->data, event->data_len);
    } 
    else if (event->topic_len == command_topic_len && strncmp(event->topic, command_topic, command_topic_len) == 0) {
      commandInterpreter(event->data, event->data_len);
    }
    else
      ESP_LOGI(TAG, "unhandled message for topic %.*s", event->topic_len, event->topic);
    break;
  default:
      ESP_LOGI(TAG, "Event received: %d", event->event_id);
    break;
  }
}

esp_mqtt_client_handle_t setup_mqtt(void)
{
  mqtt_running = false;
  mqtt_connected = false;

  mdns_result_t* mdnsres = NULL;
  esp_err_t err = mdns_query_ptr("_mqtt", "_tcp", 3000, 20,  &mdnsres);
  if (err) {
    ESP_LOGI(TAG, "mdns query for _mqtt._tcp failed");
    return NULL;
  }
  if (mdnsres == NULL) {
    ESP_LOGI(TAG, "cannot find mqtt broker via mdns");
    return NULL;
  }

  mdns_result_t* r = mdnsres;
  while (r != NULL) {
    ESP_LOGI(TAG, "mdns result for MQTT points to %s:%d", r->hostname, r->port);

    char buf[256];
    snprintf(buf, sizeof(buf), "mqtt://%s.local:%u", r->hostname, r->port);

    esp_mqtt_client_config_t mqtt_cfg = {
      .uri = buf,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client != NULL) {
      err = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, client);
      if(err)
      {
	      ESP_LOGI(TAG, "MQTT client register event failed");

      }
      err = esp_mqtt_client_start(client);
      if(err)
      {
	      ESP_LOGI(TAG, "MQTT client start failed");
      }
      else
      {
	      ESP_LOGI(TAG, "MQTT connection established");
      }
      break;
    }

    ESP_LOGI(TAG, "MQTT connection to %s failed during esp_mqtt_client_init", r->hostname);
    r = r->next;
  }

  mdns_query_results_free(mdnsres);
  if (client == NULL)
    ESP_LOGI(TAG, "mdnsres did not lead to initialized MQTT client");
  else {
    sensors_topic_len = asprintf(&sensors_topic, "%s/sensors", getHostname());
    heartbeat_topic_len = asprintf(&heartbeat_topic, "%s/heartbeat", getHostname()); 
    update_len = asprintf(&update, "%s/update", getHostname()); 
    esp_mqtt_client_subscribe(client, update, 1);
    command_topic_len = asprintf(&command_topic, "%s/command", getHostname()); 
    esp_mqtt_client_subscribe(client, command_topic, 1);
    ESP_LOGI(TAG, "MQTT channel %s subscribed", command_topic);
  }

  int tmr_id = 1;
  s_tmr = xTimerCreate("appmqttHeartbeatTmr", (5000 / portTICK_PERIOD_MS), pdTRUE, (void *) &tmr_id, appmqtt_heartbeat_cb);
  xTimerStart(s_tmr, portMAX_DELAY);
  return client;
}

