#pragma once

#define BYTE uint8_t

#define OLEDRGB_WIDTH                      	96
#define OLEDRGB_HEIGHT                     	64

#define CMD_DRAWLINE                       	0x21
#define CMD_DRAWRECTANGLE                  	0x22
#define CMD_COPYWINDOW                     	0x23
#define CMD_DIMWINDOW                      	0x24
#define CMD_CLEARWINDOW                    	0x25
#define CMD_FILLWINDOW                     	0x26
#define DISABLE_FILL    			0x00
#define ENABLE_FILL     			0x01
#define CMD_CONTINUOUSSCROLLINGSETUP      	0x27
#define CMD_DEACTIVESCROLLING              	0x2E
#define CMD_ACTIVESCROLLING                	0x2F

#define CMD_SETCOLUMNADDRESS              	0x15
#define CMD_SETROWADDRESS                 	0x75
#define CMD_SETCONTRASTA                  	0x81
#define CMD_SETCONTRASTB                  	0x82
#define CMD_SETCONTRASTC                  	0x83
#define CMD_MASTERCURRENTCONTROL          	0x87
#define CMD_SETPRECHARGESPEEDA           	0x8A
#define CMD_SETPRECHARGESPEEDB           	0x8B
#define CMD_SETPRECHARGESPEEDC           	0x8C
#define CMD_SETREMAP                       	0xA0
#define CMD_SETDISPLAYSTARTLINE          	0xA1
#define CMD_SETDISPLAYOFFSET              	0xA2
#define CMD_NORMALDISPLAY                  	0xA4
#define CMD_ENTIREDISPLAYON              	0xA5
#define CMD_ENTIREDISPLAYOFF              	0xA6
#define CMD_INVERSEDISPLAY                 	0xA7
#define CMD_SETMULTIPLEXRATIO             	0xA8
#define CMD_DIMMODESETTING                	0xAB
#define CMD_SETMASTERCONFIGURE            	0xAD
#define CMD_DIMMODEDISPLAYON             	0xAC
#define CMD_DISPLAYOFF                     	0xAE
#define CMD_DISPLAYON    			0xAF
#define CMD_POWERSAVEMODE                 	0xB0
#define CMD_PHASEPERIODADJUSTMENT         	0xB1
#define CMD_DISPLAYCLOCKDIV               	0xB3
#define CMD_SETGRAySCALETABLE            	0xB8
#define CMD_ENABLELINEARGRAYSCALETABLE  	0xB9
#define CMD_SETPRECHARGEVOLTAGE           	0xBB
#define CMD_SETVVOLTAGE                   	0xBE

void printToDisplay(char *str);
void displayInit(void);
void displayDeviceInit(void);
void displayDeviceDeInit(void);
void displayClear(void);

