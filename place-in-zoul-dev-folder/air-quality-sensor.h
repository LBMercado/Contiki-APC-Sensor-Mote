/*
Acknowledgement

Thanks to Ingo Lohs for providing a brief but descriptive guide in using MQ7 sensor. 
(Originally Arduino code, adapted to Contiki code)
Refer to https://www.hackster.io/ingo-lohs/gas-sensor-carbon-monoxide-mq-7-aka-flying-fish-e58457

Thanks to Dmitry Dziuba for providing the source code to make MQ7 more accurate
Refer to https://github.com/ultimaterobotics/mq7-co-monitor/blob/master/co_monitor.ino
*/

//include files
#include "contiki.h"
#include "lib/sensors.h"
#include "dev/adc-zoul.h"
#include "dev/gpio.h"
#include <stdio.h>
/*------------------------------------------------------------------*/
#ifndef AQS_H_ 
#define AQS_H_
/*------------------------------------------------------------------*/
//Available sensors to use
#define MQ7_SENSOR                 0x00 //CO, ppm
#define MQ131_SENSOR               0x01 //O, ppm
#define MQ135_SENSOR               0x02 //NOx/NO2/CO2, ppm
/*------------------------------------------------------------------*/
//return codes
#define AQS_ERROR                  (-1)
#define AQS_SUCCESS                0x00
/*------------------------------------------------------------------*/
//sensor states
#define AQS_ENABLED                0x01
#define AQS_READY                  AQS_ENABLED
#define AQS_INIT_PHASE             0x0F
#define AQS_BUSY                   0xFF
/*------------------------------------------------------------------*/
//configure options
#define AQS_ENABLE                 SENSORS_ACTIVE
#define AQS_DISABLE                0x00
/*------------------------------------------------------------------*/
//timings
/* heating time in seconds */
#define MQ7_HEATING_HIGH_TIME      60
#define MQ7_HEATING_LOW_TIME       90
#define MQ131_HEATING_TIME         100
#define MQ135_HEATING_TIME         100
#define MQ131_MEASUREMENT_INTERVAL 60
#define MQ135_MEASUREMENT_INTERVAL 60
/*------------------------------------------------------------------*/
//reference resistances
//used for computing RS
#ifndef MQ7_CONF_RL_KOHM
	#define MQ7_RL_KOHM 0.01
#else
	#define MQ7_RL_KOHM MQ7_CONF_RL_KOHM
#endif
#ifndef MQ131_CONF_RL_KOHM
	#define MQ131_RL_KOHM 0.01
#else
	#define MQ131_RL_KOHM MQ131_CONF_RL_KOHM
#endif
#ifndef MQ135_CONF_RL_KOHM
	#define MQ135_RL_KOHM 0.01
#else
	#define MQ135_RL_KOHM MQ135_CONF_RL_KOHM
#endif
/*------------------------------------------------------------------*/
//initial values
/* set to zero to calibrate at runtime */
//(in ohm) with reference to 100ppm CO in clean air
#ifndef MQ7_CONF_RO_CLEAN_AIR
	#define MQ7_RO_CLEAN_AIR 0 
#else
	#define MQ7_RO_CLEAN_AIR MQ7_CONF_RO_CLEAN_AIR
#endif
//(in ohm) with reference to 50ppm CL2 in clean air
#ifndef MQ131_CONF_RO_CLEAN_AIR
	#define MQ131_RO_CLEAN_AIR 0
#else
	#define MQ131_RO_CLEAN_AIR MQ131_CONF_RO_CLEAN_AIR
#endif
 //(in ohm) with reference to 100ppm NH3 in clean air
#ifndef MQ135_CONF_RO_CLEAN_AIR
	#define MQ135_RO_CLEAN_AIR 0
#else
	#define MQ135_RO_CLEAN_AIR MQ135_CONF_RO_CLEAN_AIR
#endif
/* number of heating cycles before actual measurement takes place */
#ifndef MQ7_CONF_PREHEAT_STEPS
	#define MQ7_PREHEAT_STEPS 2
#else
	#define MQ7_PREHEAT_STEPS MQ7_CONF_PREHEAT_STEPS
