#include "contiki.h"
#include "dev/dht22.h"
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#include "dev/pm25-sensor.h"
#include "dev/gpio.h"
#include "net/rime/rime.h"

#include "../apc-request.h"

//define the number of sensors for this node (each data value is categorized as a sensor)
#define SENSOR_COUNT      8

//return codes
#define OPERATION_FAILED  0x00
#define OPERATION_SUCCESS 0x01

//global variables start
static linkaddr_t sinkAddr;
//global variables end

//function that reads from the specified sensor
//returns nonzero if successful, zero otherwise
static int
read_sensor
(uint8_t sensorType, uint8_t nodeNumber);

//function called when a broadcast message is received
static void
recv_bc
(struct broadcast_conn *c, const linkaddr_t *from);

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
broadcast_callback = {recv_bc};

//callback struct to associate with unicast connection
static const struct unicast_callbacks
unicast_callback = {recv_uc, sent_uc};

//the broadcast connection
static struct broadcast_conn
bc;

//the unicast connection
static struct unicast_conn
uc;

//struct containing the sensor readings for this node
struct sensor_node_readings {
  struct sensor_reading sensor_node_reading[SENSOR_COUNT];
};

static struct sensor_node_readings
sn_readings;

/*index is node number
  the value is the sensor type
*/
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
