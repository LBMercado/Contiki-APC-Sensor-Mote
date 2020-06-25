/* C std libraries */
#include <stdio.h>
#include <string.h>
/* Contiki Core and Networking Libraries */
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "lib/sensors.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "net/rpl/rpl.h"
#include "rpl/rpl-private.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
/* Utility Libraries */
#include "lib/list.h"
#include "lib/memb.h"
/* Contiki Dev and Utilities */
#include "dev/leds.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/cc2538-sensors.h"
/* Contiki Apps */
#include "mqtt.h"
/* Project Sourcefiles */
#include "mqtt-sink-handler.h"
/*---------------------------------------------------------------------------*/
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
/*---------------------------------------------------------------------------*/
/* (code copied from cc2538-common/mqtt-demo)
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
/*---------------------------------------------------------------------------*/
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
#define RSSI_MEASURE_INTERVAL_MAX 86400 /* secs: 1 day */
#define RSSI_MEASURE_INTERVAL_MIN     5 /* secs */
#define PUBLISH_INTERVAL_MAX      86400 /* secs: 1 day */
#define PUBLISH_INTERVAL_MIN        600 /* secs */
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
#define DEFAULT_PUBLISH_INTERVAL    (600 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    900
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
* apc-iot/evt/status/fmt/json is 25 bytes
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
#define APP_BUFFER_SIZE 1024
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
/* This MEMB() definition defines a memory pool from which we allocate
   sensor node entries. */
