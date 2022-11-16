#include <ultrasonic.h>
#include <esp_err.h>

static ultrasonic_sensor_t   sensor = {
    .trigger_pin = CONFIG_ULTRASONIC_TRIGGER_GPIO,
    .echo_pin = CONFIG_ULTRASONIC_ECHO_GPIO
  };

esp_err_t ultrasonicMeasure(float *distance)
{
  esp_err_t res = ultrasonic_measure(&sensor, CONFIG_ULTRASONIC_MAX_DISTANCE, distance);
  return res;
}

void ultrasonicInit(void)
{
  ultrasonic_init(&sensor);
}
