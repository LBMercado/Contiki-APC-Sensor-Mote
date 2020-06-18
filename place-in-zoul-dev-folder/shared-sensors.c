/*
 * shared-sensors.c
 *
 * Allows sharing of analog sensors using a single analog pin,
 * take note that digital pins are needed to select a sensor.
 * This makes use of the analog multiplexer, 74HC4051 to work.
 * Current implementation considers the anemometer sensors,
 * though more can be added if required.
 * Select pins (except LSB) can be disabled by
 * reducing pin count.
 *
 *  Created on: May 19, 2020
 *      Author: luis-mercado
 */
/* --------------------------------------------- */
#include "dev/shared-sensors.h"
#include "dev/zoul-sensors.h"
#include "dev/adc-sensors.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "dev/gpio.h"
#include <stdio.h>
/* --------------------------------------------- */
#define DEBUG 0
#if DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif
/* --------------------------------------------- */
static uint32_t select_port_base[3];
static uint32_t select_pin_mask[3];
/* --------------------------------------------- */
typedef struct shared_sensor {
	/* The ->next pointer is needed since we are placing these on a
     * Contiki list. */
	struct shared_sensor *next;
	uint8_t select_pin;
	const struct sensors_sensor *sensor;
} shared_sensor_t;
/* --------------------------------------------- */
MEMB(shared_sensors_memb, shared_sensor_t, SHARED_SENSOR_MAX_SHARABLE_SENSORS);
LIST(shared_sensors_list);
/* --------------------------------------------- */
enum SHARED_SENSOR_TYPE shared_sensor_type[SHARED_SENSOR_MAX_SHARABLE_SENSORS] = {
		SHARED_ANEM_DIRECTION,
		SHARED_ANEM_SPEED
};
/* --------------------------------------------- */
// 3-bit select pin for 74HC4051
static uint8_t select_pin_count = 0;
static uint8_t sharable_sensor_count = 0;
/* --------------------------------------------- */
void shared_sensor_init(uint8_t select_count){
	if (select_count > SHARED_SENSOR_MAX_SELECT_LINES) {
		PRINTF("Failed to initialize shared sensor module. Argument \'select_count\' exceeds maximum select pins.\n");
		return;
	}
	select_pin_count = select_count;

	memb_init(&shared_sensors_memb);
	list_init(shared_sensors_list);

	sharable_sensor_count = 2;
	if (select_pin_count > 1)
		for (int i = 0; i < select_pin_count - 1; i++) {
			sharable_sensor_count *= 2;
		}

	PRINTF("Sharable Sensor Count: %u\n", sharable_sensor_count);
	for (int i = 0; i < sharable_sensor_count; i++) {
		shared_sensor_t *sensor;

		sensor = memb_alloc(&shared_sensors_memb);
		if(sensor == NULL) {
			PRINTF("Failed to allocate memory for entry.\n");
			continue;
		}

		sensor->select_pin = i;
		sensor->sensor = NULL;
		list_add(shared_sensors_list, sensor);
		PRINTF("Shared Sensor Init: Pin = %hu\n", sensor->select_pin);
	}
	PRINTF("Initialized shared sensor list: length = %hu\n", list_length(shared_sensors_list));

	for (int i = 0; i < select_pin_count; i++){
		select_port_base[i] = 0;
		select_pin_mask[i] = 0;
	}
}
/* --------------------------------------------- */
/*
 * Configures the select pin at the specified bit position with the specified pin and port number.
 * @param: bit - position of the select pin
 * @param: pin - pin number
 * @param: port - port number
 * */
