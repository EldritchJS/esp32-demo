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
#include "ChrFont0.h"
#include "PmodOLEDrgb.h"

#define DISPLAY_TAG "appdisplay"
#define DISPLAY_HOST    SPI2_HOST

#define DISPLAY_PIN_NUM_MOSI CONFIG_OLEDRGB_GPIO_MOSI //GPIO_NUM_7
#define DISPLAY_PIN_NUM_CLK  CONFIG_OLEDRGB_GPIO_CLK //GPIO_NUM_6
#define DISPLAY_PIN_NUM_CS   CONFIG_OLEDRGB_GPIO_CS //GPIO_NUM_10

#define DISPLAY_PIN_NUM_DC   CONFIG_OLEDRGB_GPIO_DC //GPIO_NUM_9
#define DISPLAY_PIN_NUM_RST  CONFIG_OLEDRGB_GPIO_RST //GPIO_NUM_4
#define DISPLAY_PIN_NUM_PME  CONFIG_OLEDRGB_GPIO_PME //GPIO_NUM_5
#define DISPLAY_PIN_NUM_VEN  CONFIG_OLEDRGB_GPIO_PME //GPIO_NUM_8

static bool isInited = false;
static spi_device_handle_t hSPI;

uint8_t num_devices = 0;
static PmodOLEDrgb OLEDInstance; 

void displayInit(void) 
{
   int ib;

   OLEDInstance.dxcoOledrgbFontCur = OLEDRGB_CHARBYTES;
   OLEDInstance.dycoOledrgbFontCur = 8;
   
   for (ib = 0; ib < OLEDRGB_CHARBYTES_USER; ib++) 
   {
      OLEDInstance.rgbOledrgbFontUser[ib] = 0;
   }
   OLEDInstance.xchOledrgbMax = OLEDRGB_WIDTH / OLEDInstance.dxcoOledrgbFontCur;
   OLEDInstance.ychOledrgbMax = OLEDRGB_HEIGHT / OLEDInstance.dycoOledrgbFontCur;

   displaySetFontColor(displayBuildRGB(0, 0xFF, 0)); // Green
   displaySetFontBkColor(displayBuildRGB(0, 0, 0));  // Black

   displaySetCurrentFontTable((uint8_t*) rgbOledRgbFont0);
   displaySetCurrentUserFontTable(OLEDInstance.rgbOledrgbFontUser);
   displaySPIInit();
   displayHostInit();
   displayDevInit();
}

void displayend(void) 
{
   displayHostTerm();
   displayDevTerm();
}

void displayDrawPixel(uint8_t c, uint8_t r, uint16_t pixelColor) 
{
   uint8_t cmds[6];
   uint8_t data[2];
   // Set column start and end
   cmds[0] = CMD_SETCOLUMNADDRESS;
   cmds[1] = c;                 // Set the starting column coordinates
   cmds[2] = OLEDRGB_WIDTH - 1; // Set the finishing column coordinates

   // Set row start and end
   cmds[3] = CMD_SETROWADDRESS;
   cmds[4] = r;                  // Set the starting row coordinates
   cmds[5] = OLEDRGB_HEIGHT - 1; // Set the finishing row coordinates

   data[0] = pixelColor >> 8;
   data[1] = pixelColor;

   displayWriteSPI(cmds, 6, data, 2);
}

void displayDrawLine(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint16_t lineColor) 
{
   uint8_t cmds[8];
   cmds[0] = CMD_DRAWLINE; // Draw line
   cmds[1] = c1;           // Start column
   cmds[2] = r1;           // Start row
   cmds[3] = c2;           // End column
   cmds[4] = r2;           // End row
   cmds[5] = displayExtractRFromRGB(lineColor);   // R
   cmds[6] = displayExtractGFromRGB(lineColor);   // G
   cmds[7] = displayExtractBFromRGB(lineColor);   // B

   displayWriteSPI(cmds, 8, NULL, 0);
}

