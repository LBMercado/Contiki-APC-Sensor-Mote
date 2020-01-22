/* C std libraries */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* Contiki Core and Networking Libraries */
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
#include "net/rpl/rpl.h"
#include "net/linkaddr.h"
#include "net/netstack.h"
/* Utility Libraries */
#include "lib/list.h"
#include "lib/memb.h"
/* Contiki Dev and Utilities */
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/stimer.h"
/* Project Sourcefiles */
#include "apc-sink-node.h"
#include "mqtt-handler.h"
/*---------------------------------------------------------------------------*/
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
/*---------------------------------------------------------------------------*/
#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_CONF_IPV6_RPL
#error "APC-Sink-Node will be unable to function properly with this current contiki configuration."
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif
/*---------------------------------------------------------------------------*/
#define WEEK_SCALETO_SECOND  604800
#define UIP_IP_BUF           ( (struct uip_ip_hdr*) & uip_buf[UIP_LLH_LEN] )
/*---------------------------------------------------------------------------*/
static struct uip_udp_conn* sink_conn;
static uint32_t seq_id;
static uip_ipaddr_t prefix;
/*---------------------------------------------------------------------------*/
/* This MEMB() definition defines a memory pool from which we allocate
   sensor node entries. */
MEMB(sensor_nodes_memb, sensor_node_t, MAX_SENSOR_NODES);
/* The sensor_nodes_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(sensor_nodes_list);
/*---------------------------------------------------------------------------*/
static void
broadcast_data_request(void)
{
	node_data_t msg;
	struct sensor_node *n;

	msg.type = DATA_REQUEST;
	msg.seq = seq_id++;

	//iterate through each sensor node
	for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
		PRINTF("Sending a DATA_REQUEST to sensor node (");
		PRINT6ADDR(&n->addr);
		PRINTF(") "); 
		PRINTF("with seq 0x%08lu\n", msg.seq);
		uip_ipaddr_copy(&sink_conn->ripaddr, &n->addr);
		uip_udp_packet_send(sink_conn, &msg, sizeof(msg));
	
		//reset dest ip address
		uip_create_unspecified(&sink_conn->ripaddr);
	}
}
/*---------------------------------------------------------------------------*/
static void
summarize_readings_callback(void* arg)
{
	struct stimer *st = (struct stimer *) arg;
	struct sensor_node *n;
	uint32_t elapsedSeconds = stimer_elapsed(st);
	PRINTF("\n\n");
	PRINTF("------------------\n");
	PRINTF("SUMMARY - elapsed time %lu:%lu:%lu:%lu\n", 
	(elapsedSeconds / (60 * 60 * 24) ) % 7, //days,
	(elapsedSeconds / (60 * 60) ) % 24, //hours
	(elapsedSeconds / 60) % 60, //minutes
	elapsedSeconds % 60 //seconds
	);
	//iterate through each sensor node
	for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
		//display readings for each node
		PRINTF("--Sensor Node--\n");
		PRINTF("Address: "); 
		PRINT6ADDR(&n->addr);
		PRINTF("\n");
		PRINTF("----Readings----\n");
		PRINTF("Temperature: %s deg. Celsius\n", n->temperature);
		PRINTF("Humidity: %s %%RH\n", n->humidity);
		PRINTF("PM25: %s ug/m3\n", n->PM25);
		PRINTF("CO: %s PPM\n", n->CO);
		PRINTF("CO2: %s PPM\n", n->CO2);
		PRINTF("O3: %s PPM\n", n->O3);
		PRINTF("Wind Speed: %s\n", n->windSpeed);
		PRINTF("Wind Direction: %s\n", n->windDir);
	}
	PRINTF("------------------\n");
}
/*---------------------------------------------------------------------------*/
static void
add_sensor_node(const uip_ipaddr_t* node_address)
{
	struct sensor_node *n;
	/* Check if we already know this neighbor. */
	for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
		/* We break out of the loop if the address of the neighbor matches
	the address of the neighbor from which we received this
	broadcast message. */
		if(uip_ipaddr_cmp(&n->addr, node_address)) {
			break;
		}
	}
	/* If n is NULL, this neighbor was not found in our list, and we
	allocate a new struct neighbor from the neighbors_memb memory
	pool. */
	if(n == NULL) {
		n = memb_alloc(&sensor_nodes_memb);
		/* If we could not allocate a new neighbor entry, we give up. We
	could have reused an old neighbor entry, but we do not do this
	for now. */
		if(n == NULL) {
			PRINTF("Failed to allocate sensor node entry.\n");
			return;
		}
		/* Initialize the fields. */
		uip_ipaddr_copy(&n->addr, node_address);
		/* Place the neighbor on the neighbor list. */
		list_add(sensor_nodes_list, n);
		/* Print out a message. */
		PRINTF("New sensor node added with address ");
		PRINT6ADDR(node_address);
		PRINTF("\n");
	}
}
/*---------------------------------------------------------------------------*/
static void
update_sensor_node_reading(const uip_ipaddr_t* nodeAddress, sensor_reading_t* reading)
{
	struct sensor_node *n;
	//get the associated sensor node of this address
	if(list_length(sensor_nodes_list) > 0) {
		int i;
		n = list_head(sensor_nodes_list);
		for(i = 0; i < list_length(sensor_nodes_list); i++) {
			//break when matching address is found
			if (uip_ipaddr_cmp(&n->addr, nodeAddress))
			break;
			n = list_item_next(n);
		}
		
		if (n == NULL){
			//no match is found, discontinue
			PRINTF("Failed to update sensor node reading (");
			PRINT6ADDR(nodeAddress);
			PRINTF(") - no matching address found in sensor_nodes_list.\n");
			return;
		}
		else{
			//update the sensor values with the given type
			switch (reading->type){
			case TEMPERATURE_T:
				strcpy(n->temperature, reading->data);
				
				PRINTF("TEMPERATURE_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->temperature);
				break;
			case HUMIDITY_T:
				strcpy(n->humidity, reading->data);
				
				PRINTF("HUMIDITY_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->humidity);
				break;
			case PM25_T:
				strcpy(n->PM25, reading->data);
				
				PRINTF("PM25_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->PM25);
				break;
			case CO_T:
				strcpy(n->CO, reading->data);
				
				PRINTF("CO_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->CO);
				break;
			case CO2_T:
				strcpy(n->CO2, reading->data);
				
				PRINTF("CO2_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->CO2);
				break;
			case O3_T:
				strcpy(n->O3, reading->data);
				
				PRINTF("O3_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->O3);
				break;
			case WIND_SPEED_T:
				strcpy(n->windSpeed, reading->data);
				
				PRINTF("WIND_SPEED_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->windSpeed);
				break;
			case WIND_DRCTN_T:
				strcpy(n->windDir, reading->data);
				
				PRINTF("WIND_DRCTN_T of sensor node (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") UPDATED with value: %s\n", n->windDir);
				break;
			default:
				PRINTF("Failed to update sensor node reading (");
				PRINT6ADDR(nodeAddress);
				PRINTF(") - unexpected type of sensor reading.\n");
				//unexpected, do nothing
			}
		}
	}
	else{
		PRINTF("Failed to update sensor node reading - sensor_nodes_list is empty.\n");
	}
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
	node_data_t* node_message;
	sensor_reading_t sensor_reading;

	if(uip_newdata()) {
		node_message = (node_data_t*)uip_appdata;
		sensor_reading.type = node_message->type;
		strcpy(sensor_reading.data, node_message->data);
		
		PRINTF("apc_sink_node: DATA recv from ");
		PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
		PRINTF("\n");
		PRINTF("Message Type: %hu, Seq. No: 0x%08lx, Size: %hu\n", node_message->type, node_message->seq, sizeof(*node_message));
		//check actual message type
		switch(node_message->type){
		case COLLECTOR_ADV:
			//add to sensor node list
			PRINTF("--COLLECTOR_ADV identified from ");
			PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
			PRINTF("\n");
			add_sensor_node(&UIP_IP_BUF->srcipaddr);
			//store a new copy of the sensor node for mqtt
			if ( mqtt_store_sensor_data(&UIP_IP_BUF->srcipaddr, NULL) == MQTT_OPFAILURE )
				PRINTF("Failed to add sensor node to MQTT.\n");
			
			node_message->type = COLLECTOR_ACK;
			node_message->seq = seq_id++;
			PRINTF("apc_sink_node: sending reply\n");
			//set dest ip address and send packet
			uip_ipaddr_copy(&sink_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
			uip_udp_packet_send(sink_conn, node_message, sizeof(node_data_t));
		
			//reset dest ip address
			uip_create_unspecified(&sink_conn->ripaddr);
			PRINTF("apc_sink_node: reply sent to ");
			PRINT6ADDR(&sink_conn->ripaddr);
			PRINTF("\n");
			break;
		case SENSOR_FAILED:
			PRINTF("--SENSOR_FAILED identified\n");
			PRINTF("Sensor node (");
			PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
			PRINTF(") reports failure in reading sensors.\n");
			break;
		case TEMPERATURE_T:
		case HUMIDITY_T:
		case PM25_T:
		case CO_T:
		case CO2_T:
		case O3_T:
		case WIND_SPEED_T:
		case WIND_DRCTN_T:
			PRINTF("--valid sensor data identified\n");
			update_sensor_node_reading(&UIP_IP_BUF->srcipaddr, &sensor_reading);
			if ( mqtt_store_sensor_data(&UIP_IP_BUF->srcipaddr, &sensor_reading) == MQTT_OPFAILURE )
				PRINTF("Failed to add sensor node to MQTT.\n");
			break;
		default:
			PRINTF("--unidentified header detected\n");
			//unexpected, do nothing
		}
	}
}
/*---------------------------------------------------------------------------*/
void
print_local_addresses(void* data)
{
	int i;
	uint8_t state;

	PRINTF("Server IPv6 addresses: ");
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
void
set_local_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* sinkAddr)
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
	if (prefix_64 == NULL || uip_is_addr_unspecified(prefix_64)){
		uip_ip6addr(sinkAddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
	}
	else{
		memcpy(sinkAddr, prefix_64, 16); //copy 64-bit prefix
		uip_ip6addr(sinkAddr, UIP_HTONS(prefix_64->u16[0]), UIP_HTONS(prefix_64->u16[1]), UIP_HTONS(prefix_64->u16[2]), UIP_HTONS(prefix_64->u16[3]), 0, 0, 0, 0);
	}
	uip_ds6_set_addr_iid(sinkAddr, &uip_lladdr);
	PRINTF("Sink set as (");
	PRINT6ADDR(sinkAddr);
	PRINTF(") ll-derived mode \n");
	#endif /* UIP_CONF_SINK_MODE */
	uip_ds6_addr_add(sinkAddr, 0, ADDR_AUTOCONF);
	#endif /* UIP_CONF_ROUTER */
}
/*----------------------------------------------------------------------------------*/
void
set_remote_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* sinkAddr)
{
	/* this node will get collector addresses on the fly, no need to initialize */
}
/*----------------------------------------------------------------------------------*/
void
update_ip_addresses_prefix(void* prefix_64)
{
	uip_ipaddr_t* pref;
	uip_ipaddr_t sinkAddr;
	
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
		set_local_ip_addresses(&prefix, &sinkAddr);
	}
}
/*---------------------------------------------------------------------------*/
PROCESS(apc_sink_node_server_process, "APC Sink Node (Server) Process Handler");
PROCESS(apc_sink_node_data_request_process, "APC Sink Node (Data Request) Process Handler");
PROCESS(apc_sink_node_summarizer_process, "APC Sink Node (Summarizer) Process Handler");
/*---------------------------------------------------------------------------*/
AUTOSTART_PROCESSES(&apc_sink_node_server_process, &apc_sink_node_summarizer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sink_node_server_process, ev, data)
{
	//initialization
	uip_ipaddr_t sink_addr;
	rpl_dag_t* dag;
	static struct ctimer ct_net_print;
	static struct ctimer ct_update_addr;
	static uip_ipaddr_t curPrefix;
	PROCESS_BEGIN();
	
	PROCESS_PAUSE();
	
	SENSORS_ACTIVATE(button_sensor);
	
	PRINTF("APC Sink Node (Server) begins...\n");
	uip_create_unspecified(&prefix);
	
	seq_id = 0;
	
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
	
	set_local_ip_addresses(&prefix, &sink_addr);
	print_local_addresses(NULL);
	
	/* The data sink runs with a 100% duty cycle in order to ensure high 
	packet reception rates. */
	NETSTACK_MAC.off(1);
	
	sink_conn = udp_new(NULL, UIP_HTONS(UDP_COLLECT_PORT), NULL);
	if(sink_conn == NULL) {
		PRINTF("No UDP connection available, exiting the process!\n");
		PROCESS_EXIT();
	}
	udp_bind(sink_conn, UIP_HTONS(UDP_SINK_PORT));

	PRINTF("Created a sink connection with remote address ");
	PRINT6ADDR(&sink_conn->ripaddr);
	PRINTF(" local/remote port %u/%u\n", UIP_HTONS(sink_conn->lport),
			 UIP_HTONS(sink_conn->rport));
	process_start(&apc_sink_node_data_request_process, NULL);
	process_start(&mqtt_handler_process, NULL);
	ctimer_set(&ct_net_print, CLOCK_SECOND * LOCAL_ADDR_PRINT_INTERVAL, print_local_addresses, NULL);
	ctimer_set(&ct_update_addr, CLOCK_SECOND * PREFIX_UPDATE_INTERVAL, update_ip_addresses_prefix, &curPrefix);
	while(1){
		PROCESS_YIELD();
		if(ev == tcpip_event) {
			tcpip_handler();
		} else if (ev == sensors_event && data == &button_sensor) {
			PRINTF("Initiating global repair\n");
			rpl_repair_root(RPL_DEFAULT_INSTANCE);
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
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(apc_sink_node_data_request_process, ev, data)
{
	//initialization
	static struct etimer et_request_timer;
	
	//preparations are complete
	PROCESS_BEGIN();
	PRINTF("APC Sink Node (Data Request) begins...\n");
		
	//send DATA_REQUEST multicast at regular intervals
	while(1){
		etimer_set(&et_request_timer, CLOCK_SECOND * SENSOR_REQUEST_INTERVAL);
		//wait until the timer expiration to multicast event
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_request_timer));
		
		//multicast sensor reading request
		leds_on(LEDS_GREEN);
		broadcast_data_request();
		leds_off(LEDS_GREEN);
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
//this node summarizes the readings of the nodes at an interval
PROCESS_THREAD(apc_sink_node_summarizer_process, ev, data)
{
	//initialization
	static struct ctimer ct;
	static struct stimer st;
	//preparations are complete
	PROCESS_BEGIN();
	PRINTF("APC Sink Node (Summarizer) begins...\n");
	stimer_set(&st, WEEK_SCALETO_SECOND);
	//set the summarizer callback timer
	ctimer_set(&ct, CLOCK_SECOND * SENSOR_SUMMARY_INTERVAL, 
	summarize_readings_callback, //callback function
	&st//callback function args
	);
	//summarize sensor node readings at regular intervals
	while(1){
		static struct etimer et;
		etimer_set(&et, CLOCK_SECOND * SENSOR_SUMMARY_INTERVAL);
		
		//wait until the specified amount of time before restarting the callback function timer
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		ctimer_restart(&ct);
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/