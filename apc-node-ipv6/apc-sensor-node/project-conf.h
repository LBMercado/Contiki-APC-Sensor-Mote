#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*----------------------------------------------------------------*/
/*------------------SENSOR-CONFIGURATION-------------------------*/
/*----------------------------------------------------------------*/
#define MAX_SENSOR_NODES_CONF                                       3
// make a reading every 5 minutes
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF                  300
#define APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS_CONF         180
#define APC_SENSOR_NODE_SINK_DEADTIME_SECONDS_CONF                  360

//enable the disabled ADC channels (ADC1 and ADC3 are enabled by default)
//(refer to board layout for pin configuration)
#define ADC_SENSORS_CONF_ADC2_PIN                                   4
#define ADC_SENSORS_CONF_ADC4_PIN                                   6
#define ADC_SENSORS_CONF_ADC5_PIN                                   7

// disable the user button and bootloader, enable ADC6 (the two features are mutually exclusive)
// WARNING: disabling the bootloader disables reprogramming through USB
/*#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR 1
 *#define ADC_SENSORS_CONF_ADC6_PIN       3
 *#define ADC_SENSORS_CONF_MAX            6
 */
/* DHT22 (Temperature/Humidity)
 * D2 (GPIO)
 */
#define DHT22_CONF_PIN                  2
#define DHT22_CONF_PORT                 GPIO_D_NUM

/* GP2Y1014AUOF (PM2.5/Dust)
 *  -> A5 (ADC Input) (ADC1) (Black cable)
 *  -> D0 (Digital Output, Enable Sensor LED to Measure) (White cable)
 */

#define PM25_SENSOR_LED_CONF_CTRL_PIN       0
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

// Wind Speed Anemometer -> PA7 (ADC5) (Shared)
#define WIND_SPEED_SENSOR_CONF_CTRL_PIN ADC_SENSORS_ADC5_PIN

// Wind Direction Anemometer -> PA7 (ADC5) (Shared)
#define WIND_DIR_SENSOR_CONF_CTRL_PIN   ADC_SENSORS_ADC5_PIN

/* Analog Multiplexer
 * PD1 -> (LSB) 0x00I
 * PC2 ->       0x0I0
 * PC3 -> (MSB) 0xI00
 */
#define APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF      GPIO_D_NUM
#define APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF       1

#define SHARED_SENSOR_MAX_SHARABLE_SENSORS_CONF  2
#define SHARED_SENSOR_MAX_SELECT_LINES_CONF      1

/* reference resistances measured on clean air
 * set to zero to force calibration
*/
#define MQ7_CONF_RO_CLEAN_AIR           0
#define MQ131_CONF_RO_CLEAN_AIR         0
#define MQ135_CONF_RO_CLEAN_AIR         0
/*----------------------------------------------------------------*/
/*------------------------IP-CONFIGURATION------------------------*/
/*----------------------------------------------------------------*/
#define APC_SINK_ADDRESS_CONF           "fd00::212:4B00:1932:E37A"
#define APC_SENSOR_ADDRESS_CONF         "fd00::212:4B00:194A:51E1"
#define UDP_COLLECT_PORT                2001
#define UDP_SINK_PORT                   2002
#define UIP_CONF_SINK_64_BIT            1
#define UIP_CONF_SINK_16_BIT            2
#define UIP_CONF_SINK_MODE              UIP_CONF_SINK_64_BIT
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

/* START - Save some ROM */
/* DO NOT DISABLE, MULTIPLE PACKETS WONT BE SENT SUCCESSFULLY */
#undef UIP_CONF_TCP
#define UIP_CONF_TCP                   1

#undef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG           1
/* END - Save some ROM */

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
