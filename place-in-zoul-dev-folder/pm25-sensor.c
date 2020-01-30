/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include "contiki.h"
#include "adc-sensors.h"
#include "adc-zoul.h"
#include "zoul-sensors.h"
#include "dev/pm25-sensor.h"
#include "dev/sys-ctrl.h"
#include "lib/sensors.h"
#include "dev/gpio.h"
#include "dev/ioc.h"
/*---------------------------------------------------------------------------*/
#define PM25_SENSOR_LED_PORT_BASE   GPIO_PORT_TO_BASE(PM25_SENSOR_LED_CTRL_PORT)
#define PM25_SENSOR_LED_PIN_MASK    GPIO_PIN_MASK(PM25_SENSOR_LED_CTRL_PIN)
#define PM25_SENSOR_OUT_PIN_MASK    GPIO_PIN_MASK(PM25_SENSOR_OUT_CTRL_PIN)
/*---------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
static uint8_t enabled;
/*---------------------------------------------------------------------------*/
static int
configure(int type, int value)
{
	if(type != SENSORS_ACTIVE) {
		PRINTF("PM25-Sensor: configure parameter \'type\' is not SENSORS_ACTIVE.\n");
		return PM25_ERROR;
	}

	switch(value){
		case PM25_ENABLE:
			if ( !enabled ){
				/* configure the led control pin*/
				GPIO_SOFTWARE_CONTROL(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);
				ioc_set_over(PM25_SENSOR_LED_CTRL_PORT, PM25_SENSOR_LED_CTRL_PIN, IOC_OVERRIDE_DIS);
				GPIO_SET_OUTPUT(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);
				GPIO_CLR_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);

				/* configure the output pin*/
				if ( adc_zoul.configure(SENSORS_HW_INIT, PM25_SENSOR_OUT_PIN_MASK) == ZOUL_SENSORS_ERROR ){
					PRINTF("PM25-Sensor: failed to configure output pin.\n");
					return PM25_ERROR;
				}
				enabled = 1;
				return PM25_SUCCESS;
			}
			PRINTF("PM25-Sensor: sensor is already enabled.\n");
			return PM25_SUCCESS;
			break;
		case PM25_DISABLE:
			if ( enabled ){
				enabled = 0;
				PRINTF("PM25-Sensor: sensor disabled.\n");
			}
			return PM25_SUCCESS;
			break;
		default:
			PRINTF("PM25-Sensor: configure parameter \'value\' is invalid.\n");
			return PM25_ERROR;
	}
}
/*---------------------------------------------------------------------------*/
static int
value(int type)
{
	uint32_t val;

	if( !enabled ) {
		return PM25_ERROR;
	}
	/* send a pulse wave to the IR LED Pin then measure */
	GPIO_SET_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);
	clock_delay_usec(PM25_SENSOR_PULSE_DELAY);
	val = (uint32_t)adc_zoul.value(PM25_SENSOR_OUT_PIN_MASK);
        
	if(val == ZOUL_SENSORS_ERROR) {
		printf("PM25-Sensor: failed to read from ADC.\n");
		return PM25_ERROR;
	}
	PRINTF("PM25-Sensor: raw adc value: %lu\n", val);

	if ( val > PM25_ADC_MAX_VAL)
		val = PM25_ADC_MAX_VAL;
	val *= PM25_ADC_REF;
	val /= PM25_ADC_MAX_VAL;
	PRINTF("PM25-Sensor: mv adc value: %lu\n", val);
	//watch out for out of spec values
	if      (val < PM25_MIN_OUTPUT_MILLIVOLT){
		PRINTF("WARNING @PM25_SENSOR: output value is not within specifications of sensor. ");
		PRINTF("Minimum value set.\n");
		val = PM25_MIN_OUTPUT_MILLIVOLT;
	}
	else if (val > PM25_MAX_OUTPUT_MILLIVOLT){
		PRINTF("WARNING @PM25_SENSOR: output value is not within specifications of sensor. ");
		PRINTF("Maximum value set.\n");
		val = PM25_MAX_OUTPUT_MILLIVOLT;
	}

	/* conversion mv->ug/m3, output voltage has a linear relationship at specified values with dust density */
	val = PM25_MIN_OUTPUT_MICRODUST + ( (val - PM25_MIN_OUTPUT_MILLIVOLT) * (PM25_MAX_OUTPUT_MICRODUST - PM25_MIN_OUTPUT_MICRODUST) )/ ( PM25_MAX_OUTPUT_MILLIVOLT - PM25_MIN_OUTPUT_MILLIVOLT );
	PRINTF("PM25-Sensor: computed dust density: %lu\n", val);
	/* clear pulse wave pin */
	GPIO_CLR_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);

	return (uint16_t)val;
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(pm25, PM25_SENSOR, value, configure, NULL);
/*---------------------------------------------------------------------------*/
