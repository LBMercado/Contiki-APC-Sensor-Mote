#include "contiki.h"
#include "dev/adc-zoul.h"
#include "dev/dht22.h"
#include "dev/air-quality-sensor.h"
#include "dev/anemometer-sensor.h"
#include "dev/pm25-sensor.h"
#include "../apc-request.h"

//define the number of sensors for this node (each data value is categorized as a sensor)
#define SENSOR_COUNT 8

//interval for sampling (in sec)
#define SAMPLING_PERIOD 60

#define WAIT_BUSY_PERIOD 30

//return codes
#define OPERATION_FAILED 0x00
#define OPERATION_SUCCESS 0x01

//function that reads from the specified sensor
//returns nonzero if successful, zero otherwise
static int
read_sensor
(uint8_t sensorType, uint8_t nodeNumber);

//struct containing the sensor readings for this node
struct sensor_node_readings {
  struct sensor_reading sensor_node_reading[SENSOR_COUNT];
};

static struct sensor_node_readings
sn_readings;

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
  WIND_SPEED_T, //unit in mm/s, compute to m/s at processing server
  WIND_DRCTN_T //may be N, S, E, W and their combinations
};
