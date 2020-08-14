/*
Acknowledgment

Thanks to Ingo Lohs for providing a brief but descriptive guide in using MQ7 sensor. 
(Originally Arduino code, adapted to Contiki code)
Refer to https://www.hackster.io/ingo-lohs/gas-sensor-carbon-monoxide-mq-7-aka-flying-fish-e58457

Thanks to Dmitry Dziuba for providing the source code to make MQ7 more accurate
Refer to https://github.com/ultimaterobotics/mq7-co-monitor/blob/master/co_monitor.ino
*/

/*------------------------------------------------------------------*/
#ifndef AQS_H_
#define AQS_H_
/*------------------------------------------------------------------*/
//include files
#include "contiki.h"
#include "lib/sensors.h"

#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#include "dev/adc-zoul.h"
#else
#include "dev/adc128s022.h"
#endif

#include "dev/gpio.h"
/*------------------------------------------------------------------*/
//Available sensors to use
#define MQ7_SENSOR                 0x00 //CO
#define MQ131_SENSOR               0x01 //O
#define MQ135_SENSOR               0x02 //CO2
#define MICS4514_SENSOR            0x03 //configuration only
//Used in value function for specific element to measure
#define MICS4514_SENSOR_NOX        0x03 //NOX
#define MICS4514_SENSOR_RED        0x04 //CO
/*------------------------------------------------------------------*/
// FOR CALIBRATION USE
// (use this with value function to get base resistance of sensor)
// actual resistance of sensor/ base resistance for calculation of ppm
#define MQ7_SENSOR_RO              0xA0
#define MQ131_SENSOR_RO            0xA1
#define MQ135_SENSOR_RO            0xA2
#define MICS4514_SENSOR_NOX_RO     0xA3
#define MICS4514_SENSOR_RED_RO     0xA4
/*------------------------------------------------------------------*/
//return codes
#define AQS_ERROR                  (-1)
#define AQS_SUCCESS                0x00
#define AQS_INITIALIZING           AQS_INIT_PHASE
/*------------------------------------------------------------------*/
//sensor states
#define AQS_DISABLED               0x00
#define AQS_ENABLED                0x01
#define AQS_READY                  AQS_ENABLED
#define AQS_INIT_PHASE             0x0F
#define AQS_BUSY                   0xFF
/*------------------------------------------------------------------*/
//configure options
#define AQS_ENABLE                 SENSORS_ACTIVE
#define AQS_DISABLE                0x00
/*------------------------------------------------------------------*/
// Timings
/* heating time in seconds */
#define MQ7_HEATING_HIGH_TIME           60
#define MQ7_HEATING_LOW_TIME            90
#define MQ131_HEATING_TIME              100
#define MQ135_HEATING_TIME              100
#define MQ131_MEASUREMENT_INTERVAL      60
#define MQ135_MEASUREMENT_INTERVAL      60
#define MICS4514_MEASUREMENT_INTERVAL   60
#define MICS4514_HEATING_TIME           60
/*------------------------------------------------------------------*/
// Resistance ratio boundaries (precision in 3 digits)
//  used to limit possible values of the sensors
#define MQ7_RESRATIO_MIN                90
#define MQ7_RESRATIO_MAX                1750
#define MQ131_RESRATIO_MIN              1250
#define MQ131_RESRATIO_MAX              8000
#define MQ135_RESRATIO_MIN              800
#define MQ135_RESRATIO_MAX              2500
#define MICS4514_RED_RESRATIO_MIN       10
#define MICS4514_RED_RESRATIO_MAX       3500
#define MICS4514_NOX_RESRATIO_MIN       65
#define MICS4514_NOX_RESRATIO_MAX       40000
/*------------------------------------------------------------------*/
// Temperature compensation boundaries
// MQ7
#define MQ7_TEMP_DEP_MIN               -10
#define MQ7_TEMP_DEP_MAX                50
#define MQ7_RH_DEP_CURVE_1              33
#define MQ7_RH_DEP_CURVE_2              88
#define MQ7_RESRATIO_DEP_1_MAX          1450
#define MQ7_RESRATIO_DEP_1_MIN          875
#define MQ7_RESRATIO_DEP_2_MAX          1175
#define MQ7_RESRATIO_DEP_2_MIN          725
// MQ131
#define MQ131_TEMP_DEP_MIN             -10
#define MQ131_TEMP_DEP_MAX              50
#define MQ131_RH_DEP_CURVE_1            30
#define MQ131_RH_DEP_CURVE_2            60
#define MQ131_RH_DEP_CURVE_3            85
#define MQ131_RESRATIO_DEP_1_MAX        1700
#define MQ131_RESRATIO_DEP_1_MIN        890
#define MQ131_RESRATIO_DEP_2_MAX        1450
#define MQ131_RESRATIO_DEP_2_MIN        750
#define MQ131_RESRATIO_DEP_3_MAX        1275
#define MQ131_RESRATIO_DEP_3_MIN        650
// MQ135
#define MQ135_TEMP_DEP_MIN             -10
#define MQ135_TEMP_DEP_MAX              50
#define MQ135_RH_DEP_CURVE_1            33
#define MQ135_RH_DEP_CURVE_2            85
#define MQ135_RESRATIO_DEP_1_MAX        1700
#define MQ135_RESRATIO_DEP_1_MIN        900
#define MQ135_RESRATIO_DEP_2_MAX        1550
#define MQ135_RESRATIO_DEP_2_MIN        800
/*------------------------------------------------- -----------------*/
// Reference resistances (load resistance RL)
//  used for computing RS
#ifndef MQ7_CONF_RL_KOHM
	#define MQ7_RL_KOHM 1
