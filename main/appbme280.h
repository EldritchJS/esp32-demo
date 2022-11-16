#pragma once

#include "bme280.h"

void initBME280(void);
#if CONFIG_BME280_I2C_ENABLE
void getBME280Data(float*, float*, float*);
#elif CONFIG_BME280_SPI_ENABLE
void initBME280SPI(void);
struct bme280_data * getBME280Data(void);
#endif

