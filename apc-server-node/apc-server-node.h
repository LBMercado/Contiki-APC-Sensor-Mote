#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "dev/leds.h"
#include "net/rime/rime.h"
#include "etimer.h"
#include "ctimer.h"
#include "stimer.h"
#include "lib/list.h"
#include "lib/memb.h"

#include "apc-request.h"

//summarizes the readings of the sensor nodes in the identified sensor node list
static void
summarize_readings_callback(void *arg);

//adds the sensor node associated with the given address to the sensor node list
static void
add_sensor_node(const linkaddr_t *node_address);

//records or updates the sensor reading value for the given node
static void
update_sensor_node_reading(const linkaddr_t *nodeAddress, struct sensor_reading *reading);

//function called when a broadcast message is received (UNUSED)
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from);

//function called when a unicast message is received
static void 
recv_uc
(struct unicast_conn *c, const linkaddr_t *from);

//function called when a unicast message is sent
static void 
sent_uc
(struct unicast_conn *c, int status, int num_tx);

//callback struct to associate with broadcast connection
static const struct broadcast_callbacks
broadcast_callback = {broadcast_recv};

//callback struct to associate with unicast connection
static const struct unicast_callbacks
unicast_callback = {recv_uc, sent_uc};

//the broadcast connection
static struct broadcast_conn
bc;

//the unicast connection
static struct unicast_conn 
uc;

//REF: example-neighbors.c in contiki examples
/* This structure holds information about sensor nodes. */
struct sensor_node {
  /* The ->next pointer is needed since we are placing these on a
     Contiki list. */
  struct sensor_node *next;

  /* The ->addr field holds the Rime address of the sensor node. */
  linkaddr_t addr;

  /* The ->last_rssi and ->last_lqi fields hold the Received Signal
     Strength Indicator (RSSI) and CC2420 Link Quality Indicator (LQI)
     values that are received for the incoming broadcast packets. */
  uint16_t last_rssi, last_lqi;

  /* Sensor readings data for this node */

  char temperature[8];
  char humidity[8];
};

/* This #define defines the maximum amount of sensor nodes we can remember. */
#define MAX_SENSOR_NODES 16

/* This MEMB() definition defines a memory pool from which we allocate
   sensor node entries entries. */
MEMB(sensor_nodes_memb, struct sensor_node, MAX_SENSOR_NODES);

/* The sensor_nodes_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(sensor_nodes_list);
