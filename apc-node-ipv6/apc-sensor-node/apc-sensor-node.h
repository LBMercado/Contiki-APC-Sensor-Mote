/* Contiki Core and Networking Libraries */
#include "net/ip/uip.h"
/* Project Sourcefiles */
#include "../apc-common.h"

//define the number of sensors for this node (each data value is categorized as a sensor)
#define SENSOR_COUNT      8

//return codes
#define OPERATION_FAILED  0x00
#define OPERATION_SUCCESS 0x01

static int
read_sensor
(uint8_t sensorType, uint8_t index);

static void
net_join_as_collector
(uip_ipaddr_t* sinkAddr);

static const uint8_t 
SENSOR_TYPES[SENSOR_COUNT] =
{
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