#endif
/*------------------------------------------------------------------*/
/* PPM/PPB REFERENCE VALUES */
//reference datasheet values for MQ7 PPM
#define MQ7_RESRATIO_MIN            0.02
#define MQ7_RESRATIO_MAX            0.25
#define MQ7_RESRATIO_BOUNDARY_1     MQ7_RESRATIO_MAX
#define MQ7_RESRATIO_BOUNDARY_2     0.065
#define MQ7_RESRATIO_BOUNDARY_3     0.03
#define MQ7_RESRATIO_BOUNDARY_4     MQ7_RESRATIO_MIN
#define MQ7_PPM_MIN                 10.0
#define MQ7_PPM_MAX                 1000.0
#define MQ7_PPM_BOUNDARY_1          MQ7_PPM_MIN
#define MQ7_PPM_BOUNDARY_2          100.0
#define MQ7_PPM_BOUNDARY_3          400.0
#define MQ7_PPM_BOUNDARY_4          MQ7_PPM_MAX
//reference datasheet values for MQ131 PPM
#define MQ131_RESRATIO_MIN          0.3
#define MQ131_RESRATIO_MAX          7.0
#define MQ131_RESRATIO_BOUNDARY_1   MQ131_RESRATIO_MAX
#define MQ131_RESRATIO_BOUNDARY_2   3.0
#define MQ131_RESRATIO_BOUNDARY_3   1.5
#define MQ131_RESRATIO_BOUNDARY_4   0.7
#define MQ131_RESRATIO_BOUNDARY_5   MQ131_RESRATIO_MIN
#define MQ131_PPM_MIN               5.0
#define MQ131_PPM_MAX               100.0
#define MQ131_PPM_BOUNDARY_1        MQ131_PPM_MIN
#define MQ131_PPM_BOUNDARY_2        10.0
#define MQ131_PPM_BOUNDARY_3        20.0
#define MQ131_PPM_BOUNDARY_4        50.0
#define MQ131_PPM_BOUNDARY_5        MQ131_PPM_MAX
//reference datasheet values for MQ135 PPM
#define MQ135_RESRATIO_MIN          0.8
#define MQ135_RESRATIO_MAX          2.5
#define MQ135_RESRATIO_BOUNDARY_1   MQ135_RESRATIO_MAX
#define MQ135_RESRATIO_BOUNDARY_2   1.1
#define MQ135_RESRATIO_BOUNDARY_3   MQ135_RESRATIO_MIN
#define MQ135_PPM_MIN               10.0
#define MQ135_PPM_MAX               200.0
#define MQ135_PPM_BOUNDARY_1        MQ135_PPM_MIN
#define MQ135_PPM_BOUNDARY_2        100.0
#define MQ135_PPM_BOUNDARY_3        MQ135_PPM_MAX
/*------------------------------------------------------------------*/
//Default Pin Configuration
#ifdef MQ7_SENSOR_CONF_CTRL_PIN
	#define MQ7_SENSOR_CTRL_PIN MQ7_SENSOR_CONF_CTRL_PIN
#else 
	#define MQ7_SENSOR_CTRL_PIN ADC_SENSORS_ADC1_PIN //PA5
#endif
/*------------------------------------------------------------------*/
#ifdef MQ131_SENSOR_CONF_CTRL_PIN
	#define MQ131_SENSOR_CTRL_PIN MQ131_SENSOR_CONF_CTRL_PIN
#else
	#define MQ131_SENSOR_CTRL_PIN ADC_SENSORS_ADC2_PIN //PA4
#endif
/*------------------------------------------------------------------*/
#ifdef MQ135_SENSOR_CONF_CTRL_PIN
	#define MQ135_SENSOR_CTRL_PIN MQ135_SENSOR_CONF_CTRL_PIN
#else 
	#define MQ135_SENSOR_CTRL_PIN ADC_SENSORS_ADC3_PIN //PA2
#endif
/*------------------------------------------------------------------*/
#ifdef MQ7_SENSOR_HEATING_CONF_PIN
	#define MQ7_SENSOR_HEATING_PIN MQ7_SENSOR_HEATING_CONF_PIN
#else
	#define MQ7_SENSOR_HEATING_PIN 0 //PC0
#endif
#ifdef MQ7_SENSOR_HEATING_CONF_PORT
	#define MQ7_SENSOR_HEATING_PORT MQ7_SENSOR_HEATING_CONF_PORT
#else
	#define MQ7_SENSOR_HEATING_PORT GPIO_C_NUM //PC0
#endif
/*------------------------------------------------------------------*/
extern const struct sensors_sensor aqs_sensor;
#define AQS_SENSOR "Air Quality Sensor"
/*------------------------------------------------------------------*/
#endif /* #ifndef AQS_H_  */
