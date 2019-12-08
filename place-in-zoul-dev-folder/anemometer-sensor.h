#include "contiki.h"
#include "lib/sensors.h"
#include "dev/gpio.h"
#include "dev/adc-zoul.h"
#include <stdio.h>
/*--------------------------------------------------------------------------------*/
#ifndef ANEM_H_
#define ANEM_H_
/*--------------------------------------------------------------------------------*/
//Available sensors to use
#define WIND_SPEED_SENSOR             0x00 // m/s
#define WIND_DIR_SENSOR               0x01 // Direction (N,E,S,W and Combinations)
/*--------------------------------------------------------------------------------*/
/* reference values for the sensors */
//Wind Direction Sensor
#define WIND_DIR_NORTH                0x0001
#define WIND_DIR_EAST                 0x0010
#define WIND_DIR_SOUTH                0x1000
#define WIND_DIR_WEST                 0x0100
//Wind Speed Sensor
#define WIND_MAX_SPEED                32
#define WIND_MIN_SPEED                0
#define WIND_ANALOG_V_MIN             0.4
#define WIND_ANALOG_V_MAX             2.0
/*--------------------------------------------------------------------------------*/
//Return values
#define WIND_SENSOR_ERROR             (-1)
#define WIND_SENSOR_SUCCESS           0x00
/*--------------------------------------------------------------------------------*/
//Sensor states
#define WIND_SENSOR_ENABLED           0x01
/*--------------------------------------------------------------------------------*/
//Pin configuration
#ifdef WIND_SPEED_SENSOR_CONF_CTRL_PIN
#define WIND_SPEED_SENSOR_CTRL_PIN    WIND_SPEED_SENSOR_CONF_CTRL_PIN
#else 
#define WIND_SPEED_SENSOR_CTRL_PIN    ADC_SENSORS_ADC3_PIN //PA2
#endif
/*--------------------------------------------------------------------------------*/
#ifdef WIND_DIR_SENSOR_CONF_CTRL_PIN
#define WIND_DIR_SENSOR_CTRL_PIN      WIND_DIR_SENSOR_CONF_CTRL_PIN
#else
#define WIND_DIR_SENSOR_CTRL_PIN      ADC_SENSORS_ADC2_PIN //PA4
#endif
/*--------------------------------------------------------------------------------*/
//initial values
/* initial wind sensor position which can be also ordinal direction */
#ifdef WIND_DIR_SENSOR_CONF_INIT_POS
#define WIND_DIR_SENSOR_INIT_POS      WIND_DIR_SENSOR_CONF_INIT_POS
#else
#define WIND_DIR_SENSOR_INIT_POS      WIND_DIR_NORTH
#endif
/*--------------------------------------------------------------------------------*/
#define ANEM_SENSOR "Anemometer Sensor"
extern const struct sensors_sensor anem_sensor;
/*--------------------------------------------------------------------------------*/
#endif /* #ifndef ANEM_H_ */
/*--------------------------------------------------------------------------------*/