shared_sensor_result shared_sensor_configure_select_pin(uint8_t bit, uint8_t pin, uint8_t port){
	if (select_pin_count == 0) {
		PRINTF("Failed to configure select pin. Shared sensor is not initialized.\n");
		return SHARED_SENSOR_FAIL;
	}
	if (bit > select_pin_count - 1) {
		PRINTF("Failed to configure select pin. Argument \'bit\' out of bounds.\n");
		return SHARED_SENSOR_FAIL;
	}
	select_port_base[bit] = GPIO_PORT_TO_BASE(port);
	select_pin_mask[bit] = GPIO_PIN_MASK(pin);

	GPIO_SOFTWARE_CONTROL(select_port_base[bit], select_pin_mask[bit]);
	GPIO_SET_OUTPUT(select_port_base[bit], select_pin_mask[bit]);
	GPIO_CLR_PIN(select_port_base[bit], select_pin_mask[bit]);

	return SHARED_SENSOR_SUCCESS;
}
/* --------------------------------------------- */
shared_sensor_result shared_sensor_share_pin(const struct sensors_sensor *sensor, uint8_t select_pin){
	shared_sensor_t *n;

	if (select_pin_count == 0) {
		PRINTF("Failed to add sensor. Shared sensor is not initialized.\n");
		return SHARED_SENSOR_FAIL;
	}

	PRINTF("accessing shared sensor list: length = %hu\n", list_length(shared_sensors_list));
	for(n = list_head(shared_sensors_list); n != NULL; n = list_item_next(n)) {
		PRINTF("Searching shared sensor for pin %hu, current pin = %hu\n", select_pin, n->select_pin);
		if(n->select_pin == select_pin) {
			n->sensor = sensor;
			PRINTF("Shared sensor assigned: pin = %hu\n", n->select_pin);
			return SHARED_SENSOR_SUCCESS;
		}
	}

	PRINTF("Failed to add sensor, argument \'select_pin\' does not exist.\n");
	return SHARED_SENSOR_FAIL;
}
/* --------------------------------------------- */
static void drive_select_line(uint8_t select_line){
	if (select_line > sharable_sensor_count - 1){
		PRINTF("WARNING! Cannot drive select line that does not exist.\n");
		return;
	}

	for (int i = 0; i < select_pin_count; i++){
		if (select_port_base[i] == 0 || select_pin_mask[i] == 0){
			PRINTF("WARNING! Select pins are unconfigured.\n");
			return;
		}

		uint8_t lsb = 1 & select_line;
		if (lsb){
			PRINTF("Select bit %hu driven high\n", i);
			GPIO_SET_PIN(select_port_base[i], select_pin_mask[i]);
		} else {
			PRINTF("Select bit %hu driven low\n", i);
			GPIO_CLR_PIN(select_port_base[i], select_pin_mask[i]);
		}
		select_line = select_line >> 1;
	}
}
/* --------------------------------------------- */
const struct sensors_sensor* shared_sensor_select_pin(uint8_t select_pin){
	shared_sensor_t *n;

	if (select_pin_count == 0) {
		PRINTF("Failed to select pin. Shared sensor is not initialized.\n");
		return NULL;
	}

	for(n = list_head(shared_sensors_list); n != NULL; n = list_item_next(n)) {
		if(n->select_pin == select_pin) {
			drive_select_line(select_pin);
			PRINTF("Line %hu selected for shared sensor.\n", select_pin);
			return n->sensor;
		}
	}
	return NULL;
}
/* --------------------------------------------- */
shared_sensor_result shared_sensor_unshare_sensor(uint8_t select_pin){
	shared_sensor_t *n;

	if (select_pin_count == 0) {
		PRINTF("Failed to remove sensor. Shared sensor is not initialized.\n");
		return SHARED_SENSOR_FAIL;
	}

	for(n = list_head(shared_sensors_list); n != NULL; n = list_item_next(n)) {
		if(n->select_pin == select_pin) {
			n->sensor = NULL;
			PRINTF("Shared sensor with pin %hu removed successfully. \n", n->select_pin);
			return SHARED_SENSOR_SUCCESS;
		}
	}
	PRINTF("Failed to remove sensor, argument \'select_pin\' does not exist.\n");
	return SHARED_SENSOR_FAIL;
}
/* --------------------------------------------- */
