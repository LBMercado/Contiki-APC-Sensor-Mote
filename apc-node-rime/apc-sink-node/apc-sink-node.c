#include "apc-sink-node.h"

//debugging purposes
#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define WEEK_SCALETO_SECOND 604800

//function definitions start
static void
summarize_readings_callback(void* arg)
{
	struct stimer *st = (struct stimer *) arg;
	struct sensor_node *n;
	uint32_t elapsedSeconds = stimer_elapsed(st);
	PRINTF("\n\n");
	PRINTF("------------------\n");
	PRINTF("SUMMARY - elapsed time %lu:%lu:%lu:%lu\n", 
	(elapsedSeconds / 60 * 60 * 24 ) % 7, //days,
	(elapsedSeconds / 60 * 60) % 24, //hours
	(elapsedSeconds / 60) % 60, //minutes
	elapsedSeconds % 60 //seconds
	);
	//iterate through each sensor node
	for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
		//display readings for each node
		PRINTF("--Sensor Node--\n");
		PRINTF("Address: %d.%d\n",n->addr.u8[0],n->addr.u8[1]);
		PRINTF("----Readings----\n");
		PRINTF("Temperature: %s deg. Celsius\n", n->temperature);
		PRINTF("Humidity: %s %%RH\n", n->humidity);
		PRINTF("PM25: %s ppm\n", n->PM25);
		PRINTF("CO: %s Rs/Ro\n", n->CO);
		PRINTF("CO2: %s Rs/Ro\n", n->CO2);
		PRINTF("O3: %s Rs/Ro\n", n->O3);
		PRINTF("Wind Speed: %s\n", n->windSpeed);
		PRINTF("Wind Direction: %s\n", n->windDir);
	}
	PRINTF("------------------\n");
	PRINTF("\n\n");
}
static void
add_sensor_node(const linkaddr_t *node_address)
{
	struct sensor_node *n;
	/* Check if we already know this neighbor. */
	for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
		/* We break out of the loop if the address of the neighbor matches
	the address of the neighbor from which we received this
	broadcast message. */
		if(linkaddr_cmp(&n->addr, node_address)) {
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
		linkaddr_copy(&n->addr, node_address);
		/* Place the neighbor on the neighbor list. */
		list_add(sensor_nodes_list, n);
		/* Print out a message. */
		PRINTF("New sensor node added with address %d.%d\n",
		node_address->u8[0], node_address->u8[1]
		);
	}
	/* We can now fill in the fields in our neighbor entry. */
	n->last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	n->last_lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
	/* Print out a message. */
	PRINTF("Sensor node identified with address %d.%d and with RSSI %u, LQI %u\n",
	node_address->u8[0], node_address->u8[1],
	packetbuf_attr(PACKETBUF_ATTR_RSSI),
	packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY)
	);
}
static void
update_sensor_node_reading(const linkaddr_t *nodeAddress, struct sensor_reading *reading)
{
	struct sensor_node *n;
	//get the associated sensor node of this address
	if(list_length(sensor_nodes_list) > 0) {
		int i;
		n = list_head(sensor_nodes_list);
		for(i = 0; i < list_length(sensor_nodes_list); i++) {
			//break when matching address is found
			if (linkaddr_cmp(&n->addr, nodeAddress))
			break;
			n = list_item_next(n);
		}
		
		if (n == NULL){
			//no match is found, discontinue
			PRINTF("Failed to update sensor node reading (%d.%d) - no matching address found in sensor_nodes_list.\n",
			nodeAddress->u8[0], nodeAddress->u8[1]
			);
			return;
		}
		else{
			//update the sensor values with the given type
			switch (reading->type){
			case TEMPERATURE_T:
				strcpy(n->temperature, reading->data);
				PRINTF("TEMPERATURE_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->temperature);
				break;
			case HUMIDITY_T:
				strcpy(n->humidity, reading->data);
				PRINTF("HUMIDITY_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->humidity);
				break;
			case PM25_T:
				strcpy(n->PM25, reading->data);
				PRINTF("PM25_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->PM25);
				break;
			case CO_T:
				strcpy(n->CO, reading->data);
				PRINTF("CO_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->CO);
				break;
			case CO2_T:
				strcpy(n->CO2, reading->data);
				PRINTF("CO2_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->CO2);
				break;
			case O3_T:
				strcpy(n->O3, reading->data);
				PRINTF("O3_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->O3);
				break;
			case WIND_SPEED_T:
				strcpy(n->windSpeed, reading->data);
				PRINTF("WIND_SPEED_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->windSpeed);
				break;
			case WIND_DRCTN_T:
				strcpy(n->windDir, reading->data);
				PRINTF("WIND_DRCTN_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->windDir);
				break;
			default:
				PRINTF("Failed to update sensor node reading (%d.%d) - unexpected type of sensor reading.\n",
				nodeAddress->u8[0], nodeAddress->u8[1]
				);
				//unexpected, do nothing
			}
		}
	}
	else{
		PRINTF("Failed to update sensor node reading - sensor_nodes_list is empty.\n");
	}
}
//this sink sends broadcasts, but does not reply to broadcasts
static void
broadcast_recv
(struct broadcast_conn *c, const linkaddr_t *from) { }

static void 
recv_uc
(struct unicast_conn *c, const linkaddr_t *from) {
	struct req_message *msg;
	struct sensor_reading reading;
	msg = packetbuf_dataptr();
	PRINTF("unicast message received from %d.%d with header type: %d and data: %s\n",
	from->u8[0], from->u8[1], msg->type, msg->data
	);
	//check message type
	switch(msg->type){
	case NBR_ADV:
		//add to sensor node list
		PRINTF("--NBR_ADV identified\n");
		add_sensor_node(from);
		break;
	case SENSOR_FAILED:
		PRINTF("--SENSOR_FAILED identified\n");
		PRINTF("Sensor node with address %d.%d reports failure in reading sensors.\n",
		from->u8[0], from->u8[1]
		);
		break;
	case TEMPERATURE_T:
		PRINTF("--TEMPERATURE_T identified\n");
		reading.type = TEMPERATURE_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case HUMIDITY_T:
		PRINTF("--HUMIDITY_T identified\n");
		reading.type = HUMIDITY_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case PM25_T:
		PRINTF("--PM25_T identified\n");
		reading.type = PM25_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case CO_T:
		PRINTF("--CO_T identified\n");
		reading.type = CO_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case CO2_T:
		PRINTF("--CO2_T identified\n");
		reading.type = CO2_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case O3_T:
		PRINTF("--O3_T identified\n");
		reading.type = O3_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case WIND_SPEED_T:
		PRINTF("--WIND_SPEED_T identified\n");
		reading.type = WIND_SPEED_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	case WIND_DRCTN_T:
		PRINTF("--WIND_DRCTN_T identified\n");
		reading.type = WIND_DRCTN_T;
		strcpy(reading.data, msg->data);
		update_sensor_node_reading(from, &reading);
		break;
	default:
		PRINTF("--unidentified header detected\n");
		//unexpected, do nothing
	}
}
static void
sent_uc
(struct unicast_conn *c, int status, int num_tx) {
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
	if(linkaddr_cmp(dest, &linkaddr_null)) {
		PRINTF("unable to send: destination address is null\n");
		return;
	}
	PRINTF("unicast message sent to %d.%d: status %d num_tx %d\n", dest->u8[0], dest->u8[1], status, num_tx);
}
//function definitions end
//Process start
PROCESS(apc_server_node_unicast_process, "APC Sink Node (Unicast) Process Handler");
PROCESS(apc_server_node_broadcast_process, "APC Sink Node (Broadcast) Process Handler");
PROCESS(apc_server_node_summarizer_process, "APC Sink Node (Summarizer) Process Handler");
AUTOSTART_PROCESSES(&apc_server_node_unicast_process, &apc_server_node_broadcast_process, &apc_server_node_summarizer_process);
PROCESS_THREAD(apc_server_node_unicast_process, ev, data)
{
	//initialization
	linkaddr_t sinkAddr = linkaddr_node_addr; //get currently configured rime address of this node
	//pass the event handler for process exit
	PROCESS_EXITHANDLER(unicast_close(&uc));
	//preparations are complete
	PROCESS_BEGIN();
	//open this device for unicast networking
	unicast_open(&uc, SENSORNETWORKCHANNEL_UC, &unicast_callback);
	PRINTF("APC Sink Node (Unicast) begins...\n");
	leds_on(LEDS_GREEN);
	PRINTF("Configured Sink Address: %d.%d\n", 
	sinkAddr.u8[0], sinkAddr.u8[1]);
	PROCESS_END();
}
PROCESS_THREAD(apc_server_node_broadcast_process, ev, data)
{
	//initialization
	static struct etimer et;
	struct req_message msg;
	//pass the event handler for process exit
	PROCESS_EXITHANDLER(broadcast_close(&bc));
	//preparations are complete
	PROCESS_BEGIN();
	//open this device for unicast networking
	broadcast_open(&bc, SENSORNETWORKCHANNEL_BC, &broadcast_callback);
	PRINTF("APC Sink Node (Broadcast) begins...\n");
	//send DATA_REQUEST broadcast at regular intervals
	while(1){
		
		etimer_set(&et, CLOCK_SECOND * SENSOR_REQUEST_INTERVAL);
		//wait until the timer expiration to broadcast event
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		
		//broadcast sensor reading request
		msg.type = DATA_REQUEST;
		packetbuf_copyfrom(&msg, sizeof(struct req_message));
		leds_toggle(LEDS_GREEN);
		broadcast_send(&bc);
		leds_toggle(LEDS_GREEN);
		PRINTF("BC - DATA_REQUEST sent.\n");
	}
	PROCESS_END();
}
//this node summarizes the readings of the nodes at an interval
PROCESS_THREAD(apc_server_node_summarizer_process, ev, data)
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
//Process end
