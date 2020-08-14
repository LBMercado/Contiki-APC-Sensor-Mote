/*--------------------------------------------------------------------------------*/
#ifndef ANEM_H_
#define ANEM_H_
/*--------------------------------------------------------------------------------*/
#include "contiki.h"
#include "lib/sensors.h"
#include "dev/gpio.h"
#include <stdio.h>
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#include "dev/adc-zoul.h"
#else
#include "dev/adc128s022.h"
#endif
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
#define WIND_ANALOG_MILLV_MIN         400
#define WIND_ANALOG_MILLV_MAX         2000
/*--------------------------------------------------------------------------------*/
//Return values
#define WIND_SENSOR_ERROR             (-1)
#define WIND_SENSOR_SUCCESS           0x00
/*--------------------------------------------------------------------------------*/
//Sensor states
#define WIND_SENSOR_ENABLED           0x01
/*--------------------------------------------------------------------------------*/
/*------------------------External ADC Definitions--------------------------------*/
/*--------------------------------------------------------------------------------*/
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
/*--------------------------------------------------------------------------------*/
#ifdef WIND_SPEED_SENSOR_CONF_EXT_ADC_CHANNEL
#define WIND_SPEED_SENSOR_EXT_ADC_CHANNEL       WIND_SPEED_SENSOR_CONF_EXT_ADC_CHANNEL
#else
#define WIND_SPEED_SENSOR_EXT_ADC_CHANNEL       6
#endif
/*--------------------------------------------------------------------------------*/
#ifdef WIND_DIR_SENSOR_CONF_EXT_ADC_CHANNEL
#define WIND_DIR_SENSOR_EXT_ADC_CHANNEL         WIND_DIR_SENSOR_CONF_EXT_ADC_CHANNEL
#else
#define WIND_DIR_SENSOR_EXT_ADC_CHANNEL         7
#endif
/*--------------------------------------------------------------------------------*/
/*------------------------Internal ADC Definitions--------------------------------*/
/*--------------------------------------------------------------------------------*/
#else
/*--------------------------------------------------------------------------------*/
#ifdef WIND_SPEED_SENSOR_CONF_CTRL_PIN
#define WIND_SPEED_SENSOR_CTRL_PIN    WIND_SPEED_SENSOR_CONF_CTRL_PIN
#else 
#define WIND_SPEED_SENSOR_CTRL_PIN    ADC_SENSORS_ADC5_PIN //PA7
#endif
/*--------------------------------------------------------------------------------*/
#ifdef WIND_DIR_SENSOR_CONF_CTRL_PIN
#define WIND_DIR_SENSOR_CTRL_PIN      WIND_DIR_SENSOR_CONF_CTRL_PIN
#else
#define WIND_DIR_SENSOR_CTRL_PIN      ADC_SENSORS_ADC5_PIN //PA7
#endif
/*--------------------------------------------------------------------------------*/
#endif /* if ADC_SENSORS_CONF_USE_EXTERNAL_ADC*/
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
