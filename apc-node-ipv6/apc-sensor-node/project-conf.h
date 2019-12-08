#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

//port to use for listening/sending messages
#define UDP_COLLECT_PORT                2001
#define UDP_SINK_PORT                   2002
#define UIP_CONF_ROUTER_64_BIT          1
#define UIP_CONF_ROUTER_16_BIT          2
#define UIP_CONF_ROUTER_MODE            UIP_CONF_ROUTER_16_BIT

//enable the disabled ADC channels (ADC1 and ADC3 are enabled by default)
//(refer to board layout for pin configuration)
#define ADC_SENSORS_CONF_ADC2_PIN       4
#define ADC_SENSORS_CONF_ADC4_PIN       6
#define ADC_SENSORS_CONF_ADC5_PIN       7

//disable the user button and bootloader, enable ADC6 (the two features are mutually exclusive)
#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR 0
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

/* reference resistances measured at clean air
*  set to zero to force calibration
*/
#define MQ7_CONF_RO_CLEAN_AIR           500
#define MQ131_CONF_RO_CLEAN_AIR         500
#define MQ135_CONF_RO_CLEAN_AIR         500

/*----------------------------------------------------------------*/
/*This code was taken from rpl-collect example found in example/ipv6*/
/*----------------------------------------------------------------*/
#ifndef WITH_NON_STORING
#define WITH_NON_STORING 0 /* Set this to run with non-storing mode */
#endif /* WITH_NON_STORING */

#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#undef UIP_CONF_MAX_ROUTES

#ifdef TEST_MORE_ROUTES
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     10
#define UIP_CONF_MAX_ROUTES   30
#else
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     10
#define UIP_CONF_MAX_ROUTES   10
#endif /* TEST_MORE_ROUTES */

#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     nullrdc_driver
#undef NULLRDC_CONF_802154_AUTOACK
#define NULLRDC_CONF_802154_AUTOACK       1

/* Define as minutes */
#define RPL_CONF_DEFAULT_LIFETIME_UNIT   60

/* 10 minutes lifetime of routes */
#define RPL_CONF_DEFAULT_LIFETIME        10

#define RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME 1

/* Save some ROM */
#undef UIP_CONF_TCP
#define UIP_CONF_TCP                   0

#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG           0

#if WITH_NON_STORING
#undef RPL_NS_CONF_LINK_NUM
#define RPL_NS_CONF_LINK_NUM 40 /* Number of links maintained at the root. Can be set to 0 at non-root nodes. */
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 0 /* No need for routes */
#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_NON_STORING /* Mode of operation*/
#endif /* WITH_NON_STORING */
/*----------------------------------------------------------------*/
/*This code was taken from rpl-collect example found in example/ipv6*/
/*----------------------------------------------------------------*/

#endif
