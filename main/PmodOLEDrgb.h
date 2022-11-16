#pragma once

/************ Macro Definitions ************/

#define OLEDRGB_WIDTH  96
#define OLEDRGB_HEIGHT 64

#define OLEDRGB_CHARBYTES    8    // Number of bytes in a glyph
#define	OLEDRGB_USERCHAR_MAX 0x20 // Number of character defs in user font
                                    // table
#define OLEDRGB_CHARBYTES_USER (OLEDRGB_USERCHAR_MAX*OLEDRGB_CHARBYTES)
                               // Number of bytes in user font table

#define CMD_DRAWLINE                 0x21
#define CMD_DRAWRECTANGLE            0x22
#define CMD_COPYWINDOW               0x23
#define CMD_DIMWINDOW                0x24
#define CMD_CLEARWINDOW              0x25
#define CMD_FILLWINDOW               0x26
#define DISABLE_FILL                 0x00
#define ENABLE_FILL                  0x01
#define CMD_CONTINUOUSSCROLLINGSETUP 0x27
#define CMD_DEACTIVESCROLLING        0x2E
#define CMD_ACTIVESCROLLING          0x2F

#define CMD_SETCOLUMNADDRESS           0x15
#define CMD_SETROWADDRESS              0x75
#define CMD_SETCONTRASTA               0x81
#define CMD_SETCONTRASTB               0x82
#define CMD_SETCONTRASTC               0x83
#define CMD_MASTERCURRENTCONTROL       0x87
#define CMD_SETPRECHARGESPEEDA         0x8A
#define CMD_SETPRECHARGESPEEDB         0x8B
#define CMD_SETPRECHARGESPEEDC         0x8C
#define CMD_SETREMAP                   0xA0
#define CMD_SETDISPLAYSTARTLINE        0xA1
#define CMD_SETDISPLAYOFFSET           0xA2
#define CMD_NORMALDISPLAY              0xA4
#define CMD_ENTIREDISPLAYON            0xA5
#define CMD_ENTIREDISPLAYOFF           0xA6
#define CMD_INVERSEDISPLAY             0xA7
#define CMD_SETMULTIPLEXRATIO          0xA8
#define CMD_DIMMODESETTING             0xAB
#define CMD_SETMASTERCONFIGURE         0xAD
#define CMD_DIMMODEDISPLAYON           0xAC
#define CMD_DISPLAYOFF                 0xAE
#define CMD_DISPLAYON                  0xAF
#define CMD_POWERSAVEMODE              0xB0
#define CMD_PHASEPERIODADJUSTMENT      0xB1
#define CMD_DISPLAYCLOCKDIV            0xB3
#define CMD_SETGRAySCALETABLE          0xB8
#define CMD_ENABLELINEARGRAYSCALETABLE 0xB9
#define CMD_SETPRECHARGEVOLTAGE        0xBB
#define CMD_SETVVOLTAGE                0xBE

typedef struct {
   uint8_t *pbOledrgbFontCur;
   uint8_t *pbOledrgbFontUser;
   uint8_t rgbOledrgbFontUser[OLEDRGB_CHARBYTES_USER];
   int dxcoOledrgbFontCur;
   int dycoOledrgbFontCur;
   uint16_t m_FontColor, m_FontBkColor;
   int xchOledCur;
   int ychOledCur;

   int xchOledrgbMax;
   int ychOledrgbMax;
} PmodOLEDrgb;


/************ Function Prototypes ************/

void displayInit(void);
void displayDeInit(void);

void displayDrawPixel(uint8_t c, uint8_t r, uint16_t pixelColor);
void displayDrawLine(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint16_t lineColor);
void displayDrawRectangle(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2,
      uint16_t lineColor, uint8_t bFill, uint16_t fillColor);
void displayClear(void);
void displayDrawBitmap(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint8_t *pBmp);

void displaySetCursor(int xch, int ych);
void displayGetCursor(int *pxch, int *pych);
int displayDefUserChar(char ch, uint8_t *pbDef);
void displayDrawGlyph(char ch);
void displayPutChar(char ch);
void displayPutString(char *sz);
void displaySetFontColor(uint16_t fontColor);
void displaySetFontBkColor(uint16_t fontBkColor);
void displaySetCurrentFontTable(uint8_t *pbFont);
void displaySetCurrentUserFontTable(uint8_t *pbUserFont);
void displayAdvanceCursor(void);
void displaySetScrolling(uint8_t scrollH, uint8_t scrollV,
      uint8_t rowAddr, uint8_t rowNum, uint8_t timeInterval);
void displayEnableScrolling(uint8_t fEnable);
void displayEnablePmod(uint8_t fEnable);
void displayEnableBackLight(uint8_t fEnable);
void display_Copy(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2, uint8_t c3, uint8_t r3);
void displayDim(uint8_t c1, uint8_t r1, uint8_t c2, uint8_t r2);
void displayHostInit(void);
void displayHostTerm(void);
void displayDevInit(void);
void displayDevTerm(void);

void displaySPIInit(void);
void displayWriteSPICommand(uint8_t cmd);
void displayWriteSPI(uint8_t *pCmd, int nCmd, uint8_t *pData, int nData);
uint16_t displayBuildHSV(uint8_t hue, uint8_t sat, uint8_t val);
uint16_t displayBuildRGB(uint8_t R, uint8_t G, uint8_t B);

uint8_t displayExtractRFromRGB(uint16_t wRGB);
uint8_t displayExtractGFromRGB(uint16_t wRGB);
uint8_t displayExtractBFromRGB(uint16_t wRGB);