void displayDrawRectangle(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint16_t lineColor, uint8_t bFill, uint16_t fillColor) 
{
   uint8_t cmds[13];
   cmds[0] = CMD_FILLWINDOW;                       // Fill window
   cmds[1] = (bFill ? ENABLE_FILL : DISABLE_FILL);
   cmds[2] = CMD_DRAWRECTANGLE;                    // Draw rectangle
   cmds[3] = c1;                                   // Start column
   cmds[4] = r1;                                   // Start row
   cmds[5] = c2;                                   // End column
   cmds[6] = r2;                                   // End row

   cmds[7] = displayExtractRFromRGB(lineColor);   // R
   cmds[8] = displayExtractGFromRGB(lineColor);   // G
   cmds[9] = displayExtractBFromRGB(lineColor);   // B

   cmds[10] = displayExtractRFromRGB(fillColor);  // R
   cmds[11] = displayExtractGFromRGB(fillColor);  // G
   cmds[12] = displayExtractBFromRGB(fillColor);  // B

   displayWriteSPI(cmds, 13, NULL, 0);
}

void displayClear(void) 
{
   uint8_t cmds[5];
   cmds[0] = CMD_CLEARWINDOW;     // Enter the "clear mode"
   cmds[1] = 0x00;                // Set the starting column coordinates
   cmds[2] = 0x00;                // Set the starting row coordinates
   cmds[3] = OLEDRGB_WIDTH - 1;   // Set the finishing column coordinates;
   cmds[4] = OLEDRGB_HEIGHT - 1;  // Set the finishing row coordinates;
   displayWriteSPI(cmds, 5, NULL, 0);
   vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displayDrawBitmap(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint8_t *pBmp) 
{
   uint8_t cmds[6];
   //set column start and end
   cmds[0] = CMD_SETCOLUMNADDRESS;
   cmds[1] = c1;                   // Set the starting column coordinates
   cmds[2] = c2;                   // Set the finishing column coordinates

   //set row start and end
   cmds[3] = CMD_SETROWADDRESS;
   cmds[4] = r1;                   // Set the starting row coordinates
   cmds[5] = r2;                   // Set the finishing row coordinates

   displayWriteSPI(cmds, 6, pBmp,
         (((c2 - c1 + 1) * (r2 - r1 + 1)) << 1));
   vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displaySetCursor(int xch, int ych) 
{
   // Clamp the specified location to the display surface
   if (xch >= OLEDInstance.xchOledrgbMax) {
      xch = OLEDInstance.xchOledrgbMax - 1;
   }

   if (ych >= OLEDInstance.ychOledrgbMax) {
      ych = OLEDInstance.ychOledrgbMax - 1;
   }

   // Save the given character location.
   OLEDInstance.xchOledCur = xch;
   OLEDInstance.ychOledCur = ych;
}

void displayGetCursor(int *pxch, int *pych) 
{
   *pxch = OLEDInstance.xchOledCur;
   *pych = OLEDInstance.ychOledCur;
}

int displayDefUserChar(char ch, uint8_t *pbDef) 
{
   uint8_t *pb;
   int ib;

   if (ch < OLEDRGB_USERCHAR_MAX) {
      pb = OLEDInstance.pbOledrgbFontUser + ch * OLEDRGB_CHARBYTES;
      for (ib = 0; ib < OLEDRGB_CHARBYTES; ib++) {
         *pb++ = *pbDef++;
      }
      return 1;
   } else {
      return 0;
   }
}

void displayDrawGlyph(char ch) 
{
   uint8_t *pbFont;
   int ibx, iby, iw, x, y;
   uint16_t rgwCharBmp[OLEDRGB_CHARBYTES << 4];

   if ((ch & 0x80) != 0) 
   {
      return;
   }

   if (ch < OLEDRGB_USERCHAR_MAX) 
   {
      pbFont = OLEDInstance.pbOledrgbFontUser + ch * OLEDRGB_CHARBYTES;
   } 
   else if ((ch & 0x80) == 0) 
   {
      pbFont = OLEDInstance.pbOledrgbFontCur
            + (ch - OLEDRGB_USERCHAR_MAX) * OLEDRGB_CHARBYTES;
   }

   iw = 0;
   for (iby = 0; iby < OLEDInstance.dycoOledrgbFontCur; iby++) 
   {
      for (ibx = 0; ibx < OLEDInstance.dxcoOledrgbFontCur; ibx++) 
      {
         if (pbFont[ibx] & (1 << iby)) 
	 {
            // Point in glyph
            rgwCharBmp[iw] = OLEDInstance.m_FontColor;
         } 
	 else 
	 {
            // Background
            rgwCharBmp[iw] = OLEDInstance.m_FontBkColor;
         }
         iw++;
      }
   }
   x = OLEDInstance.xchOledCur * OLEDInstance.dxcoOledrgbFontCur;
   y = OLEDInstance.ychOledCur * OLEDInstance.dycoOledrgbFontCur;

   displayDrawBitmap(x, y, x + OLEDRGB_CHARBYTES - 1, y + 7,
         (uint8_t*) rgwCharBmp);
}

void displayPutChar(char ch) 
{
   displayDrawGlyph(ch);
   displayAdvanceCursor();
}

void displayPutString(char *sz) 
{
   while (*sz != '\0') 
   {
      displayDrawGlyph(*sz);
      displayAdvanceCursor();
      sz += 1;
   }
}

void displaySetFontColor(uint16_t fontColor) 
{
   OLEDInstance.m_FontColor = fontColor;
}

void displaySetFontBkColor(uint16_t fontBkColor) 
{
   OLEDInstance.m_FontBkColor = fontBkColor;
}

void displaySetCurrentFontTable(uint8_t *pbFont) 
{
   OLEDInstance.pbOledrgbFontCur = pbFont;
}

void displaySetCurrentUserFontTable(uint8_t *pbUserFont) 
{
   OLEDInstance.pbOledrgbFontUser = pbUserFont;
}

void displayAdvanceCursor() 
{
   OLEDInstance.xchOledCur += 1;
   if (OLEDInstance.xchOledCur >= OLEDInstance.xchOledrgbMax) 
   {
      OLEDInstance.xchOledCur = 0;
      OLEDInstance.ychOledCur += 1;
   }
   if (OLEDInstance.ychOledCur >= OLEDInstance.ychOledrgbMax) 
   {
      OLEDInstance.ychOledCur = 0;
   }
   displaySetCursor(OLEDInstance.xchOledCur, OLEDInstance.ychOledCur);
}

void displaySetScrolling(uint8_t scrollH, uint8_t scrollV,
      uint8_t rowAddr, uint8_t rowNum, uint8_t timeInterval) 
{
   uint8_t cmds[7];
   cmds[0] = CMD_CONTINUOUSSCROLLINGSETUP;
   cmds[1] = scrollH;             // Horizontal scroll
   cmds[2] = rowAddr;             // Start row address
   cmds[3] = rowNum;              // Number of scrolling rows
   cmds[4] = scrollV;             // Vertical scroll
   cmds[5] = timeInterval;        // Time interval
   cmds[6] = CMD_ACTIVESCROLLING; // Set the starting row coordinates

   displayWriteSPI(cmds, 7, NULL, 0);
   vTaskDelay(5 / portTICK_PERIOD_MS);
}

void displayEnableScrolling(uint8_t fEnable) {
   displayWriteSPICommand(
         fEnable ? CMD_ACTIVESCROLLING : CMD_DEACTIVESCROLLING);
}

void displayEnablePmod(uint8_t fEnable) 
{
   if (fEnable) 
   {
      displayHostInit();
      displayDevInit();
   } 
   else 
   {
      displayDevTerm();
      displayHostTerm();
   }
}

void displayEnableBackLight(uint8_t fEnable) 
{
#if 0
   if (fEnable) {
      Xil_Out32(OLEDInstance.GPIO_addr + 4,
            Xil_In32(OLEDInstance.GPIO_addr + 4) & 0xB); // 0b1011
      Xil_Out32(OLEDInstance.GPIO_addr,
            Xil_In32(OLEDInstance.GPIO_addr) | 0x4); // 0b0100
      usleep(1000 * 25);
      displayWriteSPICommand(OLEDInstance, CMD_DISPLAYON);
   } else {
      displayWriteSPICommand(OLEDInstance, CMD_DISPLAYOFF);
      Xil_Out32(OLEDInstance.GPIO_addr,
            Xil_In32(OLEDInstance.GPIO_addr) & 0xB); // 0b1011
      Xil_Out32(OLEDInstance.GPIO_addr + 4,
            Xil_In32(OLEDInstance.GPIO_addr + 4) | 0x4); // 0b0100
   }
#endif
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

void displayHostInit() 
{
#if 0
   Xil_Out32(OLEDInstance.GPIO_addr + 4, 0x0000);
   Xil_Out32(OLEDInstance.GPIO_addr, 0xA);
   // Start the SPI driver so that the device is enabled.
   XSpi_Start(&OLEDInstance.OLEDSpi);
   // Disable Global interrupt to use polled mode operation
   XSpi_IntrGlobalDisable(&OLEDInstance.OLEDSpi);
#endif
}

void displayHostTerm() 
{
#if 0	
   XSpi_Stop(&OLEDInstance.OLEDSpi);
   // Make signal pins and power control pins be inputs.
   Xil_Out32(OLEDInstance.GPIO_addr, 0x3); // 0b0011
   Xil_Out32(OLEDInstance.GPIO_addr + 4, 0xF); // 0b1111
#endif
}

void displayDevInit(void) 
{
   uint8_t cmds[39];

   // Bring PmodEn HIGH
   gpio_set_level(DISPLAY_PIN_NUM_PME, 1);
   vTaskDelay(20 / portTICK_PERIOD_MS);

   // Assert Reset
   gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
   vTaskDelay(1 / portTICK_PERIOD_MS);
   gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
   vTaskDelay(2 / portTICK_PERIOD_MS);

   // Command un-lock
   cmds[0] = 0xFD;
   cmds[1] = 0x12;
   // 5. Univision Initialization Steps
   // 5a) Set Display Off
   cmds[2] = CMD_DISPLAYOFF;
   // 5b) Set Remap and Data Format
   cmds[3] = CMD_SETREMAP;
   cmds[4] = 0x72;
   // 5c) Set Display Start Line
   cmds[5] = CMD_SETDISPLAYSTARTLINE;
   cmds[6] = 0x00; // Start line is set at upper left corner
   // 5d) Set Display Offset
   cmds[7] = CMD_SETDISPLAYOFFSET;
   cmds[8] = 0x00; //no offset
   // 5e)
   cmds[9] = CMD_NORMALDISPLAY;
   // 5f) Set Multiplex Ratio
   cmds[10] = CMD_SETMULTIPLEXRATIO;
   cmds[11] = 0x3F; //64MUX
   // 5g)Set Master Configuration
   cmds[12] = CMD_SETMASTERCONFIGURE;
   cmds[13] = 0x8E;
   // 5h)Set Power Saving Mode
   cmds[14] = CMD_POWERSAVEMODE;
   cmds[15] = 0x0B;
   // 5i) Set Phase Length
   cmds[16] = CMD_PHASEPERIODADJUSTMENT;
   cmds[17] = 0x31; // Phase 2 = 14 DCLKs, phase 1 = 15 DCLKS
   // 5j) Send Clock Divide Ratio and Oscillator Frequency
   cmds[18] = CMD_DISPLAYCLOCKDIV;
   cmds[19] = 0xF0; // Mid high oscillator frequency, DCLK = FpbCllk/2
   // 5k) Set Second Pre-charge Speed of Color A
   cmds[20] = CMD_SETPRECHARGESPEEDA;
   cmds[21] = 0x64;
   // 5l) Set Set Second Pre-charge Speed of Color B
   cmds[22] = CMD_SETPRECHARGESPEEDB;
   cmds[23] = 0x78;
   // 5m) Set Second Pre-charge Speed of Color C
   cmds[24] = CMD_SETPRECHARGESPEEDC;
   cmds[25] = 0x64;
   // 5n) Set Pre-Charge Voltage
   cmds[26] = CMD_SETPRECHARGEVOLTAGE;
   cmds[27] = 0x3A; // Pre-charge voltage =...Vcc
   // 50) Set VCOMH Deselect Level
   cmds[28] = CMD_SETVVOLTAGE;
   cmds[29] = 0x3E; // Vcomh = ...*Vcc
   // 5p) Set Master Current
   cmds[30] = CMD_MASTERCURRENTCONTROL;
   cmds[31] = 0x06;
   // 5q) Set Contrast for Color A
   cmds[32] = CMD_SETCONTRASTA;
   cmds[33] = 0x91;
   // 5r) Set Contrast for Color B
   cmds[34] = CMD_SETCONTRASTB;
   cmds[35] = 0x50;
   // 5s) Set Contrast for Color C
   cmds[36] = CMD_SETCONTRASTC;
   cmds[37] = 0x7D;
   // Disable scrolling
   cmds[38] = CMD_DEACTIVESCROLLING;

   displayWriteSPI(cmds, 39, NULL, 0);

   // 5u) Clear Screen
   displayClear();
   // Turn on VCC and wait for it to become stable
   gpio_set_level(DISPLAY_PIN_NUM_VEN, 1);
   vTaskDelay(25 / portTICK_PERIOD_MS);

   // Send Display On command
   displayWriteSPICommand(CMD_DISPLAYON);

   vTaskDelay(100 / portTICK_PERIOD_MS);
}

void displayDevTerm() 
{
   displayWriteSPICommand(CMD_DISPLAYOFF);
   gpio_set_level(DISPLAY_PIN_NUM_VEN, 0);   
   vTaskDelay(400 / portTICK_PERIOD_MS);
}

void displaySPIInit(void) 
{
  static bool isInited = false;

  if(isInited == true)
    return;

  gpio_set_direction(DISPLAY_PIN_NUM_DC, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_RST, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_PME, GPIO_MODE_OUTPUT);
  gpio_set_direction(DISPLAY_PIN_NUM_VEN, GPIO_MODE_OUTPUT);

  gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
  gpio_set_level(DISPLAY_PIN_NUM_RST, 1);
  gpio_set_level(DISPLAY_PIN_NUM_VEN, 0);
  gpio_set_level(DISPLAY_PIN_NUM_PME, 0);
#if 0
  spi_bus_config_t bus_cfg = {
      .mosi_io_num = DISPLAY_PIN_NUM_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = DISPLAY_PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
      .flags = SPICOMMON_BUSFLAG_MASTER,
  };
  spi_bus_initialize(DISPLAY_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
#endif
  spi_device_interface_config_t devcfg={
      .clock_speed_hz=SPI_MASTER_FREQ_10M | SPI_DEVICE_3WIRE,     
      .mode=0,                                
      .spics_io_num=DISPLAY_PIN_NUM_CS,            
      .queue_size=1,                         
  };
  //ESP_ERROR_CHECK(ret);
  spi_bus_add_device(DISPLAY_HOST, &devcfg, &hSPI);
  //ESP_ERROR_CHECK(ret);
  isInited = true;
}

void displayWriteSPICommand(uint8_t cmd) 
{
   gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
   spi_transaction_t t = {
      .length = 8,
      .tx_data = { cmd },
      .flags = SPI_TRANS_USE_TXDATA,
   };
   spi_device_polling_transmit(hSPI, &t);	
}

void displayWriteSPI(uint8_t *pCmd, int nCmd, uint8_t *pData, int nData) 
{
   gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
   spi_transaction_t t = {
      .length = 8*nCmd,
      .tx_buffer = pCmd,
   };
   spi_device_polling_transmit(hSPI, &t);
   
   if(nData != 0)
   {
     gpio_set_level(DISPLAY_PIN_NUM_DC, 1);
     spi_transaction_t t = {
        .length = 8*nData,
        .tx_buffer = pData,
     };
     spi_device_polling_transmit(hSPI, &t);
     gpio_set_level(DISPLAY_PIN_NUM_DC, 0);
   }
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

   switch (region) 
   {
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

uint16_t displayBuildRGB(uint8_t R, uint8_t G, uint8_t B) 
{
   return ((R >> 3) << 11) | ((G >> 2) << 5) | (B >> 3);
}

uint8_t displayExtractRFromRGB(uint16_t wRGB) 
{
   return (uint8_t) ((wRGB >> 11) & 0x1F);
}

uint8_t displayExtractGFromRGB(uint16_t wRGB) 
{
   return (uint8_t) ((wRGB >> 5) & 0x3F);
}

uint8_t displayExtractBFromRGB(uint16_t wRGB) 
{
   return (uint8_t) (wRGB & 0x1F);
}
