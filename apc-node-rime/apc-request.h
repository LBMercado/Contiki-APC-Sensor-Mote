//these are the types of data that can be communicated between sensor nodes and the sink node
enum
{
  //request/reply headers
  NBR_ADV, //neighbor advertise
  DATA_REQUEST,
  DATA_ACK, //acknowledge - data received
  DATA_NACK, //not acknowledge - data not received
  
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

struct req_message {
  uint8_t type;
  char data[8];
};

struct sensor_reading {
  uint8_t type;
  char data[8];
};