#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*----------------------------------------------------------------*/
/*------------------SENSOR-CONFIGURATION-------------------------*/
/*----------------------------------------------------------------*/
// make a reading every 5 minutes
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF                  300

/* Use an external ADC chip (ADC128S022)
 * - Set to 1 to use external ADC chip, refer to the header file (adc128s022.h) for setting up the adc chip driver
 * - Set to 0 to use internal (Zoul) ADC, uncomment and modify the pin configs below
 * */
#define ADC_SENSORS_CONF_USE_EXTERNAL_ADC                           1

/* ADC channels reserved for the following sensors */
#define MQ131_SENSOR_CONF_EXT_ADC_CHANNEL                           2
#define MICS4514_SENSOR_RED_CONF_EXT_ADC_CHANNEL                    1
#define MICS4514_SENSOR_NOX_CONF_EXT_ADC_CHANNEL                    0
#define WIND_SPEED_SENSOR_CONF_EXT_ADC_CHANNEL                      3
#define WIND_DIR_SENSOR_CONF_EXT_ADC_CHANNEL                        4
#define PM25_SENSOR_OUT_CONF_EXT_ADC_CHANNEL                        5

// disable the user button and bootloader, enable ADC6 (the two features are mutually exclusive)
// WARNING: disabling the bootloader disables reprogramming through USB
//#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR 1
//#define ADC_SENSORS_CONF_ADC6_PIN       3
//#define ADC_SENSORS_CONF_MAX            6

/* DHT22 (Temperature/Humidity)
 * PA7 (ANALOG_IN/GPIO)
 */
#define DHT22_CONF_PIN                  7
#define DHT22_CONF_PORT                 GPIO_A_NUM

/* GP2Y1014AUOF (PM2.5/Dust)
 *  -> PA5 (ANALOG_IN/GPIO)(LED Enable)
 */

#define PM25_SENSOR_LED_CONF_CTRL_PIN       5
#define PM25_SENSOR_LED_CONF_CTRL_PORT      GPIO_A_NUM
//#define PM25_SENSOR_OUT_CONF_CTRL_PIN       ADC_SENSORS_ADC1_PIN

/* MICS4514 (CO2/RED) (NOx/NOX)
 * -> PA6 (ANALOG_IN/GPIO) (PREHEAT Enable)
 * */
#define MICS4514_SENSOR_HEATING_CONF_PIN       6
#define MICS4514_SENSOR_HEATING_CONF_PORT      GPIO_A_NUM
//#define MICS4514_SENSOR_RED_CONF_CTRL_PIN      ADC_SENSORS_ADC3_PIN
//#define MICS4514_SENSOR_NOX_CONF_CTRL_PIN      ADC_SENSORS_ADC2_PIN

// MQ131 (O3) -> PA6 (ADC4)
//#define MQ131_SENSOR_CONF_CTRL_PIN             ADC_SENSORS_ADC4_PIN

// Wind Speed Anemometer -> PA7 (ADC5) (Shared)
//#define WIND_SPEED_SENSOR_CONF_CTRL_PIN        ADC_SENSORS_ADC5_PIN

// Wind Direction Anemometer -> PA7 (ADC5) (Shared)
//#define WIND_DIR_SENSOR_CONF_CTRL_PIN          ADC_SENSORS_ADC5_PIN

/* Analog Multiplexer
 * PD1 -> (LSB) 0x00I
 * PC2 ->       0x0I0
 * PC3 -> (MSB) 0xI00
 */
//#define APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF      GPIO_D_NUM
//#define APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF       1
//#define SHARED_SENSOR_MAX_SELECT_LINES_CONF      1

/* reference resistances measured on clean air
 * set to zero to force calibration
*/
#define MQ131_CONF_RO_CLEAN_AIR         0
#define MICS4514_NOX_CONF_RO_CLEAN_AIR  0
#define MICS4514_RED_CONF_RO_CLEAN_AIR  0

/* load resistances for forming the voltage divider in measuring
 * sensor output
 */
#define MQ131_CONF_RL_KOHM              1000
#define MICS4514_RED_RL_KOHM            47
#define MICS4514_NOX_RL_KOHM            22

/*----------------------------------------------------------------*/
/*------------------------IP-CONFIGURATION------------------------*/
/*----------------------------------------------------------------*/
#define MQTT_CONF_BROKER_IP_ADDR        "FD00::1"
#define MOTE_ID                         113
#if MOTE_ID == 056
#define APC_SENSOR_ADDRESS_CONF         "FD00::212:4B00:194A:51E1"
#define APC_SENSOR_MOTE_ID_CONF         "056"
#elif MOTE_ID == 113
#define APC_SENSOR_ADDRESS_CONF         "FD00::212:4B00:194A:5174"
#define APC_SENSOR_MOTE_ID_CONF         "113"
#endif
#define MQTT_CONF_STATUS_LED            LEDS_WHITE
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
#define UIP_CONF_MAX_ROUTES              40
#else
/* configure number of neighbors and routes */
#define NBR_TABLE_CONF_MAX_NEIGHBORS     5
#define UIP_CONF_MAX_ROUTES              10
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
#define RPL_NS_CONF_LINK_NUM 10 /* Number of links maintained at the root. Can be set to 0 at non-root nodes. */
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES 0 /* No need for routes */
#undef RPL_CONF_MOP
#define RPL_CONF_MOP RPL_MOP_NON_STORING /* Mode of operation*/
#endif /* WITH_NON_STORING */
/*----------------------------------------------------------------*/
/*This code was taken from rpl-collect example found in example/ipv6*/
/*----------------------------------------------------------------*/
#endif
