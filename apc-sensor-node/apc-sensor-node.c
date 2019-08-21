//Include files start
#include "apc-sensor-node.h"
//Include files end

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
          printf("Sensor is busy, retrying...\n");
      } while (value1 == DHT22_BUSY);
      if (dht22_read_all(&value1, &value2) != DHT22_ERROR) {
        sprintf(sn_readings.sensor_node_reading[nodeNumber].data, 
          "%02d.%01d", 
          value1 / 10, value2 % 10
          );
        sprintf(sn_readings.sensor_node_reading[nodeNumber + 1].data, 
          "%02d.%01d", 
          value2 / 10, value2 % 10
          );
        printf("-----------------\n");
        printf("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
        printf("Temperature %02d.%01d deg. C, ", value1 / 10, value1 % 10);
        printf("Humidity %02d.%01d RH\n", value2 / 10, value2 % 10);
        printf("-----------------\n");
        return 1;
      }
      else {
        printf("-----------------\n");
        printf("read_sensor: TEMPERATURE_T OR HUMIDITY_T \n");
        printf("Failed to read sensor.\n");
        return 0;
      }
      break;
    default:
      return 0;
  }
}

static void
recv_bc
(struct broadcast_conn *c, const linkaddr_t *from)
{
  struct req_message *msg;
  struct req_message reply;
  int i;

  printf("broadcast message received from %d.%d\n",
         from->u8[0], from->u8[1]);
  leds_toggle(LEDS_GREEN);
  //verify that broadcast is from server
  if (linkaddr_cmp(from, &serverAddr)) {
    printf("--server match\n");
    //restructure that packet data
    msg = packetbuf_dataptr();

    //verify that it is requesting data
    if (msg->type == DATA_REQUEST) {
      printf("--DATA_REQUEST verified, proceeding to sensor reading\n");
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
          printf("%d bytes copied to packet buffer\n", bytesCopied);

          //get the data packet again for debugging purposes
          msg = packetbuf_dataptr();

          unicast_send(&uc, &serverAddr);
          printf("SENSOR_FAIL - faulting sensor type: %s\n", msg->data);
          printf("SENSOR_FAIL - header type %d and value %s - sent to server address %d.%d\n", 
                  msg->type, msg->data,
                  serverAddr.u8[0], serverAddr.u8[1]
                  );
        }        
        else {
          //successfully read sensor
          bytesCopied = packetbuf_copyfrom(&sn_readings.sensor_node_reading[i], sizeof( struct sensor_reading ));

          msg = packetbuf_dataptr();

          printf("%d bytes copied to packet buffer\n", bytesCopied);

          unicast_send(&uc, &serverAddr);
          printf("SENSOR_READ - header type %d and value %s - sent to server address %d.%d\n", 
                msg->type, msg->data,
                serverAddr.u8[0], serverAddr.u8[1]
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
  printf("unicast message received from %d.%d: %s\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

static void
sent_uc
(struct unicast_conn *c, int status, int num_tx) {
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  if(linkaddr_cmp(dest, &linkaddr_null)) {
    printf("unable to send: destination address is null\n");
    return;
  }

  printf("unicast message sent to %d.%d: status %d num_tx %d\n", dest->u8[0], dest->u8[1], status, num_tx);
}

//function definition end
//Process start
PROCESS(apc_sensor_node_unicast_process, "APC Sensor Node (Unicast) Process");
PROCESS(apc_sensor_node_broadcast_process, "APC Sensor Node (Broadcast) Process");

AUTOSTART_PROCESSES(&apc_sensor_node_unicast_process, &apc_sensor_node_broadcast_process);

PROCESS_THREAD(apc_sensor_node_unicast_process, ev, data)
{
  //initialization
  struct req_message msg;
  msg.type = NBR_ADV;

  //pass the event handler for process exit
  PROCESS_EXITHANDLER(unicast_close(&uc));

  //preparations are complete
  PROCESS_BEGIN();

  //enable the sensor
  SENSORS_ACTIVATE(dht22);

  //open this device for unicast networking
  unicast_open(&uc, SENSORNETWORKCHANNEL_UC, &unicast_callback);

  printf("APC Sensor Node (Unicast) begins...\n");
  leds_blink();
  leds_off(LEDS_ALL);

  serverAddr.u8[0] = SERVER_ADDR0; //sets the server address[0]
  serverAddr.u8[1] = SERVER_ADDR1; //sets the server address[1]

  //initialize sensor node readings
  int i;
  for (i = 0; i < SENSOR_COUNT; i++) {
    sn_readings.sensor_node_reading[i].type = SENSOR_TYPES[i];
  }

  //advertise this node to the server
  packetbuf_copyfrom(&msg, sizeof( struct req_message ));
 
  unicast_send(&uc, &serverAddr);
  printf("ADVERTISE - header type %d and value %s - sent to server address %d.%d\n", 
        msg.type, msg.data,
        serverAddr.u8[0], serverAddr.u8[1]
        );

  PROCESS_END();
}

PROCESS_THREAD(apc_sensor_node_broadcast_process, ev, data)
{
  //initialization

  //pass the event handler for process exit
  PROCESS_EXITHANDLER(broadcast_close(&bc));

  //preparations are complete
  PROCESS_BEGIN();

  //open this device for broadcast networking
  broadcast_open(&bc, SENSORNETWORKCHANNEL_BC, &broadcast_callback);

  printf("APC Sensor Node (Broadcast) begins...\n");
  leds_blink();
  leds_off(LEDS_ALL);

  PROCESS_END();
}

//Process end
