#ifndef APC_SINK_NODE_H
#define APC_SINK_NODE_H
/*---------------------------------------------------------------------------------------------------*/
/* Project Sourcefiles */
#include "../apc-common.h"
/*---------------------------------------------------------------------------------------------------*/
// amount of time for sink node to wait before requesting data from sensor nodes (in seconds)
#ifndef SENSOR_REQUEST_INTERVAL_CONF
#define SENSOR_REQUEST_INTERVAL 300
#else
#define SENSOR_REQUEST_INTERVAL SENSOR_REQUEST_INTERVAL_CONF
#endif
// amount of time for sink node to wait before summarizing the output of the sensor nodes (seconds)
#define SENSOR_SUMMARY_INTERVAL SENSOR_REQUEST_INTERVAL * 10
/*---------------------------------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------------------------------*/