#else
	#define MQ7_RL_KOHM MQ7_CONF_RL_KOHM
#endif
#ifndef MQ131_CONF_RL_KOHM
	#define MQ131_RL_KOHM 1000
#else
	#define MQ131_RL_KOHM MQ131_CONF_RL_KOHM
#endif
#ifndef MQ135_CONF_RL_KOHM
	#define MQ135_RL_KOHM 1
#else
	#define MQ135_RL_KOHM MQ135_CONF_RL_KOHM
#endif
#ifndef MICS4514_RED_CONF_RL_KOHM
	#define MICS4514_RED_RL_KOHM 47
#else
	#define MICS4514_RED_RL_KOHM MQ135_RED_CONF_RL_KOHM
#endif
#ifndef MICS4514_NOX_CONF_RL_KOHM
	#define MICS4514_NOX_RL_KOHM 22
#else
	#define MICS4514_NOX_RL_KOHM MQ135_NOX_CONF_RL_KOHM
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
//(in ohm) with reference to 23±5 deg. C and 50±10% RH in clean air
#ifndef MICS4514_RED_CONF_RO_CLEAN_AIR
	#define MICS4514_RED_RO_CLEAN_AIR 0
#else
	#define MICS4514_RED_RO_CLEAN_AIR MICS4514_RED_CONF_RO_CLEAN_AIR
#endif
//(in ohm) with reference to 23±5 deg. C and <5% RH in clean air
#ifndef MICS4514_NOX_CONF_RO_CLEAN_AIR
	#define MICS4514_NOX_RO_CLEAN_AIR 0
#else
	#define MICS4514_NOX_RO_CLEAN_AIR MICS4514_NOX_CONF_RO_CLEAN_AIR
#endif
/* number of heating cycles before actual measurement takes place */
#ifndef MQ7_CONF_PREHEAT_STEPS
	#define MQ7_PREHEAT_STEPS 2
#else
	#define MQ7_PREHEAT_STEPS MQ7_CONF_PREHEAT_STEPS
#endif
#ifndef MICS4514_CONF_PREHEAT_STEPS
	#define MICS4514_PREHEAT_STEPS 2
#else
	#define MICS4514_PREHEAT_STEPS MICS4514_CONF_PREHEAT_STEPS
#endif
/*------------------------------------------------------------------*/
#ifdef MQ7_SENSOR_HEATING_CONF_PIN
	#define MQ7_SENSOR_HEATING_PIN MQ7_SENSOR_HEATING_CONF_PIN
#else
	#define MQ7_SENSOR_HEATING_PIN 0 //PC0
#endif
/*------------------------------------------------------------------*/
#ifdef MQ7_SENSOR_HEATING_CONF_PORT
	#define MQ7_SENSOR_HEATING_PORT MQ7_SENSOR_HEATING_CONF_PORT
