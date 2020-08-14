/* C std libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
/* Contiki Core and Networking Libraries */
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "net/rpl/rpl.h"
#include "rpl/rpl-private.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
/* Contiki Dev and Utilities */
#include "dev/dht22.h"
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#include "dev/pm25-sensor.h"
#include "dev/leds.h"
#include "dev/cc2538-sensors.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/timer.h"
/* Contiki Apps */
#include "mqtt.h"
/* Project Sourcefiles */
#include "apc-sensor-node.h"
#include "dev/air-quality-sensor.h"
#include "dev/anemometer-sensor.h"
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#include "dev/shared-sensors.h"
#endif
/*---------------------------------------------------------------------------*/
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
/*---------------------------------------------------------------------------*/
#ifdef APC_SENSOR_ADDRESS_CONF
#define APC_SENSOR_ADDRESS APC_SENSOR_ADDRESS_CONF
#else
#define APC_SENSOR_ADDRESS ""
#endif
/*----------------------------------------------------------------------------------*/
#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_CONF_IPV6_RPL
#error "APC-Sink-Node will be unable to function properly with this current contiki configuration."
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif
/*----------------------------------------------------------------------------------*/
const uint8_t SENSOR_TYPES[SENSOR_COUNT] = {
	TEMPERATURE_T, //unit in Deg. Celsius
	HUMIDITY_T, //unit in %RH
	PM25_T, //unit in microgram per m3
	CO_T, //unitless, raw resistance ratio to compute ppm
	NO2_T, //unitless, raw resistance ratio to compute ppm
	O3_T, //unitless, raw resistance ratio to compute ppm
	WIND_SPEED_T, //unit in m/s
	WIND_DRCTN_T //may be N, S, E, W and their combinations
};
/*----------------------------------------------------------------------------------*/
const uint8_t SENSOR_CALIB_TYPES[SENSOR_CALIB_COUNT] =
{
	CO_RO_T, //unit in ohms, sf. by 3 digits
	NO2_RO_T, //unit in ohms, sf. by 3 digits
	O3_RO_T //unit in ohms, sf. by 3 digits
};
/*----------------------------------------------------------------------------------*/
/* Collect data from sensors at spaced intervals*/
#ifndef APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS             180
#else
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS             APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF
#endif
/*----------------------------------------------------------------------------------*/
/* Identifier for this mote, used for MQTT subtopic */
#define APC_SENSOR_TOPIC_NAME                             "apc-iot"
#ifdef  APC_SENSOR_MOTE_ID_CONF
#define APC_SENSOR_MOTE_ID                                APC_SENSOR_MOTE_ID_CONF
#else
#define APC_SENSOR_MOTE_ID                                "mote"
#endif
/*----------------------------------------------------------------------------------*/
/* Minimum for dht22 read intervals, consider slowest sensor */
#define APC_SENSOR_NODE_READ_WAIT_MILLIS                  CLOCK_SECOND >> 2
/*----------------------------------------------------------------------------------*/
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#ifndef APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF
#define APC_SENSOR_NODE_AMUX_0BIT_PORT                    GPIO_D_NUM
#else
#define APC_SENSOR_NODE_AMUX_0BIT_PORT                    APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF
#endif
/*----------------------------------------------------------------------------------*/
#ifndef APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF
#define APC_SENSOR_NODE_AMUX_0BIT_PIN                     1
#else
#define APC_SENSOR_NODE_AMUX_0BIT_PIN                     APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF
#endif
/*----------------------------------------------------------------------------------*/
#define APC_SENSOR_NODE_AMUX_SELECT_ANEMOMETER            0
#define APC_SENSOR_NODE_AMUX_SELECT_WIND_VANE             1
#endif /* if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC */
/*----------------------------------------------------------------------------------*/
#define UIP_IP_BUF                                        ( (struct uip_ip_hdr*) & uip_buf[UIP_LLH_LEN] )
#define UDP_CLIENT_PORT	6000
#define UDP_SERVER_PORT	6001
#define UDP_EXAMPLE_ID  190
static struct uip_udp_conn *sink_conn;
/*----------------------------------------------------------------------------------*/
static const char *SENSOR_TYPE_HEADERS[] = {
		"Temperature (°C)",
		"Humidity (%RH)",
		"PM25 (ug/m3)",
		"CO (Rs/Ro)",
		"NO2 (Rs/Ro)",
		"O3 (Rs/Ro)",
		"Wind Speed (m/s)",
		"Wind Direction"
};
static const char *SENSOR_CALIB_TYPE_HEADERS[] = {
		"CO Rs (Ohms)",
		"NO2 Rs (Ohms)",
		"O3 Rs (Ohms)"
};
/*----------------------------------------------------------------------------------*/
typedef struct{
	uint8_t sensor_type;
	uint8_t is_calibrated; //used only during calibration procedures
	char sensor_reading[10];
	char sensor_calib_reading[12]; //used only for read_calib_sensor
} sensor_info_t;
/*----------------------------------------------------------------------------------*/
static sensor_info_t sensor_infos[SENSOR_COUNT];
/*----------------------------------------------------------------------------------*/
static uip_ipaddr_t collect_addr;
static uip_ipaddr_t prefix;
/*----------------------------------------------------------------------------------*/
static uint32_t seqNo;
static struct etimer et_collect;
/*----------------------------------------------------------------------------------*/
/* MQTT-specific Configuration START (code copied from cc2538-common/mqtt-demo)*/
/*
* IBM server: messaging.quickstart.internetofthings.ibmcloud.com
* (184.172.124.189) mapped in an NAT64 (prefix 64:ff9b::/96) IPv6 address
* Note: If not able to connect; lookup the IP address again as it may change.
*
* Alternatively, publish to a local MQTT broker (e.g. mosquitto) running on
* the node that hosts your border router
*/
#ifdef MQTT_CONF_BROKER_IP_ADDR
static const char *broker_ip = MQTT_CONF_BROKER_IP_ADDR;
#define DEFAULT_ORG_ID              "mqtt-apc"
#else
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define DEFAULT_ORG_ID              "quickstart"
#endif
/*
* A timeout used when waiting for something to happen (e.g. to connect or to
* disconnect)
*/
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)
/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)
/*
* Number of times to try reconnecting to the broker.
* Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
*/
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
#define RSSI_MEASURE_INTERVAL_MAX 604800 /* secs: 7 days */
#define RSSI_MEASURE_INTERVAL_MIN     5  /* secs */
#define PUBLISH_INTERVAL_MAX      604800 /* secs: 7 days */
#define PUBLISH_INTERVAL_MIN         600 /* secs: 10 mins */
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_TYPE_ID             "cc2538"
#define DEFAULT_AUTH_TOKEN          "AUTHZ"
#define DEFAULT_EVENT_TYPE_ID       "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLISH_INTERVAL    (PUBLISH_INTERVAL_MIN * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    PUBLISH_INTERVAL_MIN + 300
#define DEFAULT_RSSI_MEAS_INTERVAL  (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN   20
/*---------------------------------------------------------------------------*/
/**
* \brief Data structure declaration for the MQTT client configuration
*/
typedef struct mqtt_client_config {
	char org_id[CONFIG_ORG_ID_LEN];
	char type_id[CONFIG_TYPE_ID_LEN];
	char auth_token[CONFIG_AUTH_TOKEN_LEN];
	char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
	char broker_ip[CONFIG_IP_ADDR_STR_LEN];
	char cmd_type[CONFIG_CMD_TYPE_LEN];
	clock_time_t pub_interval;
	int def_rt_ping_interval;
	uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
/*---------------------------------------------------------------------------*/
#ifndef MQTT_CONF_STATUS_LED
#define MQTT_CONF_STATUS_LED LEDS_WHITE
#else
#define STATUS_LED MQTT_CONF_STATUS_LED
#endif
/*---------------------------------------------------------------------------*/
/*
* Buffers for Client ID and Topic.
* Make sure they are large enough to hold the entire respective string
*
* d:quickstart:status:EUI64 is 32 bytes long
* We also need space for the null termination
*/
#define BUFFER_SIZE 64
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
* The main MQTT buffers.
* We will need to increase if we start publishing more data.
*/
#define APP_BUFFER_SIZE 600
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct_led;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/* MQTT-specific Configuration END (code copied from cc2538-common/mqtt-demo)*/
/*----------------------------------------------------------------------------------*/
PROCESS(apc_sensor_node_network_init_process, "APC Sensor Node (Network Initialization) Process Handler");
PROCESS(apc_sensor_node_collect_gather_process, "APC Sensor Node (Collector Gather) Process Handler");
PROCESS(apc_sensor_node_en_sensors_process, "APC Sensor Node (Sensor Initialization) Process Handler");
PROCESS(mqtt_handler_process, "MQTT Process Handler");
/*----------------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&apc_sensor_node_network_init_process, &apc_sensor_node_en_sensors_process, &mqtt_handler_process);
/*---------------------------------------------------------------------------*/
static int
activate_sensor
(uint8_t sensor_type){
	int retCode;
	switch(sensor_type){
	case HUMIDITY_T:
	case TEMPERATURE_T:
		retCode = SENSORS_ACTIVATE(dht22);
		return retCode != DHT22_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case PM25_T:
		retCode = pm25.configure(SENSORS_ACTIVE, PM25_ENABLE);
		return retCode != PM25_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case NO2_T:
	case CO_T:
		retCode = aqs_sensor.configure(AQS_ENABLE, MICS4514_SENSOR);
		return retCode != AQS_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case O3_T:
		retCode = aqs_sensor.configure(AQS_ENABLE, MQ131_SENSOR);
		return retCode != AQS_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case WIND_SPEED_T:
		retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_SPEED_SENSOR);
		return retCode != WIND_SENSOR_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case WIND_DRCTN_T:
		retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_DIR_SENSOR);
		return retCode != WIND_SENSOR_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	default:
		PRINTF("activate_sensor: Unknown sensor type given.\n");
		return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static uint8_t
is_calibrated_sensor
(uint8_t sensor_type){
	for (int i = 0; i < SENSOR_COUNT; i++){
		if ( sensor_infos[i].sensor_type == sensor_type ){
			return sensor_infos[i].is_calibrated;
		}
	}
	PRINTF("is_calibrated_sensor: ERROR - invalid sensor type specified.\n");
	return APC_SENSOR_OPFAILURE;
}
/*----------------------------------------------------------------------------------*/
static int
get_index_from_sensor_type
(uint8_t sensor_type){
	for(int i = 0; i < SENSOR_COUNT; i++){
		if (sensor_infos[i].sensor_type == sensor_type)
			return i;
	}
	PRINTF("get_index_from_sensor_type: ERROR - invalid sensor type specified.\n");
	return APC_SENSOR_OPFAILURE;
}
/*----------------------------------------------------------------------------------*/
static int
map_sensor_to_calib_type
(uint8_t sensor_type){
	switch(sensor_type){
		case CO_T:
			return CO_RO_T;

			break;
		case O3_T:
			return O3_RO_T;

			break;
		case NO2_T:
			return NO2_RO_T;

			break;
		default:
			PRINTF("map_sensor_to_calib_type: ERROR - invalid sensor type specified.\n");
			return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static int
map_calib_to_sensor_type
(uint8_t calib_type){
	switch(calib_type){
		case CO_RO_T:
			return CO_T;

			break;
		case O3_RO_T:
			return O3_T;

			break;
		case NO2_RO_T:
			return NO2_T;

			break;
		default:
			PRINTF("map_calib_to_sensor_type: ERROR - invalid sensor type specified.\n");
			return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static int
read_calib_sensor
(uint8_t sensor_type){
	uint8_t index = get_index_from_sensor_type(sensor_type);
	int64_t value;

	if (index == APC_SENSOR_OPFAILURE){
		PRINTF("read_calib_sensor: ERROR - invalid sensor type specified. \n");
		return APC_SENSOR_OPFAILURE;
	}
	int calibType = map_sensor_to_calib_type(sensor_type);

	switch(calibType){
		case CO_RO_T:
			value = aqs_value(MICS4514_SENSOR_RED_RO);
			if (value == AQS_ERROR){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO_RO_T \n");
				PRINTF("Failed to read sensor.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else if (value == AQS_INITIALIZING){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO_RO_T \n");
				PRINTF("sensor is initializing.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else {
				sensor_infos[index].is_calibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%lld.%03lld",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO_RO_T \n");
				PRINTF("RO: %lld.%03lld\n",
					value / 1000,
					value % 1000);
				PRINTF("-----------------\n");
				return APC_SENSOR_OPSUCCESS;
			}

			break;
		case NO2_RO_T:
			value = aqs_value(MICS4514_SENSOR_NOX_RO);
			if (value == AQS_ERROR){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: NO2_RO_T \n");
				PRINTF("Failed to read sensor.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else if (value == AQS_INITIALIZING){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: NO2_RO_T \n");
				PRINTF("sensor is initializing.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else {
				sensor_infos[index].is_calibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%lld.%03lld",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: NO2_RO_T \n");
				PRINTF("RO: %lld.%03lld\n",
					value / 1000,
					value % 1000);
				PRINTF("-----------------\n");
				return APC_SENSOR_OPSUCCESS;
			}

			break;
		case O3_RO_T:
			value = aqs_value(MQ131_SENSOR_RO);
			if (value == AQS_ERROR){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: O3_RO_T \n");
				PRINTF("Failed to read sensor.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else if (value == AQS_INITIALIZING){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: O3_RO_T \n");
				PRINTF("sensor is initializing.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else {
				sensor_infos[index].is_calibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%lld.%03lld",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: O3_RO_T \n");
				PRINTF("RO: %lld.%03lld\n",
					value / 1000,
					value % 1000);
				PRINTF("-----------------\n");
				return APC_SENSOR_OPSUCCESS;
			}

			break;
		default:
			PRINTF("read_calib_sensor: ERROR - invalid sensor type specified.\n");
			return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static int 
read_sensor
(uint8_t sensor_type) {
	static uint8_t index = -1;
	int value;

	for (int i = 0; i < SENSOR_COUNT; i++){
		if ( sensor_infos[i].sensor_type == sensor_type ){
			index = i;
			break;
		}
	}

	if (index == -1){
		PRINTF("read_sensor: ERROR - invalid sensor type specified. \n");
		return APC_SENSOR_OPFAILURE;
	}


	switch(sensor_type){
		//HUMIDITY_T and TEMPERATURE_T are read at the same sensor
	case HUMIDITY_T:
		//Assume it is busy
		do{
			value = dht22.value(DHT22_READ_HUM);
			if (value == DHT22_BUSY)
				PRINTF("Sensor is busy, retrying...\n");
		} while (value == DHT22_BUSY);
		if (value != DHT22_ERROR) {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%d",
			value / 10, value % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: HUMIDITY_T \n");
			PRINTF("Humidity %d.%d RH\n", value / 10, value % 10);
			PRINTF("-----------------\n");
			// reflect values in aqs sensor
			aqs_humidity =  value / 10;

			return APC_SENSOR_OPSUCCESS;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: HUMIDITY_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		break;
	case TEMPERATURE_T:
		//Assume it is busy
		do{
			value = dht22.value(DHT22_READ_TEMP);
			if (value == DHT22_BUSY)
				PRINTF("Sensor is busy, retrying...\n");
		} while (value == DHT22_BUSY);
		if (value != DHT22_ERROR) {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%d",
			value / 10, value % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T \n");
			PRINTF("Temperature %d.%d deg. C\n", value / 10, value % 10);
			PRINTF("-----------------\n");
			// reflect values in aqs sensor
			aqs_temperature =  value;

			return APC_SENSOR_OPSUCCESS;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		break;
	case PM25_T:
		//pm sensor only measures one value, parameter does not do anything here
		value = pm25.value(0);
		if (value == PM25_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			sprintf(sensor_infos[index].sensor_reading,
			"%d", 
			value
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Dust Density: %02d ug/m3\n", value);
			PRINTF("-----------------\n");
		return APC_SENSOR_OPSUCCESS;
		}
		break;
	case CO_T:
		value = aqs_value(MICS4514_SENSOR_RED);
		if (value == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else if (value == AQS_INITIALIZING){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("sensor is initializing.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%03d", 
			value / 1000,
			value % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Rs/Ro: %d.%03d\n",
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case NO2_T:
		value = aqs_value(MICS4514_SENSOR_NOX);
		if (value == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: NO2_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else if (value == AQS_INITIALIZING){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: NO2_T \n");
			PRINTF("sensor is initializing.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%03d", 
			value / 1000,
			value % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: NO2_T \n");
			PRINTF("Rs/Ro: %d.%03d\n",
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case O3_T:
		value = aqs_value(MQ131_SENSOR);
		if (value == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else if (value == AQS_INITIALIZING){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("sensor is initializing.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%03d", 
			value / 1000,
			value % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Rs/Ro: %d.%03d\n",
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case WIND_SPEED_T:
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		shared_sensor_select_pin(APC_SENSOR_NODE_AMUX_SELECT_ANEMOMETER);
#endif
		value = anem_sensor.value(WIND_SPEED_SENSOR);
		if (value == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			sprintf(sensor_infos[index].sensor_reading,
			"%d.%02d",
			value,
			value
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("WIND SPEED: %d.%02d m/s\n", 
				value / 100, //whole component
				(value % 100)//frac component
			);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case WIND_DRCTN_T:
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		shared_sensor_select_pin(APC_SENSOR_NODE_AMUX_SELECT_WIND_VANE);
#endif
		value = anem_sensor.value(WIND_DIR_SENSOR);
		if (value == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("WIND DRCTN (Raw): 0x%04x\n", value);
			switch (value){
				case WIND_DIR_NORTH:
					PRINTF("WIND DRCTN (Actual): NORTH\n");
					sprintf(sensor_infos[index].sensor_reading,
						"N");
					break;
				case WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): EAST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"E");
					break;
				case WIND_DIR_SOUTH:
					PRINTF("WIND DRCTN (Actual): SOUTH\n");
					sprintf(sensor_infos[index].sensor_reading,
						"S");
					break;
				case WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): WEST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"W");
					break;
				case WIND_DIR_NORTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): NORTHEAST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"NE");
					break;
				case WIND_DIR_NORTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): NORTHWEST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"NW");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): SOUTHEAST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"SE");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): SOUTHWEST\n");
					sprintf(sensor_infos[index].sensor_reading,
						"SW");
					break;
				default:
					PRINTF("ERROR: Unexpected value.");
					break;
			}
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	default:
		return APC_SENSOR_OPFAILURE;
	}
}
/*---------------------------------------------------------------------------*/
void
print_local_addresses(void* data)
{
	uint8_t i;
	uint8_t state;
	PRINTF("Mote IPv6 addresses: ");
	for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		
		if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
			
			PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
			PRINTF("\n");
			/* hack to make address "final" */
			if (state == ADDR_TENTATIVE) {
				uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
			}
		}
	}
}
/*----------------------------------------------------------------------------------*/
static void
print_local_dev_info()
{
	int temp = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
	
	printf("VDD3 = %d mV\n", vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED));
	printf("Internal Temperature = %d.%d deg. C\n", temp / 1000, temp % 1000);
}
/*----------------------------------------------------------------------------------*/
void
set_local_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* collectorAddr)
{
	if (prefix_64 != NULL && !uip_is_addr_unspecified(prefix_64)){
		memcpy(collectorAddr, prefix_64, 16); //copy 64-bit prefix_64
		uip_ds6_set_addr_iid(collectorAddr, &uip_lladdr);
		uip_ds6_addr_add(collectorAddr, 0, ADDR_AUTOCONF);
	} else{
		//no prefix available, assign a hardcoded address
		uiplib_ip6addrconv(APC_SENSOR_ADDRESS, collectorAddr);
		uip_ds6_set_addr_iid(collectorAddr, &uip_lladdr);
		uip_ds6_addr_add(collectorAddr, 0, ADDR_AUTOCONF);
	}
}
/*----------------------------------------------------------------------------------*/
void
set_remote_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* remote_addr)
{
	// do nothing
}
/*----------------------------------------------------------------------------------*/
void
update_ip_addresses_prefix(void* prefix_64)
{
	uip_ipaddr_t* pref;
	uip_ipaddr_t collectAddr;
	
	if (prefix_64 == NULL)
		return;
	pref = (uip_ipaddr_t*)prefix_64;
	//don't do anything if they are the same or unspecified
	if (!uip_ipaddr_cmp(pref, &prefix) && !uip_is_addr_unspecified(pref)) 
	{
		PRINTF("Prefix updated from (");
		PRINT6ADDR(&prefix);
		PRINTF(") to (");
		PRINT6ADDR(pref);
		PRINTF(")\n");
		memcpy(&prefix, pref, 16); //copy 64-bit prefix
		/* set address of this node */
		set_local_ip_addresses(&prefix, &collectAddr);
	}
}
/*----------------------------------------------------------------------------------*/
/* MQTT-specific definitions START (code copied from cc2538-common/mqtt-demo, with some modifications)*/
/*----------------------------------------------------------------------------------*/
int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
	uint16_t a;
	uint8_t len = 0;
	int i, f;
	for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
		a = (addr->u8[i] << 8) + addr->u8[i + 1];
		if(a == 0 && f >= 0) {
			if(f++ == 0) {
				len += snprintf(&buf[len], buf_len - len, "::");
			}
		} else {
			if(f > 0) {
				f = -1;
			} else if(i > 0) {
				len += snprintf(&buf[len], buf_len - len, ":");
			}
			len += snprintf(&buf[len], buf_len - len, "%x", a);
		}
	}
	return len;
}
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
uint16_t datalen)
{
	if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
		def_rt_rssi = sicslowpan_get_last_rssi();
	}
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
	leds_off(STATUS_LED);
}
/*---------------------------------------------------------------------------*/
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
uint16_t chunk_len)
{
	DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len,
	chunk_len);
	/* If we don't like the length, ignore */
	if(topic_len != 23 || chunk_len != 1) {
		printf("Incorrect topic or chunk len. Ignored\n");
		return;
	}
	/* If the format != json, ignore */
	if(strncmp(&topic[topic_len - 4], "json", 4) != 0) {
		printf("Incorrect format\n");
	}
	if(strncmp(&topic[10], "leds", 4) == 0) {
		if(chunk[0] == '1') {
			leds_on(LEDS_RED);
		} else if(chunk[0] == '0') {
			leds_off(LEDS_RED);
		}
		return;
	}
}
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
	switch(event) {
	case MQTT_EVENT_CONNECTED: {
			DBG("APP - Application has a MQTT connection\n");
			timer_set(&connection_life, CONNECTION_STABLE_TIME);
			state = STATE_CONNECTED;
			break;
		}
	case MQTT_EVENT_DISCONNECTED: {
			DBG("APP - MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));
			state = STATE_DISCONNECTED;
			process_poll(&mqtt_handler_process);
			break;
		}
	case MQTT_EVENT_PUBLISH: {
			msg_ptr = data;
			/* Implement first_flag in publish message? */
			if(msg_ptr->first_chunk) {
				msg_ptr->first_chunk = 0;
				DBG("APP - Application received a publish on topic '%s'. Payload "
				"size is %i bytes. Content:\n\n",
				msg_ptr->topic, msg_ptr->payload_length);
			}
			pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk,
			msg_ptr->payload_length);
			break;
		}
	case MQTT_EVENT_SUBACK: {
			DBG("APP - Application is subscribed to topic successfully\n");
			break;
		}
	case MQTT_EVENT_UNSUBACK: {
			DBG("APP - Application is unsubscribed to topic successfully\n");
			break;
		}
	case MQTT_EVENT_PUBACK: {
			DBG("APP - Publishing complete.\n");
			break;
		}
	default:
		DBG("APP - Application got a unhandled MQTT event: %i\n", event);
		break;
	}
}
/*---------------------------------------------------------------------------*/
static int
construct_pub_topic(void)
{
	int len = snprintf(pub_topic, BUFFER_SIZE, "%s/%s/evt/%s/fmt/json",
			APC_SENSOR_TOPIC_NAME,
			APC_SENSOR_MOTE_ID,
			conf.event_type_id);
	/* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
	if(len < 0 || len >= BUFFER_SIZE) {
		printf("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
		return 0;
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_sub_topic(void)
{
	int len = snprintf(sub_topic, BUFFER_SIZE, "apc-iot/cmd/%s/fmt/json",
	conf.cmd_type);
	/* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
	if(len < 0 || len >= BUFFER_SIZE) {
		printf("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
		return 0;
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_client_id(void)
{
	int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
	conf.org_id, conf.type_id,
	linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
	linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
	linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
	/* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
	if(len < 0 || len >= BUFFER_SIZE) {
		printf("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
		return 0;
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_config(void)
{
	if(construct_client_id() == 0) {
		/* Fatal error. Client ID larger than the buffer */
		state = STATE_CONFIG_ERROR;
		return;
	}
	if(construct_sub_topic() == 0) {
		/* Fatal error. Topic larger than the buffer */
		state = STATE_CONFIG_ERROR;
		return;
	}
	if(construct_pub_topic() == 0) {
		/* Fatal error. Topic larger than the buffer */
		state = STATE_CONFIG_ERROR;
		return;
	}
	/* Reset the counter */
	seq_nr_value = 0;
	state = STATE_INIT;
	/*
* Schedule next timer event ASAP
*
* If we entered an error state then we won't do anything when it fires.
*
* Since the error at this stage is a config error, we will only exit this
* error state if we get a new config.
*/
	etimer_set(&publish_periodic_timer, 0);
	return;
}
/*---------------------------------------------------------------------------*/
static int
init_config()
{
	/* Populate configuration with default values */
	memset(&conf, 0, sizeof(mqtt_client_config_t));
	memcpy(conf.org_id, DEFAULT_ORG_ID, strlen(DEFAULT_ORG_ID));
	memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
	memcpy(conf.auth_token, DEFAULT_AUTH_TOKEN, strlen(DEFAULT_AUTH_TOKEN));
	memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
	strlen(DEFAULT_EVENT_TYPE_ID));
	memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
	memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);
	conf.broker_port = DEFAULT_BROKER_PORT;
	conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
	conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;
	return 1;
}
/*---------------------------------------------------------------------------*/
static void
subscribe(void)
{
	/* Publish MQTT topic in IBM quickstart format */
	mqtt_status_t status;
	status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);
	DBG("APP - Subscribing!\n");
	if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
		DBG("APP - Tried to subscribe but command queue was full!\n");
	}
}
/*---------------------------------------------------------------------------*/
static void
pub_sensor_data
(int len, int remaining)
{
	uint8_t index = 0;

	//actual sensor values
	len = snprintf(buf_ptr, remaining, "\"%s\":%s",
		SENSOR_TYPE_HEADERS[index],
		strcmp(sensor_infos[index].sensor_reading,"") ? sensor_infos[index].sensor_reading : "-1");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	index++;
	for (; index < SENSOR_COUNT; index++){
		if (sensor_infos[index].sensor_type == WIND_DRCTN_T) {
			len = snprintf(buf_ptr, remaining, ",\"%s\":\"%s\"",
			SENSOR_TYPE_HEADERS[index],
			strcmp(sensor_infos[index].sensor_reading,"") ? sensor_infos[index].sensor_reading : "-1");
		}
		else {
			len = snprintf(buf_ptr, remaining, ",\"%s\":%s",
			SENSOR_TYPE_HEADERS[index],
			strcmp(sensor_infos[index].sensor_reading,"") ? sensor_infos[index].sensor_reading : "-1");
		}
		remaining -= len;
		buf_ptr += len;
		if(len < 0 || len >= remaining) {
			printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
			return;
		}
	}

	//calibration values
	//enclose the calibration values
	len = snprintf(buf_ptr, remaining, ",\"calibration\":[");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	int sensor_type, calib_index;
	index = 0;

	sensor_type = map_calib_to_sensor_type(SENSOR_CALIB_TYPES[index]);
	if (sensor_type != APC_SENSOR_OPFAILURE) {
		calib_index = get_index_from_sensor_type(sensor_type);
		if (calib_index != APC_SENSOR_OPFAILURE){
			len = snprintf(buf_ptr, remaining, "{\"%s\":%s}",
				SENSOR_CALIB_TYPE_HEADERS[index],
				strcmp(sensor_infos[calib_index].sensor_calib_reading,"") ? sensor_infos[calib_index].sensor_calib_reading : "-1");
			remaining -= len;
			buf_ptr += len;
			index++;
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
		}
	}

	for (; index < SENSOR_CALIB_COUNT; index++){
		sensor_type = map_calib_to_sensor_type(SENSOR_CALIB_TYPES[index]);
		if (sensor_type == APC_SENSOR_OPFAILURE)
			continue;
		calib_index = get_index_from_sensor_type(sensor_type);
		if (calib_index == APC_SENSOR_OPFAILURE)
			continue;

		len = snprintf(buf_ptr, remaining, ",{\"%s\":%s}",
				SENSOR_CALIB_TYPE_HEADERS[index],
				strcmp(sensor_infos[calib_index].sensor_calib_reading,"") ? sensor_infos[calib_index].sensor_calib_reading : "-1");

		if(len < 0 || len >= remaining) {
			printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
			return;
		}
		remaining -= len;
		buf_ptr += len;
	}

	//enclose the calibration values list
	len = snprintf(buf_ptr, remaining, "]");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
}
/*---------------------------------------------------------------------------*/
static void
publish(void)
{
	/* Publish MQTT topic in IBM quickstart format */
	int len;
	int remaining = APP_BUFFER_SIZE;
	seq_nr_value++;
	buf_ptr = app_buffer;
	len = snprintf(buf_ptr, remaining,
	"{"
	"\"collector_info\":{"
	"\"myName\":\"%s\","
	"\"Seq #\":%d,"
	"\"Uptime (sec)\":%lu",
	BOARD_STRING, seq_nr_value, clock_seconds());
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	/* Put our Default route's string representation in a buffer */
	char def_rt_str[64];
	memset(def_rt_str, 0, sizeof(def_rt_str));
	ipaddr_sprintf(def_rt_str, sizeof(def_rt_str), uip_ds6_defrt_choose());
	len = snprintf(buf_ptr, remaining, ",\"Def Route\":\"%s\",\"RSSI (dBm)\":%d",
	def_rt_str, def_rt_rssi);
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	/* Put our preferred address string representation in a buffer */
	char pref_addr_str[64];
	memset(pref_addr_str, 0, sizeof(pref_addr_str));
	ipaddr_sprintf(pref_addr_str, sizeof(pref_addr_str), &collect_addr);
	len = snprintf(buf_ptr, remaining, ",\"Preferred Address\":\"%s\"",
			pref_addr_str);
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	len = snprintf(buf_ptr, remaining, ",\"On-Chip Temp (mC)\":%d",
	cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED));
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	len = snprintf(buf_ptr, remaining, ",\"VDD3 (mV)\":%d",
	vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED));
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;
	len = snprintf(buf_ptr, remaining, "}");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;

	len = snprintf(buf_ptr, remaining, ", \"collector_sensor_data\":{");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;

	pub_sensor_data(len, remaining);

	len = snprintf(buf_ptr, remaining, "}}");
	if(len < 0 || len >= remaining) {
		printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
		return;
	}
	remaining -= len;
	buf_ptr += len;

	mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
	strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
	DBG("APP - Publish!\n");
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
	/* Connect to MQTT server */
	mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
	conf.pub_interval * 3);
	state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
