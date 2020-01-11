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
/*---------------------------------------------------------------------------*/
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
/*----------------------------------------------------------------------------------*/
/* Collect data from sensors every 3 minutes */
#define APC_SENSOR_NODE_READ_INTERVAL_SECONDS             15
#define APC_SENSOR_NODE_COLLECTOR_ADV_INTERVAL_SECONDS    30
#define APC_SENSOR_NODE_SINK_DEADTIME_SECONDS             60
#define UIP_IP_BUF                                        ( (struct uip_ip_hdr*) & uip_buf[UIP_LLH_LEN] )
/*----------------------------------------------------------------------------------*/
static struct timer sink_dead_timer;
static struct uip_udp_conn* collect_conn;
static uip_ipaddr_t sink_addr;
static uip_ipaddr_t sink_addr_pref;
static uip_ipaddr_t prefix;
static sensor_reading_t sensor_readings[SENSOR_COUNT];
static uint32_t seqNo;
/*----------------------------------------------------------------------------------*/
static int
activate_sensor
(uint8_t sensorType){
	int retCode;
	switch(sensorType){
	case HUMIDITY_T:
	case TEMPERATURE_T:
		retCode = SENSORS_ACTIVATE(dht22);
		return retCode != DHT22_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case PM25_T:
		retCode = pm25.configure(SENSORS_ACTIVE, PM25_ENABLE);
		return retCode != PM25_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case CO_T:
		retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ7_SENSOR);
		return retCode != AQS_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case CO2_T:
		retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ135_SENSOR);
		return retCode != AQS_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case O3_T:
		retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ131_SENSOR);
		return retCode != AQS_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case WIND_SPEED_T:
		retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_SPEED_SENSOR);
		return retCode != WIND_SENSOR_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	case WIND_DRCTN_T:
		retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_DIR_SENSOR);
		return retCode != WIND_SENSOR_ERROR ? OPERATION_SUCCESS : OPERATION_FAILED;
		break;
	default:
		PRINTF("activate_sensor: Unknown sensor type given.\n");
		return OPERATION_FAILED;
	}
}
/*----------------------------------------------------------------------------------*/
static int 
read_sensor
(uint8_t sensorType, uint8_t index) {
	int value1, value2;
	switch(sensorType){
		//HUMIDITY_T and TEMPERATURE_T are read at the same sensor
	case HUMIDITY_T:
		index--; // humidity is one index above temperature in sensor reading
	case TEMPERATURE_T:
		//Assume it is busy
		do{
			value1 = dht22.value(DHT22_READ_ALL);
			if (value1 == DHT22_BUSY)
			PRINTF("Sensor is busy, retrying...\n");
		} while (value1 == DHT22_BUSY);
		if (dht22_read_all(&value1, &value2) != DHT22_ERROR) {
			sprintf(sensor_readings[index].data, 
			"%02d.%02d", 
			value1 / 10, value2 % 10
			);
			sprintf(sensor_readings[index + 1].data, 
			"%02d.%02d", 
			value2 / 10, value2 % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			PRINTF("Temperature %02d.%02d deg. C, ", value1 / 10, value1 % 10);
			PRINTF("Humidity %02d.%02d RH\n", value2 / 10, value2 % 10);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		break;
	case PM25_T:
		//pm sensor only measures one value, parameter does not do anything here
		value1 = pm25.value(0); 
		if (value1 == PM25_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sensor_readings[index].data, 
			"%d", 
			value1
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Dust Density: %02d ug/m3\n", value1);
			PRINTF("-----------------\n");
		return OPERATION_SUCCESS;
		}
		break;
	case CO_T:
		value1 = aqs_sensor.value(MQ7_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sensor_readings[index].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Rs/Ro: %d.%03d\n",
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case CO2_T:
		value1 = aqs_sensor.value(MQ135_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sensor_readings[index].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("Rs/Ro: %d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case O3_T:
		value1 = aqs_sensor.value(MQ131_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sensor_readings[index].data, 
			"%d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Rs/Ro: %d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_SPEED_T:
		value1 = anem_sensor.value(WIND_SPEED_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sensor_readings[index].data, 
			"%d.%02d",
			value1,
			value1
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("WIND SPEED: %d.%02d m/s\n", 
				value1 / 100, //whole component
				(value1 % 100)//frac component
			);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_DRCTN_T:
		value1 = anem_sensor.value(WIND_DIR_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("WIND DRCTN (Raw): 0x%04x\n", value1);
			switch (value1){
				case WIND_DIR_NORTH:
					PRINTF("WIND DRCTN (Actual): NORTH\n");
					sprintf(sensor_readings[index].data, 
						"N");
					break;
				case WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): EAST\n");
					sprintf(sensor_readings[index].data, 
						"E");
					break;
				case WIND_DIR_SOUTH:
					PRINTF("WIND DRCTN (Actual): SOUTH\n");
					sprintf(sensor_readings[index].data, 
						"S");
					break;
				case WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): WEST\n");
					sprintf(sensor_readings[index].data, 
						"W");
					break;
				case WIND_DIR_NORTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): NORTHEAST\n");
					sprintf(sensor_readings[index].data, 
						"NE");
					break;
				case WIND_DIR_NORTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): NORTHWEST\n");
					sprintf(sensor_readings[index].data, 
						"NW");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): SOUTHEAST\n");
					sprintf(sensor_readings[index].data, 
						"SE");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): SOUTHWEST\n");
					sprintf(sensor_readings[index].data, 
						"SW");
					break;
				default:
					PRINTF("ERROR: Unexpected value.");
					break;
			}
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	default:
		return OPERATION_FAILED;
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
	uint8_t index;
	
	if( uip_newdata() ) {
		msg = (node_data_t*)uip_appdata;
		
		switch( msg->type ){
			case COLLECTOR_ACK:
				PRINTF("--Identified COLLECTOR_ACK, resetting dead timer for sink.\n");
				if (!uip_ipaddr_cmp(&UIP_IP_BUF->srcipaddr,&sink_addr_pref)) {
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
					for (index = 0; index < SENSOR_COUNT; index++){
						msg->seq = ++seqNo;
						msg->type = SENSOR_TYPES[index];
						sprintf( msg->data, sensor_readings[index].data);
						
						PRINTF("Sending packet to sink (");
						PRINT6ADDR(&sink_addr);
						PRINTF(") with seq 0x%08lx, type %hu, data %s, size %hu\n",
							msg->seq, msg->type, msg->data, sizeof(*msg));
						uip_udp_packet_sendto(collect_conn, msg, sizeof(*msg),
							&sink_addr, UIP_HTONS(UDP_SINK_PORT));
					}
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
PROCESS(apc_sensor_node_network_init_process, "APC Sensor Node (Network Initialization) Process Handler");
PROCESS(apc_sensor_node_collect_adv_process, "APC Sensor Node (Collector Advertise) Process Handler");
PROCESS(apc_sensor_node_collect_gather_process, "APC Sensor Node (Collector Gather) Process Handler");
PROCESS(apc_sensor_node_en_sensors_process, "APC Sensor Node (Sensor Initialization) Process Handler");
/*----------------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&apc_sensor_node_network_init_process, &apc_sensor_node_en_sensors_process);
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
		leds_off(LEDS_BLUE);
		//advertise frequently at set intervals
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et_adv) );
		leds_on(LEDS_BLUE);
		PRINTF("apc_sensor_node_collect_adv_process: sending collector advertise to sink.\n");
		net_join_as_collector();
		etimer_reset(&et_adv);
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
	uip_ipaddr_t collectAddr;
	rpl_dag_t* dag;
	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Network Initialization) begins...\n");
	seqNo = 1;
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
	set_local_ip_addresses(&prefix, &collectAddr);
	
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
	uint8_t index;
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
			read_sensor(SENSOR_TYPES[index], index);
			leds_toggle(LEDS_YELLOW);
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
		sensor_readings[i].type = SENSOR_TYPES[i];
		PRINTF("apc_sensor_node_en_sensors_process: Sensor(0x%01x): %s\n", sensor_readings[i].type,
		activate_sensor(sensor_readings[i].type) == OPERATION_FAILED ? "ERROR\0" : "OK\0" );
	}
	leds_off(LEDS_YELLOW);
	process_start(&apc_sensor_node_collect_gather_process, NULL);
	PROCESS_END();
}
/*----------------------------------------------------------------------------------*/
