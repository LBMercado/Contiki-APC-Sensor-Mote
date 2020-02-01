/* modified code of pm10-sensor found in zoul/dev */
/*---------------------------------------------------------*/
#include "lib/sensors.h"
/*---------------------------------------------------------*/
#ifndef PM25_SENSOR_H_
#define PM25_SENSOR_H_
/*---------------------------------------------------------*/
#define PM25_ERROR 	             (-1)
#define PM25_SUCCESS                 0x00
#define PM25_SENSOR                  "PM25 Sensor (GP2Y1014AU0f)"
#define PM25_SENSOR_PULSE_DELAY      320 //refer to datasheet of GP2Y1014AU0f
#define PM25_ADC_REF                 50000 //in mV, sf. 1st decimal place
#define PM25_ADC_CROSSREF            33000 //in mV, sf. 1st decimal place
/* -------------------------------------------------------------------------- */
#define PM25_DISABLE                 0x00
#define PM25_ENABLE                  0x01
/* -------------------------------------------------------------------------- */
/* refer to datasheet of GP2Y1014AU0f
 * approximate values used to compute dust density */
#define PM25_MAX_OUTPUT_MILLIVOLT     3500
#define PM25_MIN_OUTPUT_MILLIVOLT     600
#define PM25_MAX_OUTPUT_MICRODUST     500
#define PM25_MIN_OUTPUT_MICRODUST     0
/* -------------------------------------------------------------------------- */
/* default LED pin = PD2         */
/* default Output pin = PA5/ADC1 */
#ifdef PM25_SENSOR_LED_CONF_CTRL_PIN
#define PM25_SENSOR_LED_CTRL_PIN         PM25_SENSOR_LED_CONF_CTRL_PIN
#else
#define PM25_SENSOR_LED_CTRL_PIN         2
#endif
#ifdef PM25_SENSOR_LED_CONF_CTRL_PORT
#define PM25_SENSOR_LED_CTRL_PORT        PM25_SENSOR_LED_CONF_CTRL_PORT
#else
#define PM25_SENSOR_LED_CTRL_PORT        GPIO_D_NUM
#endif
#ifdef PM25_SENSOR_OUT_CONF_CTRL_PIN
#define PM25_SENSOR_OUT_CTRL_PIN         PM25_SENSOR_OUT_CONF_CTRL_PIN
#else
#define PM25_SENSOR_OUT_CTRL_PIN         ADC_SENSORS_ADC1_PIN
#endif
/* -------------------------------------------------------------------------- */
extern const struct sensors_sensor pm25;
/* -------------------------------------------------------------------------- */
#endif /* ifndef PM25_SENSOR_H_ */