static void
ping_parent(void)
{
	if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) {
		return;
	}
	uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
	ECHO_REQ_PAYLOAD_LEN);
}
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
	switch(state) {
	case STATE_INIT:
		/* If we have just been configured register MQTT connection */
		mqtt_register(&conn, &mqtt_handler_process, client_id, mqtt_event,
		MAX_TCP_SEGMENT_SIZE);
		/*
	* If we are not using the quickstart service (thus we are an IBM
	* registered device), we need to provide user name and password
	*/
		if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) != 0) {
			if(strlen(conf.auth_token) == 0) {
				printf("User name set, but empty auth token\n");
				state = STATE_ERROR;
				break;
			} else {
				mqtt_set_username_password(&conn, "use-token-auth",
				conf.auth_token);
			}
		}
		/* _register() will set auto_reconnect. We don't want that. */
		conn.auto_reconnect = 0;
		connect_attempt = 1;
		state = STATE_REGISTERED;
		DBG("Init\n");
		/* Continue */
	case STATE_REGISTERED:
		if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
			/* Registered and with a public IP. Connect */
			DBG("Registered. Connect attempt %u\n", connect_attempt);
			ping_parent();
			connect_to_broker();
		} else {
			leds_on(STATUS_LED);
			ctimer_set(&ct_led, NO_NET_LED_DURATION, publish_led_off, NULL);
		}
		etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
		return;
		break;
	case STATE_CONNECTING:
		leds_on(STATUS_LED);
		ctimer_set(&ct_led, CONNECTING_LED_DURATION, publish_led_off, NULL);
		/* Not connected yet. Wait */
		DBG("Connecting (%u)\n", connect_attempt);
		break;
	case STATE_CONNECTED:
		/* Don't subscribe unless we are a registered device */
		if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) == 0) {
			DBG("Using 'quickstart': Skipping subscribe\n");
			state = STATE_PUBLISHING;
		}
		/* Continue */
	case STATE_PUBLISHING:
		/* If the timer expired, the connection is stable. */
		if(timer_expired(&connection_life)) {
			/*
	* Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
	* attempts if we disconnect after a successful connect
	*/
			connect_attempt = 0;
		}
		if(mqtt_ready(&conn) && conn.out_buffer_sent) {
			/* Connected. Publish */
			if(state == STATE_CONNECTED) {
				subscribe();
				state = STATE_PUBLISHING;
			} else {
				leds_on(STATUS_LED);
				ctimer_set(&ct_led, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
				publish();
			}
			etimer_set(&publish_periodic_timer, conf.pub_interval);
			DBG("Publishing\n");
			/* Return here so we don't end up rescheduling the timer */
			return;
		} else {
			/*
	* Our publish timer fired, but some MQTT packet is already in flight
	* (either not sent at all, or sent but not fully ACKd).
	*
	* This can mean that we have lost connectivity to our broker or that
	* simply there is some network delay. In both cases, we refuse to
	* trigger a new message and we wait for TCP to either ACK the entire
	* packet after retries, or to timeout and notify us.
	*/
			DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
			conn.out_queue_full);
		}
		break;
	case STATE_DISCONNECTED:
		DBG("Disconnected\n");
		if(connect_attempt < RECONNECT_ATTEMPTS ||
				RECONNECT_ATTEMPTS == RETRY_FOREVER) {
			/* Disconnect and backoff */
			clock_time_t interval;
			mqtt_disconnect(&conn);
			connect_attempt++;
			interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
			RECONNECT_INTERVAL << 3;
			DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);
			etimer_set(&publish_periodic_timer, interval);
			state = STATE_REGISTERED;
			return;
		} else {
			/* Max reconnect attempts reached. Enter error state */
			state = STATE_ERROR;
			DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
		}
		break;
	case STATE_CONFIG_ERROR:
		/* Idle away. The only way out is a new config */
		printf("Bad configuration.\n");
		return;
	case STATE_ERROR:
	default:
		leds_on(STATUS_LED);
		/*
	* 'default' should never happen.
	*
	* If we enter here it's because of some error. Stop timers. The only thing
	* that can bring us out is a new config event
	*/
		printf("Default case: State=0x%02x\n", state);
		return;
	}
	/* If we didn't return so far, reschedule ourselves */
	etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*----------------------------------------------------------------------------------*/
