#ifndef APC_COMMON_H
#define APC_COMMON_H
/*---------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
/*---------------------------------------------------------*/
/* This defines the maximum amount of sensor nodes we can remember. */
#define MAX_SENSOR_NODES 3
/*---------------------------------------------------------*/
#define LOCAL_ADDR_PRINT_INTERVAL 30
#define PREFIX_UPDATE_INTERVAL 20
/*---------------------------------------------------------*/
//these are the types of data that can be communicated between sensor nodes and the sink node
enum
{
	//request/reply headers
	COLLECTOR_ADV, //advertise as collector mote
	COLLECTOR_ACK, //acknowledge as collector mote
	SINK_ADV, //advertise as sink mote
	SINK_ACK, //acknowledge as sink mote
	DATA_REQUEST,
	DATA_ACK,
	
	//error headers
	SENSOR_FAILED, //failed to read sensor values
	
	//sensor data identity headers (also a sensor type)
	TEMPERATURE_T, //unit in Deg. Celsius
	HUMIDITY_T, //unit in %RH
	PM25_T, //unit in ppm, compute to microgram per m3 at processing server
	CO_T, //unitless (ratio in milli scale), compute to ppm at processing server
	/*NO2_T, //unit in ppm, unused */
	CO2_T, //unitless (ratio in milli scale), compute to ppm at processing server
	O3_T, //unitless (ratio in milli scale), compute to ppm at processing server
	WIND_SPEED_T, //unit in m/s
	WIND_DRCTN_T //may be N, S, E, W and their combinations
};
/*---------------------------------------------------------*/
typedef struct {
	uint32_t seq;
	uint8_t type;
	char data[8];
} node_data_t;
/*---------------------------------------------------------*/
typedef struct {
	uint8_t type;
	char data[8];
} sensor_reading_t;
/*---------------------------------------------------------*/
//REF: example-neighbors.c in contiki examples
/* This structure holds information about sensor nodes. */
typedef struct sensor_node {
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
} sensor_node_t;
/*---------------------------------------------------------*/
void
print_local_addresses(void* data);
/*---------------------------------------------------------*/
void
set_local_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* local_addr);
/*---------------------------------------------------------*/
void
set_remote_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* remote_addr);
/*---------------------------------------------------------*/
void
update_ip_addresses_prefix(void* prefix_64);
/*---------------------------------------------------------*/
#endif
/*---------------------------------------------------------*/