//these are the types of data that can be communicated between sensor nodes and the sink node
enum
{
	//request/reply headers
	COLLECTOR_ADV, //advertise as collector mote
	COLLECTOR_ACK, //acknowledge as collector mote
	SINK_ADV, //advertise as sink mote
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

typedef struct {
	uint32_t seq;
	uint8_t type;
	char data[8];
} node_data_t;

typedef struct {
	uint8_t type;
	char data[8];
} sensor_reading_t;