#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

//enable the disabled ADC channels (ADC1 and ADC3 are enabled by default)
//(refer to board layout for pin configuration)
#define ADC_SENSORS_CONF_ADC2_PIN       4
#define ADC_SENSORS_CONF_ADC4_PIN       6
#define ADC_SENSORS_CONF_ADC5_PIN       7

//disable the user button, enable ADC6 (the two are mutually exclusive)
#define ADC_SENSORS_CONF_ADC6_PIN       3
#define ADC_SENSORS_CONF_MAX            6

/* apc-sensor-node config */
/* DHT22 (Temperature/Humidity)
	D1 (Digital Input/Output) 
*/
#define DHT22_CONF_PIN                  1
#define DHT22_CONF_PORT                 GPIO_D_NUM

/* 
   GP2Y1014AUOF -> A5 (ADC Input) (ADC1)
		-> D2 (Digital Output, Enable Sensor LED to Measure)
		(PM2.5)
 */

#define PM25_SENSOR_LED_CONF_CTRL_PIN       2
#define PM25_SENSOR_LED_CONF_CTRL_PORT      GPIO_D_NUM
#define PM25_SENSOR_OUT_CONF_CTRL_PIN       ADC_SENSORS_ADC1_PIN

// MQ7 (CO) -> PA2 (ADC3)
#define MQ7_SENSOR_CONF_CTRL_PIN        ADC_SENSORS_ADC3_PIN
// heater -> PC0
#define MQ7_SENSOR_HEATING_CONF_PIN     0
#define MQ7_SENSOR_HEATING_CONF_PORT    GPIO_C_NUM

// MQ131 (O3) -> PA6 (ADC4)
#define MQ131_SENSOR_CONF_CTRL_PIN      ADC_SENSORS_ADC4_PIN

// MQ135 (NOx/No2/CO2) -> PA4 (ADC2)
#define MQ135_SENSOR_CONF_CTRL_PIN      ADC_SENSORS_ADC2_PIN

// Wind Speed Anemometer -> PA7 (ADC5)
#define WIND_SPEED_SENSOR_CONF_CTRL_PIN ADC_SENSORS_ADC5_PIN

// Wind Direction Anemometer -> PA3 (ADC6) (Button is Disabled)
#define WIND_DIR_SENSOR_CONF_CTRL_PIN   ADC_SENSORS_ADC6_PIN

// reference resistances measured at clean air
#define MQ7_CONF_RO_CLEAN_AIR           500
#define MQ131_CONF_RO_CLEAN_AIR         500
#define MQ135_CONF_RO_CLEAN_AIR         500

#endif
