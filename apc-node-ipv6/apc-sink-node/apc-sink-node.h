#ifndef APC_SINK_NODE_H
#define APC_SINK_NODE_H
/*---------------------------------------------------------------------------------------------------*/
/* Project Sourcefiles */
#include "../apc-common.h"
/*---------------------------------------------------------------------------------------------------*/
//amount of time for sink node to wait before requesting data from sensor nodes (seconds)
#define SENSOR_REQUEST_INTERVAL 30
//amount of time for sink node to wait before summarizing the output of the sensor nodes (seconds)
#define SENSOR_SUMMARY_INTERVAL SENSOR_REQUEST_INTERVAL * 10
/*---------------------------------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------------------------------*/