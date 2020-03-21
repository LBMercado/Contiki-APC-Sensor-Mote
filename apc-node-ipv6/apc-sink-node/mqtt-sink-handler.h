#ifndef MQTT_SINK_HANDLER_H_
#define MQTT_SINK_HANDLER_H_
/*---------------------------------------------------------------------------*/
/* Contiki Core */
#include "contiki.h"
#include "contiki-net.h"
#include "net/ip/uip.h"
/* Project Sourcefiles */
#include "../apc-common.h"
/*---------------------------------------------------------------------------*/
#define MQTT_OPSUCCESS 0x00
#define MQTT_OPFAILURE 0xFF
/*---------------------------------------------------------------------------*/
int
mqtt_store_sensor_data
(uip_ipaddr_t* nodeAddr, sensor_reading_t* data);
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_handler_process);
/*---------------------------------------------------------------------------*/
#endif /* IFNDEF MQTT_SINK_HANDLER_H_ */
/*---------------------------------------------------------------------------*/
