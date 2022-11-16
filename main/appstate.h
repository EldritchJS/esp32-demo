#pragma once
void read_wifi_config(void);
void write_wifi_config(void);
void init_time(void);
char* getHostname(void);
void setHostname(uint8_t* mac);
void setup_mdns(void);


