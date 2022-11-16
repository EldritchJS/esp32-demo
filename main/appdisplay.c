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
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "appsensors.h"
#include "appdisplay.h"


#define DISPLAY_TAG "appdisplay"
#define DISPLAY_HOST    SPI2_HOST

#define DISPLAY_PIN_NUM_MOSI GPIO_NUM_7
#define DISPLAY_PIN_NUM_CLK  GPIO_NUM_6
#define DISPLAY_PIN_NUM_CS   GPIO_NUM_10

#define DISPLAY_PIN_NUM_DC   GPIO_NUM_9
#define DISPLAY_PIN_NUM_RST  GPIO_NUM_4
#define DISPLAY_PIN_NUM_PME  GPIO_NUM_5
#define DISPLAY_PIN_NUM_VEN  GPIO_NUM_8

static bool isInited = false;
static spi_device_handle_t hSPI;

static esp_err_t displayCMD(uint8_t cmd);
static esp_err_t displayCMD1(uint8_t cmd, uint8_t a);
static esp_err_t displayData(void* data, int len);

static esp_err_t displayWriteSPI(uint8_t *pCmd, int nCmd, uint8_t *pData, int nData);
static esp_err_t displayWriteSPI(uint8_t *pCmd, int nCmd, uint8_t *pData, int nData)
{
   gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
   spi_transaction_t t = {
      .length = 8*nCmd,
      .tx_data = pCmd,
   };
   esp_err_t e = spi_device_polling_transmit(hSPI, &t);
   if(e)
   {
     return e;
   }
   if(nData != 0)
   {
     gpio_set_level(DISPLAY_PIN_NUM_DC, 1);
     spi_transaction_t t = {
        .length = 8*nData,
        .tx_data = pData,
     };
     e = spi_device_polling_transmit(hSPI, &t);
   }
  return e;
}

static esp_err_t displayCMD(uint8_t cmd)
{                               /* Send command */
   gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
   spi_transaction_t t = {
      .length = 8,
      .tx_data = { cmd },
      .flags = SPI_TRANS_USE_TXDATA,
   };
   esp_err_t e = spi_device_polling_transmit(hSPI, &t);
   return e;
}

static esp_err_t displayCMD1(uint8_t cmd, uint8_t a)
{                               /* Send a command with an arg */
   esp_err_t e = displayCMD(cmd);
   if (e)
      return e;
//   gpio_set_level(DISPLAY_PIN_NUM_DC, 1);
   spi_transaction_t d = {
      .length = 8,
      .tx_data = { a },
      .flags = SPI_TRANS_USE_TXDATA,
   };
   return spi_device_polling_transmit(hSPI, &d);
}

static esp_err_t displayBulk(void *data, int len);
static esp_err_t displayBulk(void *data, int len)
{
   spi_transaction_t c = {
      .length = 8 * len,
      .tx_buffer = data,
   };
   return spi_device_transmit(hSPI, &c);
}

static esp_err_t displayData(void *data, int len)
{                               /* Send data */
   gpio_set_level(DISPLAY_PIN_NUM_DC, 1);
   spi_transaction_t c = {
      .length = 8 * len,
      .tx_buffer = data,
   };
   return spi_device_transmit(hSPI, &c);
}

uint8_t OLEDrgb_ExtractRFromRGB(uint16_t wRGB) 
{
   return (uint8_t) ((wRGB >> 11) & 0x1F);
}

uint8_t OLEDrgb_ExtractGFromRGB(uint16_t wRGB) 
{
   return (uint8_t) ((wRGB >> 5) & 0x3F);
}

uint8_t OLEDrgb_ExtractBFromRGB(uint16_t wRGB) 
{
   return (uint8_t) (wRGB & 0x1F);
}

uint16_t displayBuildRGB(uint8_t R, uint8_t G, uint8_t B) 
{
   return ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
}

uint16_t displayBuildHSV(uint8_t hue, uint8_t sat, uint8_t val)
{
   uint8_t region, remain, p, q, t;
   uint8_t R, G, B;
   region = hue / 43;
   remain = (hue - (region * 43)) * 6;
   p = (val * (255 - sat)) >> 8;
   q = (val * (255 - ((sat * remain) >> 8))) >> 8;
   t = (val * (255 - ((sat * (255 - remain)) >> 8))) >> 8;

   switch (region) {
   case 0:
      R = val;
      G = t;
      B = p;
      break;
   case 1:
      R = q;
      G = val;
      B = p;
      break;
   case 2:
      R = p;
      G = val;
      B = t;
      break;
   case 3:
      R = p;
      G = q;
      B = val;
      break;
   case 4:
      R = t;
      G = p;
      B = val;
      break;
   default:
      R = val;
      G = p;
      B = q;
      break;
   }
   return displayBuildRGB(R, G, B);
}

