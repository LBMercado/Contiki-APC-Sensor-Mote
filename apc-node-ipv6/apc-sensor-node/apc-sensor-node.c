/* C std libraries */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
/* Contiki Core and Networking Libraries */
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
/* Contiki Dev and Utilities */
#include "dev/dht22.h"
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#include "dev/pm25-sensor.h"
#include "dev/leds.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/timer.h"
/* Project Sourcefiles */
#include "apc-sensor-node.h"
#include "dev/air-quality-sensor.h"
#include "dev/anemometer-sensor.h"
#include "dev/shared-sensors.h"
/*---------------------------------------------------------------------------*/
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
/*---------------------------------------------------------------------------*/
#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_CONF_IPV6_RPL
#error "APC-Sink-Node will be unable to function properly with this current contiki configuration."
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif
/*----------------------------------------------------------------------------------*/
const uint8_t SENSOR_TYPES[SENSOR_COUNT] = {
	TEMPERATURE_T, //unit in Deg. Celsius
	HUMIDITY_T, //unit in %RH
	PM25_T, //unit in microgram per m3
	CO_T, //unit in ppm
	CO2_T, //unit in ppm
	O3_T, //unit in ppm
	WIND_SPEED_T, //unit in m/s
	WIND_DRCTN_T //may be N, S, E, W and their combinations
};
/*----------------------------------------------------------------------------------*/
const uint8_t SENSOR_CALIB_TYPES[SENSOR_CALIB_COUNT] =
{
	CO_RO_T, //unit in ohms, sf. by 3 digits
	CO2_RO_T, //unit in ohms, sf. by 3 digits
	O3_RO_T //unit in ohms, sf. by 3 digits
};
/*----------------------------------------------------------------------------------*/
/* Collect data from sensors at spaced intervals*/
#ifndef APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS             180
#else
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS             APC_SENSOR_NODE_READ_INTERVAL_SECONDS_CONF
#endif

/* Minimum for dht22 read intervals, consider slowest sensor */
#define APC_SENSOR_NODE_READ_WAIT_MILLIS                  CLOCK_SECOND >> 2

/* Amount of time to wait before collector pings sink */
#ifndef APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS_CONF
#define APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS    45
#else
#define APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS    APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS_CONF
#endif

/* Amount of time to wait before sink is considered unreachable or unreliable */
#ifndef APC_SENSOR_NODE_SINK_DEADTIME_SECONDS_CONF
#define APC_SENSOR_NODE_SINK_DEADTIME_SECONDS             60
#else
#if APC_SENSOR_NODE_SINK_DEADTIME_SECONDS_CONF < APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS
#error "Collector dead timer cannot be less than the advertisement time."
#else
#define APC_SENSOR_NODE_SINK_DEADTIME_SECONDS             APC_SENSOR_NODE_SINK_DEADTIME_SECONDS_CONF
#endif
#endif

#ifndef APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF
#define APC_SENSOR_NODE_AMUX_0BIT_PORT                    GPIO_D_NUM
#else
#define APC_SENSOR_NODE_AMUX_0BIT_PORT                    APC_SENSOR_NODE_AMUX_0BIT_PORT_CONF
#endif

#ifndef APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF
#define APC_SENSOR_NODE_AMUX_0BIT_PIN                     1
#else
#define APC_SENSOR_NODE_AMUX_0BIT_PIN                     APC_SENSOR_NODE_AMUX_0BIT_PIN_CONF
#endif

