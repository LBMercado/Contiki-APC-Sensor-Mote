/*
 * shared-sensors.h
 *
 *  Created on: May 19, 2020
 *      Author: ubuntu-mercado
 */
/*---------------------------------------------------*/
#ifndef PLATFORM_ZOUL_DEV_SHARED_SENSORS_H_
#define PLATFORM_ZOUL_DEV_SHARED_SENSORS_H_
/*---------------------------------------------------*/
#include "contiki.h"
#include "lib/sensors.h"
/*---------------------------------------------------*/
#ifndef SHARED_SENSOR_MAX_SELECT_LINES_CONF
#define SHARED_SENSOR_MAX_SELECT_LINES          3
#else
#define SHARED_SENSOR_MAX_SELECT_LINES          SHARED_SENSOR_MAX_SELECT_LINES_CONF
#endif
/* --------------------------------------------------*/
extern uint8_t SHARED_SENSOR_MAX_SHARABLE_SENSORS;
/*---------------------------------------------------*/
#define SHARED_SENSOR_SUCCESS                   0x00
#define SHARED_SENSOR_FAIL                      (-1)
/*---------------------------------------------------*/
typedef int shared_sensor_result;
/*---------------------------------------------------*/
void shared_sensor_init(uint8_t select_count);
shared_sensor_result shared_sensor_configure_select_pin(uint8_t bit, uint8_t pin, uint8_t port);
shared_sensor_result shared_sensor_share_pin(const struct sensors_sensor *sensor, uint8_t select_pin);
const struct sensors_sensor* shared_sensor_select_pin(uint8_t select_pin);
shared_sensor_result shared_sensor_unshare_sensor(uint8_t select_pin);
/*---------------------------------------------------*/
#endif /* PLATFORM_ZOUL_DEV_SHARED_SENSORS_H_ */
