//Include files start
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "dev/leds.h"
#include "apc-sensor-node.h"
#include "dev/air-quality-sensor.h"
#include "dev/anemometer-sensor.h"
//Include files end

//debugging purposes
#define DEBUG 1
#if DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif

//function definition start

static int
read_sensor
(uint8_t sensorType, uint8_t nodeNumber) {

	int value1, value2;

	switch(sensorType){
		//HUMIDITY_T and TEMPERATURE_T are read at the same sensor
	case HUMIDITY_T:
		nodeNumber--; // humidity is one index above temperature in sensor reading
	case TEMPERATURE_T:
		//Assume it is busy
		do{
			value1 = dht22.value(DHT22_READ_ALL);
			if (value1 == DHT22_BUSY)
			PRINTF("Sensor is busy, retrying...\n");
		} while (value1 == DHT22_BUSY);
		if (dht22_read_all(&value1, &value2) != DHT22_ERROR) {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%02d", 
			value1 / 10, value2 % 10
			);
			sprintf(sn_readings.sensor_node_reading[nodeNumber + 1].data, 
			"%02d.%02d", 
			value2 / 10, value2 % 10
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			PRINTF("Temperature %02d.%02d deg. C, ", value1 / 10, value1 % 10);
			PRINTF("Humidity %02d.%02d RH\n", value2 / 10, value2 % 10);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		break;
	case PM25_T:
		//pm sensor only measures one value, parameter does not do anything here
		value1 = pm25.value(0); 
		if (value1 == PM25_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d", 
			value1
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: PM25_T \n");
			PRINTF("Dust Density: %d ug/m3\n", value1);
			PRINTF("-----------------\n");
		return OPERATION_SUCCESS;
		}
		break;
	case CO_T:
		value1 = aqs_sensor.value(MQ7_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO_T \n");
			PRINTF("Rs/Ro: %02d.%03d\n",
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case CO2_T:
		value1 = aqs_sensor.value(MQ135_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: CO2_T \n");
			PRINTF("Rs/Ro: %02d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case O3_T:
		value1 = aqs_sensor.value(MQ131_SENSOR);
		if (value1 == AQS_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else if (value1 == AQS_BUSY){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("sensor is busy.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%03d", 
			value1 / 1000,
			value1 % 1000
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: O3_T \n");
			PRINTF("Rs/Ro: %02d.%03d\n", 
				value1 / 1000,
				value1 % 1000);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_SPEED_T:
		value1 = anem_sensor.value(WIND_SPEED_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
			"%02d.%02d",
			value1 / 100,
			value1 % 100
			);
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_SPEED_T \n");
			PRINTF("WIND SPEED: %02d.%02d m/s\n", 
				value1 / 100, //whole component
				(value1 % 100)//frac component
			);
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	case WIND_DRCTN_T:
		value1 = anem_sensor.value(WIND_DIR_SENSOR);
		if (value1 == WIND_SENSOR_ERROR){
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("Failed to read sensor.\n");
			PRINTF("-----------------\n");
			return OPERATION_FAILED;
		}
		else {
			PRINTF("-----------------\n");
			PRINTF("read_sensor: WIND_DRCTN_T \n");
			PRINTF("WIND DRCTN (Raw): 0x%04x\n", value1);
			switch (value1){
				case WIND_DIR_NORTH:
					PRINTF("WIND DRCTN (Actual): NORTH\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"N");
					break;
				case WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): EAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"E");
					break;
				case WIND_DIR_SOUTH:
					PRINTF("WIND DRCTN (Actual): SOUTH\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"S");
					break;
				case WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): WEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"W");
					break;
				case WIND_DIR_NORTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): NORTHEAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"NE");
					break;
				case WIND_DIR_NORTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): NORTHWEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"NW");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_EAST:
					PRINTF("WIND DRCTN (Actual): SOUTHEAST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"SE");
					break;
				case WIND_DIR_SOUTH | WIND_DIR_WEST:
					PRINTF("WIND DRCTN (Actual): SOUTHWEST\n");
					sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
						"SW");
					break;
				default:
					PRINTF("ERROR: Unexpected value.");
					break;
			}
			PRINTF("-----------------\n");
			return OPERATION_SUCCESS;
		}
		break;
	default:
		return OPERATION_FAILED;
	}
}

static void
recv_bc
(struct broadcast_conn *c, const linkaddr_t *from)
{
	static struct req_message *msg;
	static struct req_message reply;
	static int i;

	PRINTF("broadcast message received from %d.%d\n",
	from->u8[0], from->u8[1]);
	leds_toggle(LEDS_GREEN);
	//verify that broadcast is from server
	if (linkaddr_cmp(from, &sinkAddr)) {
		PRINTF("--server match\n");
		//restructure that packet data
		msg = packetbuf_dataptr();

		//verify that it is requesting data
		if (msg->type == DATA_REQUEST) {
			PRINTF("--DATA_REQUEST verified, proceeding to sensor reading\n");
			//read data now
			for (i = 0; i < SENSOR_COUNT; i++) {
				int bytesCopied;

				if (!read_sensor(sn_readings.sensor_node_reading[i].type, i)) {
					//failed to read sensor
					reply.type = SENSOR_FAILED;

					//convert uint8_t to char array (base 10 formatting)
					itoa(sn_readings.sensor_node_reading[i].type,
					reply.data,
					10
					);

					bytesCopied = packetbuf_copyfrom(&reply, sizeof( struct req_message ));
					PRINTF("%d bytes copied to packet buffer\n", bytesCopied);

					//get the data packet again for debugging purposes
					msg = packetbuf_dataptr();

					unicast_send(&uc, &sinkAddr);
					PRINTF("SENSOR_FAIL - faulting sensor type: %s\n", msg->data);
					PRINTF("SENSOR_FAIL - header type %d and value %s - sent to server address %d.%d\n", 
					msg->type, msg->data,
					sinkAddr.u8[0], sinkAddr.u8[1]
					);
				}        
				else {
					//successfully read sensor
					bytesCopied = packetbuf_copyfrom(&sn_readings.sensor_node_reading[i], sizeof( struct sensor_reading ));

					msg = packetbuf_dataptr();

					PRINTF("%d bytes copied to packet buffer\n", bytesCopied);

					unicast_send(&uc, &sinkAddr);
					PRINTF("SENSOR_READ - header type %d and value %s - sent to server address %d.%d\n", 
					msg->type, msg->data,
					sinkAddr.u8[0], sinkAddr.u8[1]
					);
				}
			}
		}
		//if not, do nothing
	}
	leds_toggle(LEDS_GREEN);
}

static void 
recv_uc
(struct unicast_conn *c, const linkaddr_t *from) {
	PRINTF("unicast message received from %d.%d: %s\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

static void
sent_uc
(struct unicast_conn *c, int status, int num_tx) {
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

	if(linkaddr_cmp(dest, &linkaddr_null)) {
		PRINTF("unable to send: destination address is null\n");
		return;
	}

	PRINTF("unicast message sent to %d.%d: status %d num_tx %d\n", dest->u8[0], dest->u8[1], status, num_tx);
}

//function definition end
//Process start
PROCESS(apc_sensor_node_unicast_process, "APC Sensor Node (Unicast) Process Handler");
PROCESS(apc_sensor_node_broadcast_process, "APC Sensor Node (Broadcast) Process Handler");
PROCESS(apc_sensor_node_en_sensors_process, "APC Sensor Node (Sensor Initialization) Process Handler");

AUTOSTART_PROCESSES(&apc_sensor_node_unicast_process, &apc_sensor_node_broadcast_process,
	&apc_sensor_node_en_sensors_process);

PROCESS_THREAD(apc_sensor_node_unicast_process, ev, data)
{
	//initialization
	static struct req_message msg;
	msg.type = NBR_ADV;

	//pass the event handler for process exit
	PROCESS_EXITHANDLER(unicast_close(&uc));

	//preparations are complete
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Unicast) begins...\n");

	//open this device for unicast networking
	unicast_open(&uc, SENSORNETWORKCHANNEL_UC, &unicast_callback);
	PRINTF("apc_sensor_node_unicast_process: unicast opened\n");
	leds_blink();
	leds_off(LEDS_ALL);

	sinkAddr.u8[0] = SINK_ADDR0; //sets the server address[0]
	sinkAddr.u8[1] = SINK_ADDR1; //sets the server address[1]

	//initialize sensor node readings
	int i;
	for (i = 0; i < SENSOR_COUNT; i++) {
		sn_readings.sensor_node_reading[i].type = SENSOR_TYPES[i];
	}

	//advertise this node to the server
	packetbuf_copyfrom(&msg, sizeof( struct req_message ));

	unicast_send(&uc, &sinkAddr);
	PRINTF("ADVERTISE - header type %d and value %s - sent to server address %d.%d\n", 
	msg.type, msg.data,
	sinkAddr.u8[0], sinkAddr.u8[1]
	);
	leds_blink();
	leds_off(LEDS_ALL);

	PROCESS_END();
}

PROCESS_THREAD(apc_sensor_node_broadcast_process, ev, data)
{
	//initialization

	//pass the event handler for process exit
	PROCESS_EXITHANDLER(broadcast_close(&bc));

	//preparations are complete
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Broadcast) begins...\n");

	//open this device for broadcast networking
	broadcast_open(&bc, SENSORNETWORKCHANNEL_BC, &broadcast_callback);
	PRINTF("apc_sensor_node_broadcast_process: broadcast opened\n");
	
	leds_blink();
	leds_off(LEDS_ALL);

	PROCESS_END();
}

PROCESS_THREAD(apc_sensor_node_en_sensors_process, ev, data)
{
	//initialization
	int retCode = 0;

	//pass the event handler for process exit
	PROCESS_EXITHANDLER();

	//preparations are complete
	PROCESS_BEGIN();
	PRINTF("APC Sensor Node (Sensor Initialization) begins...\n");

	PRINTF("apc_sensor_node_en_sensors_process: enabling sensors...\n");
	
	//enable the sensors
	retCode = SENSORS_ACTIVATE(dht22);
	PRINTF("apc_sensor_node_en_sensors_process: TEMPERATURE_T & HUMIDITY_T: %s\n", 
		retCode == DHT22_ERROR ? "ERROR\0" : "OK\0" );
	retCode = pm25.configure(SENSORS_ACTIVE, PM25_ENABLE);
	PRINTF("apc_sensor_node_en_sensors_process: PM25_T: %s\n",
		retCode == PM25_ERROR ? "ERROR\0" : "OK\0");
	retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ7_SENSOR);
	PRINTF("apc_sensor_node_en_sensors_process: CO_T: %s\n",
		retCode == AQS_ERROR ? "ERROR\0" : "OK\0");
	retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ131_SENSOR);
	PRINTF("apc_sensor_node_en_sensors_process: O3_T: %s\n",
		retCode == AQS_ERROR ? "ERROR\0" : "OK\0");
	retCode = aqs_sensor.configure(SENSORS_ACTIVE, MQ135_SENSOR);
	PRINTF("apc_sensor_node_en_sensors_process: CO2_T: %s\n",
		retCode == AQS_ERROR ? "ERROR\0" : "OK\0");
	retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_SPEED_SENSOR);
	PRINTF("apc_sensor_node_en_sensors_process: WIND_SPEED_T: %s\n",
		retCode == WIND_SENSOR_ERROR ? "ERROR\0" : "OK\0");
	retCode = anem_sensor.configure(SENSORS_ACTIVE, WIND_DIR_SENSOR);
	PRINTF("apc_sensor_node_en_sensors_process: WIND_DRCTN_T: %s\n",
		retCode == WIND_SENSOR_ERROR ? "ERROR\0" : "OK\0");
	PRINTF("apc_sensor_node_en_sensors_process: sensors enabled\n");
	leds_blink();
	leds_off(LEDS_ALL);
	PROCESS_END();
}

//Process end
