#pragma once

bool isMQTTRunning(void);
bool isMQTTConnected(void);
esp_mqtt_client_handle_t setup_mqtt(void);
void postSensorsReport(char *);