#define UIP_IP_BUF                                        ( (struct uip_ip_hdr*) & uip_buf[UIP_LLH_LEN] )
/*----------------------------------------------------------------------------------*/
typedef struct{
	uint8_t sensorType;
	uint8_t isCalibrated; //used only during calibration procedures
	char sensor_reading[8];
	char sensor_calib_reading[8]; //used only for read_calib_sensor
} sensor_info_t;
/*----------------------------------------------------------------------------------*/
static sensor_info_t sensor_infos[SENSOR_COUNT];
/*----------------------------------------------------------------------------------*/
static struct timer sink_dead_timer;
/*----------------------------------------------------------------------------------*/
static struct uip_udp_conn* collect_conn;
static uip_ipaddr_t collect_addr;
static uip_ipaddr_t sink_addr;
static uip_ipaddr_t sink_addr_pref;
static uip_ipaddr_t prefix;
/*----------------------------------------------------------------------------------*/
static uint32_t seqNo;
/*----------------------------------------------------------------------------------*/
PROCESS(apc_sensor_node_network_init_process, "APC Sensor Node (Network Initialization) Process Handler");
PROCESS(apc_sensor_node_collect_adv_process, "APC Sensor Node (Collector Advertise) Process Handler");
PROCESS(apc_sensor_node_collect_gather_process, "APC Sensor Node (Collector Gather) Process Handler");
PROCESS(apc_sensor_node_en_sensors_process, "APC Sensor Node (Sensor Initialization) Process Handler");
/*----------------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&apc_sensor_node_network_init_process, &apc_sensor_node_en_sensors_process);
/*----------------------------------------------------------------------------------*/
static int
activate_sensor
(uint8_t sensorType){
	int retCode;
	switch(sensorType){
	case HUMIDITY_T:
	case TEMPERATURE_T:
		retCode = SENSORS_ACTIVATE(dht22);
		return retCode != DHT22_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case PM25_T:
		retCode = pm25.configure(SENSORS_ACTIVE, PM25_ENABLE);
		return retCode != PM25_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case CO_T:
		retCode = aqs_sensor.configure(AQS_ENABLE, MQ7_SENSOR);
		return retCode != AQS_ERROR ? APC_SENSOR_OPSUCCESS : APC_SENSOR_OPFAILURE;
		break;
	case CO2_T:
		retCode = aqs_sensor.configure(AQS_ENABLE, MQ135_SENSOR);
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
(uint8_t sensorType){
	for (int i = 0; i < SENSOR_COUNT; i++){
		if ( sensor_infos[i].sensorType == sensorType ){
			return sensor_infos[i].isCalibrated;
		}
	}
	PRINTF("is_calibrated_sensor: ERROR - invalid sensor type specified.\n");
	return APC_SENSOR_OPFAILURE;
}
/*----------------------------------------------------------------------------------*/
static int
get_index_from_sensor_type
(uint8_t sensorType){
	for(int i = 0; i < SENSOR_COUNT; i++){
		if (sensor_infos[i].sensorType == sensorType)
			return i;
	}
	PRINTF("get_index_from_sensor_type: ERROR - invalid sensor type specified.\n");
	return APC_SENSOR_OPFAILURE;
}
/*----------------------------------------------------------------------------------*/
static int
map_sensor_to_calib_type
(uint8_t sensorType){
	switch(sensorType){
		case CO_T:
			return CO_RO_T;

			break;
		case O3_T:
			return O3_RO_T;

			break;
		case CO2_T:
			return CO2_RO_T;

			break;
		default:
			PRINTF("map_sensor_to_calib_type: ERROR - invalid sensor type specified.\n");
			return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static int
map_calib_to_sensor_type
(uint8_t calibType){
	switch(calibType){
		case CO_RO_T:
			return CO_T;

			break;
		case O3_RO_T:
			return O3_T;

			break;
		case CO2_RO_T:
			return CO2_T;

			break;
		default:
			PRINTF("map_calib_to_sensor_type: ERROR - invalid sensor type specified.\n");
			return APC_SENSOR_OPFAILURE;
	}
}
/*----------------------------------------------------------------------------------*/
static int
read_calib_sensor
(uint8_t sensorType){
	uint8_t index = get_index_from_sensor_type(sensorType);
	int value;

	if (index == APC_SENSOR_OPFAILURE){
		PRINTF("read_calib_sensor: ERROR - invalid sensor type specified. \n");
		return APC_SENSOR_OPFAILURE;
	}
	int calibType = map_sensor_to_calib_type(sensorType);

	switch(calibType){
		case CO_RO_T:
			value = aqs_sensor.value(MQ7_SENSOR_RO);
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
				sensor_infos[index].isCalibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%d.%03d",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO_RO_T \n");
				PRINTF("RO: %d.%03d\n",
					value / 1000,
					value % 1000);
				PRINTF("-----------------\n");
				return APC_SENSOR_OPSUCCESS;
			}

			break;
		case CO2_RO_T:
			value = aqs_sensor.value(MQ135_SENSOR_RO);
			if (value == AQS_ERROR){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO2_RO_T \n");
				PRINTF("Failed to read sensor.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else if (value == AQS_INITIALIZING){
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO2_RO_T \n");
				PRINTF("sensor is initializing.\n");
				PRINTF("-----------------\n");
				return APC_SENSOR_OPFAILURE;
			}
			else {
				sensor_infos[index].isCalibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%d.%03d",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: CO2_RO_T \n");
				PRINTF("RO: %d.%03d\n",
					value / 1000,
					value % 1000);
				PRINTF("-----------------\n");
				return APC_SENSOR_OPSUCCESS;
			}

			break;
		case O3_RO_T:
			value = aqs_sensor.value(MQ131_SENSOR_RO);
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
				sensor_infos[index].isCalibrated = 1;
				sprintf(sensor_infos[index].sensor_calib_reading,
				"%d.%03d",
				value / 1000,
				value % 1000
				);
				PRINTF("-----------------\n");
				PRINTF("read_sensor: O3_RO_T \n");
				PRINTF("RO: %d.%03d\n",
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
(uint8_t sensorType) {
	static uint8_t index = -1;
	int value;

	for (int i = 0; i < SENSOR_COUNT; i++){
		if ( sensor_infos[i].sensorType == sensorType ){
			index = i;
			break;
		}
	}

	if (index == -1){
		PRINTF("read_sensor: ERROR - invalid sensor type specified. \n");
		return APC_SENSOR_OPFAILURE;
	}

	switch(sensorType){
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
			"%02d.%02d", 
			value / 10, value % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: HUMIDITY_T \n");
			PRINTF("Humidity %02d.%02d RH\n", value / 10, value % 10);
			PRINTF("-----------------\n");
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
			"%02d.%02d", 
			value / 10, value % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T \n");
			PRINTF("Temperature %02d.%02d deg. C\n", value / 10, value % 10);
			PRINTF("-----------------\n");
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
		value = aqs_sensor.value(MQ7_SENSOR);
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
			PRINTF("PPM: %d.%03d\n",
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case CO2_T:
		value = aqs_sensor.value(MQ135_SENSOR);
		if (value == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return APC_SENSOR_OPFAILURE;
		}
		else if (value == AQS_INITIALIZING){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
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
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("PPM: %d.%03d\n", 
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case O3_T:
		value = aqs_sensor.value(MQ131_SENSOR);
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
			PRINTF("PPM: %d.%03d\n", 
				value / 1000,
				value % 1000);
			PRINTF("-----------------\n");
			return APC_SENSOR_OPSUCCESS;
		}
		break;
	case WIND_SPEED_T:
		shared_sensor_select_pin(SHARED_ANEM_SPEED);
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
		shared_sensor_select_pin(SHARED_ANEM_DIRECTION);
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
static void
net_join_as_collector
()
{
	node_data_t msg;
	
	msg.seq = ++seqNo;
	msg.type = COLLECTOR_ADV;
	
	PRINTF("Sending packet to sink (");
	PRINT6ADDR(&sink_addr);
	PRINTF(") with seq 0x%08lx, type %hu, size %hu\n",
		msg.seq, msg.type, sizeof(msg));
	uip_udp_packet_sendto(collect_conn, &msg, sizeof(msg),
		&sink_addr, UIP_HTONS(UDP_SINK_PORT));
}
/*----------------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
	node_data_t* msg;
	
	if( uip_newdata() ) {
		msg = (node_data_t*)uip_appdata;
		
		switch( msg->type ){
			case SINK_ADV:
				PRINTF("--Identified SINK_ADV \n");
				if ( !uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr, &sink_addr_pref) ) {
					uip_ipaddr_copy(&sink_addr_pref, &UIP_IP_BUF->srcipaddr);
					PRINTF("Preferred sink address set to (");
					PRINT6ADDR(&sink_addr_pref);
					PRINTF(")\n");
					process_poll(&apc_sensor_node_collect_adv_process);
				}
				break;
			case COLLECTOR_ACK:
				PRINTF("--Identified COLLECTOR_ACK, resetting dead timer for sink.\n");
				if ( !uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr,&sink_addr_pref) ) {
					uip_ipaddr_copy(&sink_addr_pref, &UIP_IP_BUF->srcipaddr);
					PRINTF("Preferred sink address set to (");
					PRINT6ADDR(&sink_addr_pref);
					PRINTF(")\n");
				}
				timer_set(&sink_dead_timer, CLOCK_SECOND * APC_SENSOR_NODE_SINK_DEADTIME_SECONDS);
				break;
			case DATA_REQUEST:
				PRINTF("--Identified DATA_REQUEST, proceeding to data delivery.\n");
				/* make sure dead timer hasn't expired, and sink address matches source address */
				if ( !timer_expired(&sink_dead_timer) && uip_ipaddr_cmp( &UIP_IP_BUF->srcipaddr, &sink_addr_pref ) ){
					sensor_data_t sensor_data;

					const uint8_t DATA_MAX = SENSOR_COUNT + SENSOR_CALIB_COUNT;
					uint8_t data_left = DATA_MAX;
					uint8_t index = 0;

					//actual sensor values
					for (index = 0; index < SENSOR_COUNT && data_left > 0; index++){
						sensor_data.readings[data_left - 1].type = sensor_infos[index].sensorType;
						sprintf(sensor_data.readings[data_left - 1].data, sensor_infos[index].sensor_reading);

						data_left--;
					}
					//calibration values
					for (index = 0; index < SENSOR_CALIB_COUNT && data_left > 0 ; index++){
						int sensor_type = map_calib_to_sensor_type(SENSOR_CALIB_TYPES[index]);
						if (sensor_type == APC_SENSOR_OPFAILURE)
							continue;

						int calib_index = get_index_from_sensor_type(sensor_type);
						if (calib_index == APC_SENSOR_OPFAILURE)
							continue;

						sensor_data.readings[data_left - 1].type = SENSOR_CALIB_TYPES[index];
						sprintf(sensor_data.readings[data_left - 1].data, sensor_infos[calib_index].sensor_calib_reading);

						data_left--;
					}

					sensor_data.size = DATA_MAX - data_left;
					PRINTF("%d out of %d sensors read\n", sensor_data.size, DATA_MAX);
					msg->type = DATA_ACK;
					msg->seq = ++seqNo;
					memcpy(&msg->reading, &sensor_data, sizeof(sensor_data));
					PRINTF("%d bytes in sensor data\n", sizeof(sensor_data));

					uint16_t packet_size = sizeof(*msg);
					PRINTF("Sending packet to sink (");
					PRINT6ADDR(&sink_addr);
					PRINTF(") with seq 0x%08lx, type DATA_ACK(%hu), size %hu\n",
						msg->seq, msg->type, packet_size);

					uip_udp_packet_sendto(collect_conn, msg, packet_size,
						&sink_addr, UIP_HTONS(UDP_SINK_PORT));
				}
				else {
					PRINTF("(!!!)Sink Dead Timer has expired or source ip address does not match sink address, this node will refuse to send data to sink.\n");
				}
				break;
		}
	}
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
		//no prefix_64, hardcode the address
		uip_ip6addr(collectorAddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
		uip_ds6_set_addr_iid(collectorAddr, &uip_lladdr);
		uip_ds6_addr_add(collectorAddr, 0, ADDR_AUTOCONF);
	}
}
/*----------------------------------------------------------------------------------*/
void
set_remote_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* sinkAddr)
{
	#if UIP_CONF_ROUTER
	/* The choice of server address determines its 6LoWPAN header compression.
	 * Obviously the choice made here must also be selected in apc-sink-node.
	 *
	 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the
	 * 6LowPAN protocol preferences,
	 * e.g. set Context 0 to fd00::. At present Wireshark copies Context/128 and
	 * then overwrites it.
	 * (Setting Context 0 to fd00::1111:2222:3333:4444 will report a 16 bit
	 * compressed address of fd00::1111:22ff:fe33:xxxx)
	 * Note Wireshark's IPCMV6 checksum verification depends on the correct
	 * uncompressed addresses.
	 */
	 
	#if UIP_CONF_SINK_MODE == UIP_CONF_SINK_64_BIT
	/* Mode 1 - 64 bits inline */
	if (prefix_64 == NULL || uip_is_addr_unspecified(prefix_64))
		uip_ip6addr(sinkAddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0xff00);
	else
		uip_ip6addr(sinkAddr, UIP_HTONS(prefix_64->u16[0]), UIP_HTONS(prefix_64->u16[1]), UIP_HTONS(prefix_64->u16[2]), UIP_HTONS(prefix_64->u16[3]), 0, 0, 0, 0xff00);
	PRINTF("Sink set as (");
	PRINT6ADDR(sinkAddr);
	PRINTF(") 64-bit inline mode \n");
	#elif UIP_CONF_SINK_MODE == UIP_CONF_SINK_16_BIT
	/* Mode 2 - 16 bits inline */
	if (prefix_64 == NULL || uip_is_addr_unspecified(prefix_64))
		uip_ip6addr(sinkAddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0x00ff, 0xfe00, 0xff00);
	else
		uip_ip6addr(sinkAddr, UIP_HTONS(prefix_64->u16[0]), UIP_HTONS(prefix_64->u16[1]), UIP_HTONS(prefix_64->u16[2]), UIP_HTONS(prefix_64->u16[3]), 0, 0x00ff, 0xfe00, 0xff00);
	PRINTF("Sink set as (");
	PRINT6ADDR(sinkAddr);
	PRINTF(") 16-bit inline mode \n");
	#else //UIP_CONF_SINK_MODE == UIP_CONF_SINK_LL_DERIVED
	/* Mode 3 - derived from link local (MAC) address */
	/* hardcoded address */
	if (prefix_64 == NULL || uip_is_addr_unspecified(prefix_64))
		uip_ip6addr(sinkAddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0x0212, 0x4b00, 0x1932, 0xe37a);
	else{ //hardcoded address
		memcpy(sinkAddr, prefix_64, 16); //copy 64-bit prefix
		uip_ip6addr(sinkAddr, UIP_HTONS(prefix_64->u16[0]), UIP_HTONS(prefix_64->u16[1]), UIP_HTONS(prefix_64->u16[2]), UIP_HTONS(prefix_64->u16[3]), 0x0212, 0x4b00, 0x1932, 0xe37a);
	}
	PRINTF("Sink set as (");
	PRINT6ADDR(sinkAddr);
	PRINTF(") ll-derived mode \n");
	#endif /* UIP_CONF_SINK_MODE */
	#endif /* UIP_CONF_ROUTER */
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
		/* set server/sink address */
		set_remote_ip_addresses(&prefix, &sink_addr);
	}
}
/*----------------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sensor_node_collect_adv_process, ev, data)
{
	//initialization
	static struct etimer et_adv;
	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Collector Advertise) begins...\n");
	
	etimer_set(&et_adv, CLOCK_SECOND * APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS);
	while(1)
	{
		PROCESS_YIELD();
		if ((ev == PROCESS_EVENT_TIMER && data == &et_adv) ||
				ev == PROCESS_EVENT_POLL){
			leds_on(LEDS_BLUE);
			PRINTF("apc_sensor_node_collect_adv_process: sending collector advertise to sink.\n");
			net_join_as_collector();
			etimer_reset(&et_adv);
			leds_off(LEDS_BLUE);
		}
	}
	
	PROCESS_END();
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
	PRINTF("UIP_CONF_BUFFER_SIZE: %d\n", UIP_CONF_BUFFER_SIZE);
	uip_create_unspecified(&prefix);
	leds_off(LEDS_ALL);
	leds_on(LEDS_GREEN);
	
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
	
	/* set server/sink address */
	set_remote_ip_addresses(&prefix, &sink_addr);
	
	/* new connection with remote host */
	collect_conn = udp_new(NULL, UIP_HTONS(UDP_SINK_PORT), NULL);
	if(collect_conn == NULL) {
		printf("No UDP connection available, exiting the process!\n");
		PROCESS_EXIT();
	}
	udp_bind(collect_conn, UIP_HTONS(UDP_COLLECT_PORT));
	process_start(&apc_sensor_node_collect_adv_process, NULL);
	
	PRINTF("Created a connection with the sink ");
	PRINT6ADDR(&collect_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n",
		UIP_HTONS(collect_conn->lport), UIP_HTONS(collect_conn->rport));
	leds_off(LEDS_GREEN);
	
	print_local_addresses(NULL);
	ctimer_set(&ct_net_print, CLOCK_SECOND * LOCAL_ADDR_PRINT_INTERVAL, print_local_addresses, NULL);
	ctimer_set(&ct_update_addr, CLOCK_SECOND * PREFIX_UPDATE_INTERVAL, update_ip_addresses_prefix, &curPrefix);
	while(1) {
		PROCESS_YIELD();
		if(ev == tcpip_event) {
			leds_on(LEDS_LIGHT_BLUE);
			tcpip_handler();
			leds_off(LEDS_ALL);
		}
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
	static struct etimer et_collect;
	static struct etimer et_read_wait;
	static uint8_t index = 0;
	static uint8_t read_calib_sensors_count = 0;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Collector Gather) begins...\n");
	etimer_set(&et_collect, CLOCK_SECOND * APC_SENSOR_NODE_READ_INTERVAL_SECONDS);
	while (1)
	{
		leds_off(LEDS_YELLOW);
		//wait for a specified interval before reading sensors
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et_collect) );
		PRINTF("apc_sensor_node_collect_gather_process: starting collection\n");
		for (index = 0; index < SENSOR_COUNT; index++){
			read_sensor(sensor_infos[index].sensorType);

			etimer_set(&et_read_wait, APC_SENSOR_NODE_READ_WAIT_MILLIS);
			PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et_read_wait) );
			leds_toggle(LEDS_YELLOW);
		}

		if (read_calib_sensors_count != SENSOR_CALIB_COUNT){
			for (index = 0; index < SENSOR_CALIB_COUNT; index++){
				int sensorType = map_calib_to_sensor_type(SENSOR_CALIB_TYPES[index]);
				int is_calibrated = is_calibrated_sensor(sensorType);

				if ( !is_calibrated ) {

					if (read_calib_sensor(sensorType) == APC_SENSOR_OPSUCCESS)
						read_calib_sensors_count++;
				}
				leds_toggle(LEDS_YELLOW);
			}
		}
		PRINTF("apc_sensor_node_collect_gather_process: collection finished\n");
		etimer_reset(&et_collect);
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
		sensor_infos[i].sensorType = SENSOR_TYPES[i];
		sensor_infos[i].isCalibrated = 0;
		PRINTF("apc_sensor_node_en_sensors_process: Sensor(0x%01x): %s\n", sensor_infos[i].sensorType,
		activate_sensor(sensor_infos[i].sensorType) == APC_SENSOR_OPFAILURE ? "ERROR\0" : "OK\0" );
	}
	//configure the analog multiplexer
	uint8_t select_lines_count = SHARED_SENSOR_MAX_SELECT_LINES;
	PRINTF("Max select lines configured to be %u\n", SHARED_SENSOR_MAX_SELECT_LINES);
	shared_sensor_init(select_lines_count);
	shared_sensor_configure_select_pin(0, APC_SENSOR_NODE_AMUX_0BIT_PIN, APC_SENSOR_NODE_AMUX_0BIT_PORT);

	//share the anemometer and wind vane sensors
	for (i = 0; i < SHARED_SENSOR_MAX_SHARABLE_SENSORS; i++){
		shared_sensor_share_pin(&anem_sensor, shared_sensor_type[i]);
	}

	print_local_dev_info();
	leds_off(LEDS_YELLOW);
	process_start(&apc_sensor_node_collect_gather_process, NULL);
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