/* MQTT-specific definitions END (code copied from cc2538-common/mqtt-demo, with some modifications)*/
/*----------------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
	mote_message_t *appdata;

	if(uip_newdata()) {
		leds_on(STATUS_LED);
		ctimer_set(&ct_led, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);

		appdata = (mote_message_t *)uip_appdata;
		if (appdata->type == SINK_CMD) {
			PRINTF("Command received from sink [");
			PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
			PRINTF("]: %s\n", appdata->data);

			// reset timers
			/* TODO: timer reset hangs the collect timer and mqtt publish functionality, fix this
			 * etimer_reset(&et_collect);
			PRINTF("Collect timer reset!\n");
			etimer_reset(&publish_periodic_timer);
			PRINTF("MQTT publish timer reset!\n");*/
		}
		else {
			PRINTF("Unknown message type received from [");
			PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
			PRINTF("]\n");
		}
	}
}
/*----------------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sensor_node_network_init_process, ev, data)
{
	//initialization
	static struct ctimer ct_net_print;
	static struct ctimer ct_update_addr;
	static uip_ipaddr_t curPrefix;
	rpl_dag_t* dag;
	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Network Initialization) begins...\n");
	seqNo = 1;
	uip_create_unspecified(&prefix);
	leds_off(LEDS_ALL);
	leds_on(LEDS_GREEN);
	
	PRINTF("Mote ID configured as %s\n", APC_SENSOR_MOTE_ID);

	/* get prefix from DAG */
	dag = rpl_get_any_dag();
	if (dag != NULL && !uip_is_addr_unspecified(&dag->prefix_info.prefix) ){
		uip_ip6addr_copy(&prefix, &dag->prefix_info.prefix);
		PRINTF("DAG Prefix: ");
		PRINT6ADDR(&prefix);
		PRINTF("\n");
	}
	else{
		PRINTF("DAG Prefix not set.\n");
	}
	
	/* set address of this node */
	set_local_ip_addresses(&prefix, &collect_addr);

	sink_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL);
	if(sink_conn == NULL) {
		PRINTF("No UDP connection available, this node won't be able to receive sink commands!\n");
	} else {
		udp_bind(sink_conn, UIP_HTONS(UDP_SERVER_PORT));

		PRINTF("Created a server connection with remote address ");
		PRINT6ADDR(&sink_conn->ripaddr);
		PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sink_conn->lport),
			 UIP_HTONS(sink_conn->rport));
	}

	print_local_addresses(NULL);
	ctimer_set(&ct_net_print, CLOCK_SECOND * LOCAL_ADDR_PRINT_INTERVAL, print_local_addresses, NULL);
	ctimer_set(&ct_update_addr, CLOCK_SECOND * PREFIX_UPDATE_INTERVAL, update_ip_addresses_prefix, &curPrefix);

	while(1) {
		PROCESS_YIELD();
		if(ev == tcpip_event)
		   tcpip_handler();
		if(ctimer_expired(&ct_net_print))
			ctimer_reset(&ct_net_print);
		if(ctimer_expired(&ct_update_addr)) {
			/* get prefix from DAG */
			dag = rpl_get_any_dag();
			if (dag != NULL && !uip_is_addr_unspecified(&dag->prefix_info.prefix) ){
				uip_ip6addr_copy(&curPrefix, &dag->prefix_info.prefix);
				PRINTF("DAG Prefix: ");
				PRINT6ADDR(&curPrefix);
				PRINTF("\n");
			}
			else{
				PRINTF("DAG missing or prefix not set.\n");
			}
			ctimer_reset(&ct_update_addr);
		}
	}
	
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sensor_node_collect_gather_process, ev, data)
{
	//initialization
	static struct etimer et_read_wait;
	static uint8_t index = 0;
	static uint8_t read_calib_sensors_count = 0;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Collector Gather) begins...\n");

	etimer_set(&et_collect, CLOCK_SECOND * APC_SENSOR_NODE_READ_INTERVAL_SECONDS);
	while (1)
	{
		PROCESS_YIELD();
		if (ev == PROCESS_EVENT_TIMER && data == &et_collect) {
			PRINTF("apc_sensor_node_collect_gather_process: starting collection\n");
			// collect general sensor data
			for (index = 0; index < SENSOR_COUNT; index++){
				read_sensor(sensor_infos[index].sensor_type);

				etimer_set(&et_read_wait, APC_SENSOR_NODE_READ_WAIT_MILLIS);
				PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et_read_wait) );

				leds_on(LEDS_YELLOW);
				ctimer_set(&ct_led, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
			}

			// collect sensor calibration data
			if (read_calib_sensors_count != SENSOR_CALIB_COUNT){
				PRINTF(apc_sensor_node_collect_gather_process.name);
				PRINTF(": reading calibration data.\n");
				for (index = 0; index < SENSOR_CALIB_COUNT; index++){
					int sensor_type = map_calib_to_sensor_type(SENSOR_CALIB_TYPES[index]);
					PRINTF(apc_sensor_node_collect_gather_process.name);
					PRINTF(": Sensor type (for -calibration): 0x%02x\n", sensor_type);
					if ( !is_calibrated_sensor(sensor_type) ) {

						if (read_calib_sensor(sensor_type) == APC_SENSOR_OPSUCCESS)
							read_calib_sensors_count++;
					}
					leds_on(LEDS_YELLOW);
					ctimer_set(&ct_led, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
				}
			}
			PRINTF("apc_sensor_node_collect_gather_process: collection finished\n");
			etimer_reset(&et_collect);
		}
	}
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sensor_node_en_sensors_process, ev, data)
{
	//initialization
	uint8_t i;
	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Sensor Initialization) begins...\n");
	leds_on(LEDS_YELLOW);
	//initialize sensor types and configure
	for (i = 0; i < SENSOR_COUNT; i++) {
		sensor_infos[i].sensor_type = SENSOR_TYPES[i];
		sensor_infos[i].is_calibrated = 0;
		PRINTF("apc_sensor_node_en_sensors_process: Sensor(0x%01x): %s\n", sensor_infos[i].sensor_type,
		activate_sensor(sensor_infos[i].sensor_type) == APC_SENSOR_OPFAILURE ? "ERROR\0" : "OK\0" );
	}
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	//configure the analog multiplexer
	uint8_t select_lines_count = SHARED_SENSOR_MAX_SELECT_LINES;
	PRINTF("Max select lines configured to be %u\n", SHARED_SENSOR_MAX_SELECT_LINES);
	shared_sensor_init(select_lines_count);
	shared_sensor_configure_select_pin(0, APC_SENSOR_NODE_AMUX_0BIT_PIN, APC_SENSOR_NODE_AMUX_0BIT_PORT);

	//share the anemometer and wind vane sensors
	shared_sensor_share_pin(&anem_sensor, APC_SENSOR_NODE_AMUX_SELECT_WIND_VANE);
	shared_sensor_share_pin(&anem_sensor, APC_SENSOR_NODE_AMUX_SELECT_ANEMOMETER);
#endif
	print_local_dev_info();
	leds_off(LEDS_YELLOW);
	process_start(&apc_sensor_node_collect_gather_process, NULL);
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_handler_process, ev, data)
{
	PROCESS_BEGIN();
	DBG(mqtt_handler_process.name);
	DBG(" begins...\n");
	if(init_config() != 1) {
		printf(mqtt_handler_process.name);
		printf(": FATAL ERROR - failed to initialize config.\n");
		PROCESS_EXIT();
	}
	update_config();
	def_rt_rssi = 0x8000000;
	uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
	echo_reply_handler);
	etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
	/* Main loop */
	while(1) {
		PROCESS_YIELD();
		if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
				ev == PROCESS_EVENT_POLL ) {
			state_machine();
		}
		if(ev == PROCESS_EVENT_TIMER && data == &echo_request_timer) {
			ping_parent();
			etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
		}
	}
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