void displayClear(void)
{
  uint8_t data[4] = {0x00, 0x00, OLEDRGB_WIDTH - 1, OLEDRGB_HEIGHT - 1};

  displayCMD(CMD_CLEARWINDOW);
  displayData(data, sizeof(data));
  vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displayDim(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2) 
{
   uint8_t cmds[5];
   cmds[0] = CMD_DIMWINDOW;
   cmds[1] = c1; // Set the starting column coordinates
   cmds[2] = r1; // Set the starting row coordinates
   cmds[3] = c2; // Set the finishing column coordinates
   cmds[4] = r2; // Set the finishing row coordinates

   displayWriteSPI(cmds, 5, NULL, 0);
   vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displayCopy(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint8_t c3, uint8_t r3) 
{
   uint8_t cmds[7];
   cmds[0] = CMD_COPYWINDOW;
   cmds[1] = c1;                   // Set the starting column coordinates
   cmds[2] = r1;                   // Set the starting row coordinates
   cmds[3] = c2;                   // Set the finishing column coordinates
   cmds[4] = r2;                   // Set the finishing row coordinates
   cmds[5] = c3;                   // Set the new starting column coordinates
   cmds[6] = r3;                   // Set the new starting row coordinates

   displayWriteSPI(cmds, 7, NULL, 0);
   vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displayDeviceDeInit(void)
{
  if(isInited==false)
    return;

  displayCMD(CMD_DISPLAYOFF);
  gpio_set_level(DISPLAY_PIN_NUM_DC, 1);
  gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
  gpio_set_level(DISPLAY_PIN_NUM_VEN, 0);
  gpio_set_level(DISPLAY_PIN_NUM_PME, 1);
}

void displayDeviceInit(void)
{
  if(isInited==false)
    return;

#if 0
  // https://digilent.com/reference/pmod/pmodoledrgb/reference-manual
  // Step 1
  gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
  // Step 2
  gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
  // Step 3
  gpio_set_level(DISPLAY_PIN_NUM_VEN, 0);
  // Step 4
  gpio_set_level(DISPLAY_PIN_NUM_PME, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  // Steps 5 amd 6
  gpio_set_level(DISPLAY_PIN_NUM_RST, 0);
  vTaskDelay(1000 / portTICK_PERIOD_MS);	
  gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  ESP_LOGI(DISPLAY_TAG, "Step 7");
  // Step 7 - Unlock command
  displayCMD(0xFD);
  displayCMD(0x12);
  // Step 8
  displayCMD(CMD_DISPLAYOFF);
  // Step 9
  displayCMD1(CMD_SETREMAP, 0x72);
  // Step 10
  displayCMD1(CMD_SETDISPLAYSTARTLINE, 0x00);
  // Step 11
  displayCMD1(CMD_SETDISPLAYOFFSET, 0x00);
  ESP_LOGI(DISPLAY_TAG, "Step 12");
  // Step 12
  displayCMD(CMD_NORMALDISPLAY);
  // Step 13
  displayCMD1(CMD_SETMULTIPLEXRATIO, 0x3F);
  // Step 14
  displayCMD1(CMD_SETMASTERCONFIGURE, 0x8E);
  // Step 15
  displayCMD1(CMD_POWERSAVEMODE, 0x0B);
  // Step 16
  displayCMD1(CMD_PHASEPERIODADJUSTMENT, 0x31);
  // Step 17
  displayCMD1(CMD_DISPLAYCLOCKDIV, 0xF0);
  // Step 18
  displayCMD1(CMD_SETPRECHARGESPEEDA, 0x64);
  // Step 19
  displayCMD1(CMD_SETPRECHARGESPEEDB, 0x78);
  // Step 20
  displayCMD1(CMD_SETPRECHARGESPEEDC, 0x64);  
  // Step 21
  displayCMD1(CMD_SETPRECHARGEVOLTAGE, 0x3A);
  ESP_LOGI(DISPLAY_TAG, "Step 22");
  // Step 22
  displayCMD1(CMD_SETVVOLTAGE, 0x3E);
  // Step 23
  displayCMD1(CMD_MASTERCURRENTCONTROL, 0x06);
  // Step 24
  displayCMD1(CMD_SETCONTRASTA, 0x91);
  // Step 25
  displayCMD1(CMD_SETCONTRASTB, 0x50);
  // Step 26
  displayCMD1(CMD_SETCONTRASTC, 0x7D);
  ESP_LOGI(DISPLAY_TAG, "Step 27");
  // Step 27
  displayCMD(CMD_DEACTIVESCROLLING);
  // Step 28
  displayClear(); 
  // Step 29
  gpio_set_level(DISPLAY_PIN_NUM_VEN, 1);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  ESP_LOGI(DISPLAY_TAG, "Step 31");
  // Step 30
  displayCMD(CMD_DISPLAYON);
  // Step 31
  vTaskDelay(300 / portTICK_PERIOD_MS);
#else
//  gpio_set_level(DISPLAY_PIN_NUM_VEN, 1);
//  displayBulk(cmds, sizeof(cmds));
  displayCMD(CMD_DISPLAYOFF);
  displayCMD(CMD_SETREMAP);
  displayCMD(0x72);
  displayCMD(CMD_SETDISPLAYSTARTLINE);
  displayCMD(0x00);
  displayCMD(CMD_SETDISPLAYOFFSET);
  displayCMD(0x00);
  displayCMD(CMD_NORMALDISPLAY);
  displayCMD(CMD_SETMULTIPLEXRATIO);
  displayCMD(0x3F);
  displayCMD(CMD_SETMASTERCONFIGURE);     // 0xAD
  displayCMD(0x8E);
  displayCMD(CMD_POWERSAVEMODE); // 0xB0
  displayCMD(0x0B);
  displayCMD(CMD_PHASEPERIODADJUSTMENT); // 0xB1
  displayCMD(0x31);
  displayCMD(CMD_DISPLAYCLOCKDIV); // 0xB3
  displayCMD(0xF0); // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio
                     // (A[3:0]+1 = 1..16)
  displayCMD(CMD_SETPRECHARGESPEEDA); // 0x8A
  displayCMD(0x64);
  displayCMD(CMD_SETPRECHARGESPEEDB); // 0x8B
  displayCMD(0x78);
  displayCMD(CMD_SETPRECHARGESPEEDC); // 0x8C
  displayCMD(0x64);
  displayCMD(CMD_SETPRECHARGEVOLTAGE); // 0xBB
  displayCMD(0x3A);
  displayCMD(CMD_SETVVOLTAGE); // 0xBE
  displayCMD(0x3E);
  displayCMD(CMD_MASTERCURRENTCONTROL); // 0x87
  displayCMD(0x06);
  displayCMD(CMD_SETCONTRASTA); // 0x81
  displayCMD(0x91);
  displayCMD(CMD_SETCONTRASTB); // 0x82
  displayCMD(0x50);
  displayCMD(CMD_SETCONTRASTC); // 0x83
  displayCMD(0x7D);
  displayCMD(CMD_DEACTIVESCROLLING);
//  displayClear();
//  displayCMD(CMD_CLEARWINDOW);
//  displayCMD(0x00);
//  displayCMD(0x00);
//  displayCMD(OLEDRGB_WIDTH - 1);
//  displayCMD(OLEDRGB_HEIGHT - 1); 
  displayCMD(CMD_DISPLAYON); //--turn on oled panel
#endif
}

void displayTask(void)
{
}

void printToDisplay(char *str)
{
  if(isInited==false)
    return;	  
}

void displayInit(void)
{
  esp_err_t ret;

  gpio_set_direction(DISPLAY_PIN_NUM_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_RST, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_PME, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_VEN, GPIO_MODE_OUTPUT);

  gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
  gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
  gpio_set_level(DISPLAY_PIN_NUM_VEN, 1);
  gpio_set_level(DISPLAY_PIN_NUM_PME, 1);

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = DISPLAY_PIN_NUM_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = DISPLAY_PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
      .flags = SPICOMMON_BUSFLAG_MASTER,
  };
  spi_device_interface_config_t devcfg={
      .clock_speed_hz=SPI_MASTER_FREQ_10M | SPI_DEVICE_3WIRE,     
      .mode=0,                                
      .spics_io_num=DISPLAY_PIN_NUM_CS,            
      .queue_size=1,                         
  };
  ret = spi_bus_initialize(DISPLAY_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);
  ret=spi_bus_add_device(DISPLAY_HOST, &devcfg, &hSPI);
  ESP_ERROR_CHECK(ret);
  isInited = true;
}


