#ifndef APC_SENSOR_NODE_H_
#define APC_SENSOR_NODE_H_
/* Project Sourcefiles */
#include "../apc-common.h"

//return codes
#define APC_SENSOR_OPFAILURE        (-1)
#define APC_SENSOR_OPSUCCESS        0x01

extern const uint8_t
SENSOR_TYPES[SENSOR_COUNT];

extern const uint8_t
SENSOR_CALIB_TYPES[SENSOR_CALIB_COUNT];

#endif /* ifndef APC_SENSOR_NODE_H_ */
