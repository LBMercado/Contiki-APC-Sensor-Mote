#include "contiki.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "dev/dht22.h"
#include "dev/adc-sensors.h"
#include "dev/leds.h"
#include "dev/gpio.h"
#include "net/rime/rime.h"

#include "apc-request.h"

//define pin configuration
#define DHT22_CONF_PIN 5
#define DHT22_CONF_PORT GPIO_A_NUM

//define the number of sensors for this node
#define SENSOR_COUNT 2

//global variables start
static linkaddr_t serverAddr;
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

static const uint8_t 
SENSOR_TYPES[SENSOR_COUNT] =
{
  TEMPERATURE_T,
  HUMIDITY_T
};
