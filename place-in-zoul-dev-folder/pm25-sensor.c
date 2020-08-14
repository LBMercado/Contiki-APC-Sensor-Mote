/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include "contiki.h"
#include "adc-sensors.h"
#include "zoul-sensors.h"
#include "dev/pm25-sensor.h"
#include "dev/sys-ctrl.h"
#include "lib/sensors.h"
#include "dev/gpio.h"
#include "dev/ioc.h"
/*---------------------------------------------------------------------------*/
#define PM25_SENSOR_LED_PORT_BASE   GPIO_PORT_TO_BASE(PM25_SENSOR_LED_CTRL_PORT)
#define PM25_SENSOR_LED_PIN_MASK    GPIO_PIN_MASK(PM25_SENSOR_LED_CTRL_PIN)
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#define PM25_SENSOR_OUT_PIN_MASK    GPIO_PIN_MASK(PM25_SENSOR_OUT_CTRL_PIN)
#endif
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
				// note: active low IR LED Pin
				GPIO_SET_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);

				/* configure the output pin*/
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
				if ( adc_zoul.configure(SENSORS_HW_INIT, PM25_SENSOR_OUT_PIN_MASK) == ZOUL_SENSORS_ERROR ){
					PRINTF("PM25-Sensor: failed to configure output pin.\n");
					return PM25_ERROR;
				}
#else
				if ( adc128s022.configure(ADC128S022_INIT, PM25_SENSOR_OUT_EXT_ADC_CHANNEL) == ADC128S022_ERROR ){
					PRINTF("PM25-Sensor: failed to configure output pin.\n");
					return PM25_ERROR;
				}
#endif
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
	// note: active low IR LED Pin
	GPIO_CLR_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);
	clock_delay_usec(PM25_SENSOR_PULSE_DELAY);
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	val = (uint32_t)adc_zoul.value(PM25_SENSOR_OUT_PIN_MASK);
	if(val == ZOUL_SENSORS_ERROR) {
		printf("PM25-Sensor: failed to read from ADC.\n");
		return PM25_ERROR;
	}
	PRINTF("PM25-Sensor: raw adc value: %lu\n", val);
	/* the measured value is with reference to 3v, map it to 5v ref. */
	val *= PM25_ADC_REF;
	val /= PM25_ADC_CROSSREF;
	// the ADC value is significant to the tenth decimal place
#else
	val = (uint32_t)adc128s022.value(PM25_SENSOR_OUT_EXT_ADC_CHANNEL);
	PRINTF("PM25-Sensor: raw adc value: %lu\n", val);
	if(val == ADC128S022_ERROR) {
		printf("PM25-Sensor: failed to read from ADC.\n");
		return PM25_ERROR;
	}
	/* 12 ENOBs ADC, output is digitized level of analog signal*/
	/* convert to 5v ref */
	val *= PM25_ADC_REF;
	val /= ADC128S022_ADC_MAX_LEVEL;
	// the ADC value is significant to the tenth decimal place
#endif
	PRINTF("PM25-Sensor: mv adc value: %lu.%lu\n", val / 10, val % 10);
	//watch out for out of spec values, also consider precision of val
	if      ( (val % 10 > 4 ? val / 10 + 1 : val / 10) < PM25_MIN_OUTPUT_MILLIVOLT){
		PRINTF("WARNING @PM25_SENSOR: output value is not within specifications of sensor. ");
		PRINTF("Minimum value set.\n");
		val = PM25_MIN_OUTPUT_MICRODUST;
	}
	else if ( (val % 10 > 4 ? val / 10 + 1 : val / 10) > PM25_MAX_OUTPUT_MILLIVOLT){
		PRINTF("WARNING @PM25_SENSOR: output value is not within specifications of sensor. ");
		PRINTF("Maximum value set.\n");
		val = PM25_MAX_OUTPUT_MICRODUST;
	}
	else {
		/* conversion mv->ug/m3, output voltage has a linear relationship at specified values with dust density */
		// make sure to account for significant part in value
		val = PM25_MIN_OUTPUT_MICRODUST * 10000 + ( val - PM25_MIN_OUTPUT_MILLIVOLT * 10 )
				* (( PM25_MAX_OUTPUT_MICRODUST - PM25_MIN_OUTPUT_MICRODUST) * 1000 / ( PM25_MAX_OUTPUT_MILLIVOLT - PM25_MIN_OUTPUT_MILLIVOLT ));
		// truncate the significant value
		val /= 10000;
	}
	
	PRINTF("PM25-Sensor: computed dust density: %lu ug/m3\n", val);
	/* clear pulse wave pin */
	GPIO_SET_PIN(PM25_SENSOR_LED_PORT_BASE, PM25_SENSOR_LED_PIN_MASK);

	return (uint16_t)val;
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(pm25, PM25_SENSOR, value, configure, NULL);
/*---------------------------------------------------------------------------*/
