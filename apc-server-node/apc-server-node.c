#include "apc-server-node.h"

//function definitions start

static void
summarize_readings_callback(void* arg)
{
  struct stimer *st = (struct stimer *) arg;

  struct sensor_node *n;
  unsigned long elapsedSeconds = stimer_elapsed(st);

  printf("\n\n");
  printf("------------------\n");
  printf("SUMMARY - elapsed time %lu:%lu:%lu:%lu\n", 
         (elapsedSeconds / 60 / 60 / 24) % 7, //days,
         (elapsedSeconds / 60 / 60) % 24, //hours
         (elapsedSeconds / 60) % 60, //minutes
         elapsedSeconds % 60 //seconds
        );

  //iterate through each sensor node
  for(n = list_head(sensor_nodes_list); n != NULL; n = list_item_next(n)) {
      //display readings for each node
      printf("--Sensor Node--\n");
      printf("Address: %d.%d\n",n->addr.u8[0],n->addr.u8[1]);
      printf("----Readings----\n");
      printf("Temperature: %s deg. C\n", n->temperature);
      printf("Humidity: %s RH\n", n->humidity);
  }

  printf("------------------\n");
  printf("\n\n");
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
      printf("Failed to allocate sensor node entry.\n");
      return;
    }

    /* Initialize the fields. */
    linkaddr_copy(&n->addr, node_address);

    /* Place the neighbor on the neighbor list. */
    list_add(sensor_nodes_list, n);

    /* Print out a message. */
    printf("New sensor node added with address %d.%d\n",
           node_address->u8[0], node_address->u8[1]
          );
  }

  /* We can now fill in the fields in our neighbor entry. */
  n->last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  n->last_lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);

  /* Print out a message. */
    printf("Sensor node identified with address %d.%d and with RSSI %u, LQI %u\n",
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
         printf("Failed to update sensor node reading (%d.%d) - no matching address found in sensor_nodes_list.\n",
               nodeAddress->u8[0], nodeAddress->u8[1]
               );
         return;
      }
      else{
         //update the sensor values with the given type
         switch (reading->type){
           case TEMPERATURE_T:
             strcpy(n->temperature, reading->data);
             printf("TEMPERATURE_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->temperature);
             break;
           case HUMIDITY_T:
             strcpy(n->humidity, reading->data);
             printf("HUMIDITY_T of sensor node %d.%d UPDATED with value: %s\n", nodeAddress->u8[0], nodeAddress->u8[1], n->humidity);
             break;
           default:
             printf("Failed to update sensor node reading (%d.%d) - unexpected type of sensor reading.\n",
                   nodeAddress->u8[0], nodeAddress->u8[1]
                   );
             //unexpected, do nothing
         }
      }
    }
  else{
    printf("Failed to update sensor node reading - sensor_nodes_list is empty.\n");
  }
}

//this server sends broadcasts, but does not reply to broadcasts
static void
broadcast_recv
(struct broadcast_conn *c, const linkaddr_t *from) { }

static void 
recv_uc
(struct unicast_conn *c, const linkaddr_t *from) {
  struct req_message *msg;
  struct sensor_reading reading;

  msg = packetbuf_dataptr();

  printf("unicast message received from %d.%d with header type: %d and data: %s\n",
         from->u8[0], from->u8[1], msg->type, msg->data
        );

  //check message type
  switch(msg->type){
      case NBR_ADV:
        //add to sensor node list
        printf("--NBR_ADV identified\n");
        add_sensor_node(from);
        break;
      case SENSOR_FAILED:
        printf("--SENSOR_FAILED identified\n");
        printf("Sensor node with address %d.%d reports failure in reading sensors.\n",
              from->u8[0], from->u8[1]
              );
        break;
      case TEMPERATURE_T:
        printf("--TEMPERATURE_T identified\n");
        reading.type = TEMPERATURE_T;
        strcpy(reading.data, msg->data);
        update_sensor_node_reading(from, &reading);
        break;
      case HUMIDITY_T:
        printf("--HUMIDITY_T identified\n");
        reading.type = HUMIDITY_T;
        strcpy(reading.data, msg->data);
        update_sensor_node_reading(from, &reading);
        break;
      default:
        printf("--unidentified header detected\n");
        //unexpected, do nothing
        break;
    }
}

static void
sent_uc
(struct unicast_conn *c, int status, int num_tx) {
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  if(linkaddr_cmp(dest, &linkaddr_null)) {
    printf("unable to send: destination address is null\n");
    return;
  }

  printf("unicast message sent to %d.%d: status %d num_tx %d\n", dest->u8[0], dest->u8[1], status, num_tx);
}

//function definitions end
//Process start
PROCESS(apc_server_node_unicast_process, "APC Server Node (Unicast) Process");
PROCESS(apc_server_node_broadcast_process, "APC Server Node (Broadcast) Process");
PROCESS(apc_server_node_summarizer_process, "APC Server Node Summarizer Process");
AUTOSTART_PROCESSES(&apc_server_node_unicast_process, &apc_server_node_broadcast_process, &apc_server_node_summarizer_process);

PROCESS_THREAD(apc_server_node_unicast_process, ev, data)
{
  //initialization
  linkaddr_t svrAddr = linkaddr_node_addr; //get currently configured rime address of this node

  //pass the event handler for process exit
  PROCESS_EXITHANDLER(unicast_close(&uc));

  //preparations are complete
  PROCESS_BEGIN();

  //open this device for unicast networking
  unicast_open(&uc, SENSORNETWORKCHANNEL_UC, &unicast_callback);

  printf("APC Server Node (Unicast) begins...\n");
  leds_on(LEDS_GREEN);
  printf("Configured Server Address: %d.%d\n", 
    svrAddr.u8[0], svrAddr.u8[1]);

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

  printf("APC Server Node (Broadcast) begins...\n");

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
    printf("BC - DATA_REQUEST sent.\n");
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

  printf("APC Server Node Summarizer begins...\n");
  stimer_set(&st, 60 * 60 * 24 * 7); //set to work for 1 week

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
