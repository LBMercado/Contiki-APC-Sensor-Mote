//Include files start
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "dev/leds.h"
#include "dev/gpio.h"
#include "sys/etimer.h"
#include "dev/zoul-sensors.h"
#include "apc-sensor-node-test.h"
//Include files end

//function definition start

static int
read_sensor
(uint8_t sensorType, uint8_t nodeNumber) {

	int value1, value2;

	switch(sensorType){
		//HUMIDITY_T and TEMPERATURE_T are read at the same sensor
	case HUMIDITY_T:
		nodeNumber--; // humidity is one index above temperature in sensor reading
	case TEMPERATURE_T:
		//Assume it is busy
		do{
			value1 = dht22.value(DHT22_READ_ALL);
			if (value1 == DHT22_BUSY)
			printf("Sensor is busy, retrying...\n");
		} while (value1 == DHT22_BUSY);
		if (dht22_read_all(&value1, &value2) != DHT22_ERROR) {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%02d", 
			value1 / 10, value2 % 10
			);
			sprintf(sn_readings.sensor_node_reading[nodeNumber + 1].data, 
			"%02d.%02d", 
			value2 / 10, value2 % 10
			);
			printf("-----------------\n");
			printf("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			printf("Temperature %02d.%d deg. C, ", value1 / 10, value1 % 10);
			printf("Humidity %02d.%d RH\n", value2 / 10, value2 % 10);
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		else {
			printf("-----------------\n");
			printf("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		break;
	case PM25_T:
		//pm sensor only measures one value, parameter does not do anything here
		value1 = pm25.value(0);
		if (value1 == PM25_ERROR){
			printf("-----------------\n");
			printf("read_sensor: PM25_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d", 
			value1
			);
			printf("-----------------\n");
			printf("read_sensor: PM25_T \n");
			printf("Dust Density: %d ug/m3\n", value1);
			printf("-----------------\n");
		return OPERATION_SUCCESS;
		}
		break;
	case CO_T:
		value1 = aqs_sensor.value(MQ7_SENSOR);
		if (value1 == AQS_ERROR){
			printf("-----------------\n");
			printf("read_sensor: CO_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			printf("-----------------\n");
			printf("read_sensor: CO_T \n");
			printf("sensor is busy.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			printf("-----------------\n");
			printf("read_sensor: CO_T \n");
			printf("Rs/Ro: %d.%03d\n",
				value1 / 1000,
				value1 % 1000);
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case CO2_T:
		value1 = aqs_sensor.value(MQ135_SENSOR);
		if (value1 == AQS_ERROR){
			printf("-----------------\n");
			printf("read_sensor: CO2_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			printf("-----------------\n");
			printf("read_sensor: CO2_T \n");
			printf("sensor is busy.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			printf("-----------------\n");
			printf("read_sensor: CO2_T \n");
			printf("Rs/Ro: %d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case O3_T:
		value1 = aqs_sensor.value(MQ131_SENSOR);
		if (value1 == AQS_ERROR){
			printf("-----------------\n");
			printf("read_sensor: O3_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			printf("-----------------\n");
			printf("read_sensor: O3_T \n");
			printf("sensor is busy.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			printf("-----------------\n");
			printf("read_sensor: O3_T \n");
			printf("Rs/Ro: %d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_SPEED_T:
		value1 = anem_sensor.value(WIND_SPEED_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			printf("-----------------\n");
			printf("read_sensor: WIND_SPEED_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%d.%02d", 
				value1 / 100, //whole component
				value1 % 100 //frac component
			);
			printf("-----------------\n");
			printf("read_sensor: WIND_SPEED_T \n");
			printf("WIND SPEED: %d.%02d m/s\n", 
				value1 / 100, //whole component
				value1 % 100); //frac component
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_DRCTN_T:
		value1 = anem_sensor.value(WIND_DIR_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			printf("-----------------\n");
			printf("read_sensor: WIND_DRCTN_T \n");
			printf("Failed to read sensor.\n");
			printf("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			printf("-----------------\n");
			printf("read_sensor: WIND_DRCTN_T \n");
			printf("WIND DRCTN (Raw): 0x%04x\n", value1);
			switch (value1){
				case WIND_DIR_NORTH:
					printf("WIND DRCTN (Actual): NORTH\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"N");
					break;
				case WIND_DIR_EAST:
					printf("WIND DRCTN (Actual): EAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"E");
					break;
				case WIND_DIR_SOUTH:
					printf("WIND DRCTN (Actual): SOUTH\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"S");
					break;
				case WIND_DIR_WEST:
					printf("WIND DRCTN (Actual): WEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"W");
					break;
				case WIND_DIR_NORTH | WIND_DIR_EAST:
					printf("WIND DRCTN (Actual): NORTHEAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"NE");
					break;
				case WIND_DIR_NORTH | WIND_DIR_WEST:
					printf("WIND DRCTN (Actual): NORTHWEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"NW");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_EAST:
					printf("WIND DRCTN (Actual): SOUTHEAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"SE");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_WEST:
					printf("WIND DRCTN (Actual): SOUTHWEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"SW");
					break;
				default:
					printf("ERROR: Unexpected value.");
					break;
			}
			printf("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	default:
		return OPERATION_FAILED;
	}
}

//function definition end
//Process start
PROCESS(apc_sensor_node_test_process, "APC Sensor Node Test Process");
AUTOSTART_PROCESSES(&apc_sensor_node_test_process);

PROCESS_THREAD(apc_sensor_node_test_process, ev, data) {
	//initialization
	static struct etimer et;
	static uint16_t count = 0;

	//preparations are complete
	PROCESS_BEGIN();
	printf("APC Sensor Node Test begins...\n");
	leds_blink();
	leds_off(LEDS_ALL);

	//enable the sensors
	printf("apc_sensor_node_test_process: enabling sensors...\n\n");
	printf("ADC1_PIN: PA(%d)", ADC_SENSORS_ADC1_PIN);
	printf(" -> %s\n", ADC_SENSORS_ADC1_PIN > 0 ? "OK\0" : "ERROR\0");
	printf("ADC2_PIN: PA(%d)", ADC_SENSORS_ADC2_PIN);
	printf(" -> %s\n", ADC_SENSORS_ADC2_PIN > 0 ? "OK\0" : "ERROR\0");
	printf("ADC3_PIN: PA(%d)", ADC_SENSORS_ADC3_PIN);
	printf(" -> %s\n", ADC_SENSORS_ADC3_PIN > 0 ? "OK\0" : "ERROR\0");
	printf("ADC4_PIN: PA(%d)", ADC_SENSORS_ADC4_PIN);
	printf(" -> %s\n", ADC_SENSORS_ADC4_PIN > 0 ? "OK\0" : "ERROR\0");
	printf("ADC5_PIN: PA(%d)", ADC_SENSORS_ADC5_PIN);
	printf(" -> %s\n", ADC_SENSORS_ADC5_PIN > 0 ? "OK\0" : "ERROR\0");
	printf("ADC6_PIN: PA(%d)", ADC_SENSORS_ADC6_PIN);
	printf(" -> %s\n\n", ADC_SENSORS_ADC6_PIN > 0 ? "OK\0" : "ERROR\0");

	printf("TEMPERATURE_T & HUMIDITY_T: %s\n", SENSORS_ACTIVATE(dht22) == DHT22_ERROR 
	? "ERROR\0" : "OK\0");
	printf("PM25_T: %s\n", pm25.configure(SENSORS_ACTIVE, PM25_ENABLE)  == PM25_ERROR ? "ERROR\0" : "OK\0");
	printf("CO_T: %s\n", aqs_sensor.configure(SENSORS_ACTIVE, MQ7_SENSOR) == AQS_ERROR ? "ERROR\0" : "OK\0");
	printf("O3_T: %s\n", aqs_sensor.configure(SENSORS_ACTIVE, MQ131_SENSOR) == AQS_ERROR ? "ERROR\0" : "OK\0");
	printf("CO2_T: %s\n", aqs_sensor.configure(SENSORS_ACTIVE, MQ135_SENSOR) == AQS_ERROR ? "ERROR\0" : "OK\0");
	printf("WIND_SENSOR_T: %s\n", anem_sensor.configure(SENSORS_ACTIVE, WIND_SPEED_SENSOR) == WIND_SENSOR_ERROR ? "ERROR\0" : "OK\0");
	printf("WIND_DIR_T: %s\n", anem_sensor.configure(SENSORS_ACTIVE, WIND_DIR_SENSOR) == WIND_SENSOR_ERROR ? "ERROR\0" : "OK\0");

	printf("Initialization check------------\n");
	printf("delaying for 5 seconds------------\n");
	etimer_set(&et, CLOCK_SECOND * 5);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	printf("AQS is initializing...\n");
	while(aqs_sensor.status(MQ7_SENSOR) == AQS_INIT_PHASE || aqs_sensor.status(MQ7_SENSOR) == AQS_BUSY ||
		aqs_sensor.status(MQ131_SENSOR) == AQS_INIT_PHASE || aqs_sensor.status(MQ7_SENSOR) == AQS_BUSY || 
		aqs_sensor.status(MQ135_SENSOR) == AQS_INIT_PHASE || aqs_sensor.status(MQ7_SENSOR) == AQS_BUSY){
		etimer_set(&et, CLOCK_SECOND * WAIT_BUSY_PERIOD);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}
	printf("AQS finished initialization.!\n");
	printf("apc_sensor_node_test_process: all sensors enabled/initialized.\n");

	//initialize sensor node readings
	int i;
	for (i = 0; i < SENSOR_COUNT; i++) {
		sn_readings.sensor_node_reading[i].type = SENSOR_TYPES[i];
	}
	
	while (1) {
		printf("-----------------Sample#%04x-----------------\n", count + 1);
		for (i = 0; i < SENSOR_COUNT; i++) {
				leds_toggle(LEDS_GREEN);

				/* failed to read sensor */
				if (!read_sensor(sn_readings.sensor_node_reading[i].type, i)) {
					leds_off(LEDS_ALL);
					leds_on(LEDS_RED);
					
					printf("****SENSOR_FAIL - faulting sensor type: %u\n", 
						sn_readings.sensor_node_reading[i].type);
					
					leds_off(LEDS_RED);
				}
				/* successfully read sensor */
				else {
					
					printf("****SENSOR_READ - header type %u and value %s\n", 
						sn_readings.sensor_node_reading[i].type,
						sn_readings.sensor_node_reading[i].data);
				}
			}
		count++;
		etimer_set(&et, CLOCK_SECOND * SAMPLING_PERIOD);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}
	PROCESS_END();
}

//Process end
