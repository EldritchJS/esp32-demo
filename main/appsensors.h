#pragma once

enum {
    SENSORS_START_SIG   = 0x0000,    	/* start sensors */
    SENSORS_STOP_SIG 	= 0x0010, 	/* stop sensors */
    SENSORS_ACQUIRE_SIG = 0x0100,    /* acquire data from sensors */
};

enum {
    SENSORS_STATE_OFF,
    SENSORS_STATE_ON
};

/* message to be sent */
typedef struct {
    uint16_t             sig;      /*!< signal to sensors_task */
    void                 *param;   /*!< parameter area needs to be last */
} sensors_msg_t;

void initSensors(uint8_t settings);
bool stopSensors(void);
bool startSensors(void);
