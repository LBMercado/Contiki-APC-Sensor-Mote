/* Utility Libraries */
#include "lib/list.h"
#include "lib/memb.h"

/* Project Sourcefiles */
#include "../apc-common.h"

/* This defines the maximum amount of sensor nodes we can remember. */
#define MAX_SENSOR_NODES 3

//amount of time for sink node to wait before requesting data from sensor nodes (seconds)
#define SENSOR_REQUEST_INTERVAL 30

//amount of time for sink node to wait before summarizing the output of the sensor nodes (seconds)
#define SENSOR_SUMMARY_INTERVAL SENSOR_REQUEST_INTERVAL * 10

//send DATA REQUEST message to verified sensor nodes
static void
broadcast_data_request(void);

//summarizes the readings of the sensor nodes in the identified sensor node list
static void
summarize_readings_callback
(void*);

//adds the sensor node associated with the given address to the sensor node list
static void
add_sensor_node
(const uip_ipaddr_t*);

//records or updates the sensor reading value for the given node
static void
update_sensor_node_reading
(const uip_ipaddr_t*, sensor_reading_t*);

//REF: example-neighbors.c in contiki examples
/* This structure holds information about sensor nodes. */
struct sensor_node {
	/* The ->next pointer is needed since we are placing these on a
		Contiki list. */
	struct sensor_node *next;

	/* The ->addr field holds the IPv6 address of the sensor node. */
	uip_ipaddr_t addr;

	/* Sensor readings data for a node */

	char temperature[8]; //unit in Deg. Celsius
	char humidity[8];    //unit in %RH (0%-100%)
	char PM25[8];        //unit in ppm, compute to microgram per m3 at processing server
	char CO[8];          //unitless (ratio in milli scale), compute to ppm at processing server
	char CO2[8];         //unitless (ratio in milli scale), compute to ppm at processing server
	char O3[8];          //unitless (ratio in milli scale), compute to ppm at processing server
	char windSpeed[8];   //unit in m/s
	char windDir[2];     //may be N, S, E, W and their combinations (NW, NE, SW, SE)
};

/* This MEMB() definition defines a memory pool from which we allocate
   sensor node entries entries. */
MEMB(sensor_nodes_memb, struct sensor_node, MAX_SENSOR_NODES);

/* The sensor_nodes_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(sensor_nodes_list);