MEMB(mqtt_node_memb, sensor_node_t, MAX_SENSOR_NODES);
/* The sensor_nodes_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(mqtt_node_list);
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_handler_process, "MQTT Process Handler");
/*---------------------------------------------------------------------------*/
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
	int len = snprintf(pub_topic, BUFFER_SIZE, "apc-iot/evt/%s/fmt/json",
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
update_mqtt_sensor_data
(sensor_reading_t* newData, sensor_node_t* node)
{
	if (node == NULL || newData == NULL){
		//invalid parameter
		PRINTF("MQTT - ERROR: invalid node parameter.\n");
		return;
	}
	//update the sensor values with the given type
	switch (newData->type){
	case TEMPERATURE_T:
		strcpy(node->temperature, newData->data);
		
		PRINTF("MQTT - TEMPERATURE_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->temperature);
		break;
	case HUMIDITY_T:
		strcpy(node->humidity, newData->data);
		
		PRINTF("MQTT - HUMIDITY_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->humidity);
		break;
	case PM25_T:
		strcpy(node->PM25, newData->data);
		
		PRINTF("MQTT - PM25_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->PM25);
		break;
	case CO_T:
		strcpy(node->CO, newData->data);
		
		PRINTF("MQTT - CO_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->CO);
		break;
	case NO2_T:
		strcpy(node->NO2, newData->data);
		
		PRINTF("MQTT - NO2_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->NO2);
		break;
	case O3_T:
		strcpy(node->O3, newData->data);
		
		PRINTF("MQTT - O3_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->O3);
		break;
	case WIND_SPEED_T:
		strcpy(node->windSpeed, newData->data);
		
		PRINTF("MQTT - WIND_SPEED_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->windSpeed);
		break;
	case WIND_DRCTN_T:
		strcpy(node->windDir, newData->data);
		
		PRINTF("MQTT - WIND_DRCTN_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->windDir);
		break;
	case CO_RO_T:
		strcpy(node->CO_RO, newData->data);

		PRINTF("MQTT - CO_RO_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->CO_RO);
		break;
	case NO2_RO_T:
		strcpy(node->NO2_RO, newData->data);

		PRINTF("MQTT - NO2_RO_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->NO2_RO);
		break;
	case O3_RO_T:
		strcpy(node->O3_RO, newData->data);

		PRINTF("MQTT - O3_RO_T of sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") UPDATED with value: %s\n", node->O3_RO);
		break;
	default:
		PRINTF("MQTT - Failed to update sensor node (");
		PRINT6ADDR(&node->addr);
		PRINTF(") - unexpected type of sensor.\n");
		return;
		//unexpected, do nothing
	}
}
/*---------------------------------------------------------------------------*/
int
mqtt_store_sensor_data
(uip_ipaddr_t* nodeAddr, sensor_reading_t* data)
{
	sensor_node_t *n;
	/* Check if we already know this sensor node. */
	for(n = list_head(mqtt_node_list); n != NULL; n = list_item_next(n)) {
		if(uip_ipaddr_cmp(&n->addr, nodeAddr)) {
			//update the corresponding node with new value
			update_mqtt_sensor_data(data, n);
			return MQTT_OPSUCCESS;
		}
	}
	/* If n is NULL, this node was not found in our list, and we
	allocate a new struct node from the memory pool. */
	if(n == NULL) {
		n = memb_alloc(&mqtt_node_memb);
		/* If we could not allocate a new neighbor entry, we give up. We
		could have reused an old neighbor entry, but we do not do this
		for now. */
		if(n == NULL) {
			PRINTF("MQTT: Failed to allocate sensor node entry.\n");
			return MQTT_OPFAILURE;
		}
		/* Initialize the fields. */
		uip_ipaddr_copy(&n->addr, nodeAddr);
		/* Place the neighbor on the neighbor list. */
		list_add(mqtt_node_list, n);
		/* Print out a message. */
		PRINTF("MQTT: new sensor node added with address ");
		PRINT6ADDR(nodeAddr);
		PRINTF("\n");
		return MQTT_OPSUCCESS;
	}
	//not supposed to reach here
	return MQTT_OPFAILURE;
}
/*---------------------------------------------------------------------------*/
static void
pub_sensor_data
(int len, int remaining)
{
	struct sensor_node *n;
	if(list_length(mqtt_node_list) > 0) {
		int i;
		n = list_head(mqtt_node_list);
		for(i = 0; i < list_length(mqtt_node_list); i++) {
			if (n == NULL)
				continue;
			//Enclose and specify which node
			char node_addr[64];
			ipaddr_sprintf(node_addr, sizeof(node_addr), &n->addr);
			len = snprintf(buf_ptr, remaining, ",\"collector%d\":{\"address\":\"%s\"",
			i + 1, node_addr);
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//Temperature
			len = snprintf(buf_ptr, remaining, ",\"Temperature (°C)\":%s",
			strcmp(n->temperature,"") ? n->temperature : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//Humidity
			len = snprintf(buf_ptr, remaining, ",\"Humidity (%%RH)\":%s",
			strcmp(n->humidity,"") ? n->humidity : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//PM25
			len = snprintf(buf_ptr, remaining, ",\"PM25 (ug/m3)\":%s",
			strcmp(n->PM25,"") ? n->PM25 : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//NO2
			len = snprintf(buf_ptr, remaining, ",\"NO2 (Rs/Ro)\":%s",
			strcmp(n->NO2,"") ? n->NO2 : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//CO
			len = snprintf(buf_ptr, remaining, ",\"CO (Rs/Ro)\":%s",
			strcmp(n->CO,"") ? n->CO : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//O3
			len = snprintf(buf_ptr, remaining, ",\"O3 (Rs/Ro)\":%s",
			strcmp(n->O3,"") ? n->O3 : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//Wind Speed
			len = snprintf(buf_ptr, remaining, ",\"Wind Speed (m/s)\":%s",
			strcmp(n->windSpeed,"") ? n->windSpeed : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//Wind Direction
			len = snprintf(buf_ptr, remaining, ",\"Wind Direction\":\"%s\"",
			n->windDir);
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;
			//Initial calibration values list
			len = snprintf(buf_ptr, remaining, ",\"calibration\":[");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			//CO calibration value
			len = snprintf(buf_ptr, remaining, "{\"CO Rs (Ohms)\":%s},",
					strcmp(n->CO_RO, "") ? n->CO_RO : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			//NO2 calibration value
			len = snprintf(buf_ptr, remaining, "{\"NO2 Rs (Ohms)\":%s},",
					strcmp(n->NO2_RO, "") ? n->NO2_RO : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			//O3 calibration value
			len = snprintf(buf_ptr, remaining, "{\"O3 Rs (Ohms)\":%s}",
					strcmp(n->O3_RO, "") ? n->O3_RO : "-1");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			//enclose the calibration values list
			len = snprintf(buf_ptr, remaining, "]");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			//enclose the node
			len = snprintf(buf_ptr, remaining, "}");
			if(len < 0 || len >= remaining) {
				printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
				return;
			}
			remaining -= len;
			buf_ptr += len;

			n = list_item_next(n);
		}
	}
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
	"\"sink\":{"
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
	
	pub_sensor_data(len, remaining);
	
	len = snprintf(buf_ptr, remaining, "}");
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
			ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
		}
		etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
		return;
		break;
	case STATE_CONNECTING:
		leds_on(STATUS_LED);
		ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
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
				ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
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
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_handler_process, ev, data)
{
	PROCESS_BEGIN();
	DBG("MQTT Handler Process begins...\n");
	if(init_config() != 1) {
		printf("FATAL ERROR@mqtt_handler_process: failed to initialize config.\n");
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
/*---------------------------------------------------------------------------*/
