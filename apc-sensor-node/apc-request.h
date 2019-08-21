//amount of time for server to wait before requesting data from sensor nodes
#define SENSOR_REQUEST_INTERVAL 30

//these are the types of data that can be communicated between sensor nodes and the server node
enum {
  //request/reply headers
  NBR_ADV, //neighbor advertise
  DATA_REQUEST,
  DATA_ACK, //acknowledge - data received
  DATA_NACK, //not acknowledge - data not received
  //error headers
  SENSOR_FAILED, //failed to read sensor values
  //sensor data identity headers (also a sensor type)
  TEMPERATURE_T,
  HUMIDITY_T
};

struct req_message {
  uint8_t type;
  char data[8];
};

struct sensor_reading {
  uint8_t type;
  char data[8];
};