#else
	#define MQ7_SENSOR_HEATING_PORT GPIO_C_NUM //PC0
#endif
/*------------------------------------------------------------------*/
#ifdef MICS4514_SENSOR_HEATING_CONF_PIN
	#define MICS4514_SENSOR_HEATING_PIN MICS4514_SENSOR_HEATING_CONF_PIN
#else
	#define MICS4514_SENSOR_HEATING_PIN 0 //PC0
#endif
/*------------------------------------------------------------------*/
#ifdef MICS4514_SENSOR_HEATING_CONF_PORT
	#define MICS4514_SENSOR_HEATING_PORT MICS4514_SENSOR_HEATING_CONF_PORT
#else
	#define MICS4514_SENSOR_HEATING_PORT GPIO_C_NUM //PC0
#endif
/*------------------------------------------------------------------*/
/* External ADC definitions */
/*------------------------------------------------------------------*/
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
/*------------------------------------------------------------------*/
#ifdef MQ7_SENSOR_CONF_EXT_ADC_CHANNEL
#define MQ7_SENSOR_EXT_ADC_CHANNEL                MQ7_SENSOR_CONF_EXT_ADC_CHANNEL
#else
#define MQ7_SENSOR_EXT_ADC_CHANNEL                0
#endif
/*------------------------------------------------------------------*/
#ifdef MQ131_SENSOR_CONF_EXT_ADC_CHANNEL
#define MQ131_SENSOR_EXT_ADC_CHANNEL              MQ131_SENSOR_CONF_EXT_ADC_CHANNEL
#else
#define MQ131_SENSOR_EXT_ADC_CHANNEL              1
#endif
/*------------------------------------------------------------------*/
#ifdef MQ135_SENSOR_CONF_EXT_ADC_CHANNEL
#define MQ135_SENSOR_EXT_ADC_CHANNEL              MQ135_SENSOR_CONF_EXT_ADC_CHANNEL
#else
#define MQ135_SENSOR_EXT_ADC_CHANNEL              2
#endif
/*------------------------------------------------------------------*/
#ifdef MICS4514_SENSOR_RED_CONF_EXT_ADC_CHANNEL
#define MICS4514_SENSOR_RED_EXT_ADC_CHANNEL       MICS4514_SENSOR_RED_CONF_EXT_ADC_CHANNEL
#else
#define MICS4514_SENSOR_RED_EXT_ADC_CHANNEL       3
#endif
/*------------------------------------------------------------------*/
#ifdef MICS4514_SENSOR_NOX_CONF_EXT_ADC_CHANNEL
#define MICS4514_SENSOR_NOX_EXT_ADC_CHANNEL       MICS4514_SENSOR_NOX_CONF_EXT_ADC_CHANNEL
#else
#define MICS4514_SENSOR_NOX_EXT_ADC_CHANNEL       4
#endif
/*------------------------------------------------------------------*/
/* Internal ADC definitions */
/*------------------------------------------------------------------*/
#else
/*------------------------------------------------------------------*/
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
#ifdef MICS4514_SENSOR_RED_CONF_CTRL_PIN
	#define MICS4514_SENSOR_RED_CTRL_PIN MICS4514_SENSOR_RED_CONF_CTRL_PIN
#else
	#define MICS4514_SENSOR_RED_CTRL_PIN ADC_SENSORS_ADC1_PIN //PA5
#endif
/*------------------------------------------------------------------*/
#ifdef MICS4514_SENSOR_NOX_CONF_CTRL_PIN
	#define MICS4514_SENSOR_NOX_CTRL_PIN MICS4514_SENSOR_NOX_CONF_CTRL_PIN
#else
	#define MICS4514_SENSOR_NOX_CTRL_PIN ADC_SENSORS_ADC3_PIN //PA2
#endif
/*------------------------------------------------------------------*/
#endif /* if ADC_SENSORS_CONF_USE_EXTERNAL_ADC */
/*------------------------------------------------------------------*/
/*------------------------------------------------------------------*/
extern const struct sensors_sensor aqs_sensor;
int64_t aqs_value(int type);
extern int16_t aqs_temperature;
extern uint8_t aqs_humidity;
#define AQS_SENSOR "Air Quality Sensor"
/*------------------------------------------------------------------*/
#endif /* #ifndef AQS_H_  */
