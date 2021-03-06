#ifndef APC_COMMON_H
#define APC_COMMON_H
/*---------------------------------------------------------*/
#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
/*---------------------------------------------------------*/
#define LOCAL_ADDR_PRINT_INTERVAL   30
#define PREFIX_UPDATE_INTERVAL      20
/*---------------------------------------------------------*/
//define the number of sensors for each sensor node (each data value is categorized as a sensor)
#define SENSOR_COUNT                8
//define the number of sensors with initial calibration values
#define SENSOR_CALIB_COUNT          3
/*---------------------------------------------------------*/
//these are the types of data that can be communicated between sensor nodes and the sink node
typedef enum
{
	//request/reply headers
	SINK_CMD, // identify header as command from sink
	
	//sensor data identity headers (also a sensor type)
	TEMPERATURE_T, //unit in Deg. Celsius
	HUMIDITY_T, //unit in %RH
	PM25_T, //unit in ug/m3
	CO_T, //unitless, raw resistance ratio to compute ppm
	NO2_T, //unitless, raw resistance ratio to compute ppm
	O3_T, //unitless, raw resistance ratio to compute ppm
	WIND_SPEED_T, //unit in m/s
	WIND_DRCTN_T, //may be N, S, E, W and their combinations

	//calibration values that denote initial config of sensor
	CO_RO_T,
	NO2_RO_T,
	O3_RO_T
} apc_iot_message_t;
/*---------------------------------------------------------*/
typedef struct {
	uint8_t type;
	char data[10];
} sensor_reading_t;
typedef sensor_reading_t mote_message_t;
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
	char PM25[8];        //unit in ug/m3
	char CO[9];          //unitless, raw resistance ratio to compute ppm
	char NO2[9];         //unitless, raw resistance ratio to compute ppm
	char O3[9];          //unitless, raw resistance ratio to compute ppm
	char windSpeed[8];   //unit in m/s
	char windDir[3];     //may be N, S, E, W and their combinations (NW, NE, SW, SE)
	char CO_RO[10];       //base resistance of sensor in ohms, sf. in 3 decimal digits
	char NO2_RO[10];       //base resistance of sensor in ohms, sf. in 3 decimal digits
	char O3_RO[10];       //base resistance of sensor in ohms, sf. in 3 decimal digits
} sensor_node_t;
/*---------------------------------------------------------*/
void
print_local_addresses(void* data);
/*---------------------------------------------------------*/
void
set_local_ip_addresses(uip_ipaddr_t* prefix_64, uip_ipaddr_t* local_addr);
/*---------------------------------------------------------*/
void
update_ip_addresses_prefix(void* prefix_64);
/*---------------------------------------------------------*/
#endif /* ifndef APC_COMMON_H */
/*---------------------------------------------------------*/
