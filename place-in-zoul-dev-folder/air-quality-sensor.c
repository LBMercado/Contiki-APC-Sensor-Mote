//include files
#include "dev/air-quality-sensor.h"
#include "sys/etimer.h"
#include "dev/zoul-sensors.h"
#include "dev/gpio.h"
#include "dev/ioc.h"
#include <stdio.h>
#include <limits.h>
/*------------------------------------------------------------------*/
#if !ADC_SENSORS_CONF_USE_EXTERNAL_ADC
#define MQ7_SENSOR_PIN_MASK                      GPIO_PIN_MASK(MQ7_SENSOR_CTRL_PIN)
#define MQ131_SENSOR_PIN_MASK                    GPIO_PIN_MASK(MQ131_SENSOR_CTRL_PIN)
#define MQ135_SENSOR_PIN_MASK                    GPIO_PIN_MASK(MQ135_SENSOR_CTRL_PIN)
#define MICS4514_SENSOR_RED_PIN_MASK             GPIO_PIN_MASK(MICS4514_SENSOR_RED_CTRL_PIN)
#define MICS4514_SENSOR_NOX_PIN_MASK             GPIO_PIN_MASK(MICS4514_SENSOR_NOX_CTRL_PIN)
#endif
#define MQ7_SENSOR_HEATING_PIN_MASK              GPIO_PIN_MASK(MQ7_SENSOR_HEATING_PIN)
#define MQ7_SENSOR_HEATING_PORT_BASE             GPIO_PORT_TO_BASE(MQ7_SENSOR_HEATING_PORT)
#define MICS4514_SENSOR_HEATING_PIN_MASK         GPIO_PIN_MASK(MICS4514_SENSOR_HEATING_PIN)
#define MICS4514_SENSOR_HEATING_PORT_BASE        GPIO_PORT_TO_BASE(MICS4514_SENSOR_HEATING_PORT)
/*------------------------------------------------------------------*/
//values for calibration
#define CALIBRATION_SAMPLE_COUNT 100 //number of samples before finalizing calibration values
/*------------------------------------------------------------------*/
// tolerance in 3 decimal digit precision
#define RESRATIO_TOLERANCE       1
/*------------------------------------------------------------------*/
/*Scaling Factors*/
//scaling factors for resistances
#define OHM_SCALETO_MILLIOHM     1000
#define KOHM_SCALETO_MILLIOHM    1000000
/*------------------------------------------------------------------*/
#define AQS_SUPPORTED_SENSOR_COUNT  5
/*------------------------------------------------------------------*/
#define AQS_ADC_REF       50000  // in mV, sf. 1 dec. digit
#define AQS_ADC_CROSSREF  33000  // in mV, sf. 1 dec. digit
/*------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif
/*------------------------------------------------------------------*/
PROCESS(aqs_mq7_calibration_process,"AQS Calibration (MQ7) Process Handler");
PROCESS(aqs_mq131_calibration_process,"AQS Calibration (MQ131) Process Handler");
PROCESS(aqs_mq135_calibration_process,"AQS Calibration (MQ135) Process Handler");
PROCESS(aqs_mics4514_calibration_process, "AQS Calibration (MICS-4514) Process Handler");
PROCESS(aqs_mq7_measurement_process, "AQS Measurement (MQ7) Process Handler");
PROCESS(aqs_mq131_measurement_process, "AQS Measurement (MQ131) Process Handler");
PROCESS(aqs_mq135_measurement_process, "AQS Measurement (MQ135) Process Handler");
PROCESS(aqs_mics4514_measurement_process, "AQS Measurement (MICS-4514) Process Handler");
/*------------------------------------------------------------------*/
static process_event_t calibration_finished_event;
/*------------------------------------------------------------------*/
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
typedef struct{
	uint8_t sensor_type;
	uint8_t measure_channel;
} measure_aqs_data_t;
#else
typedef struct{
	uint8_t sensor_type;
	uint32_t measure_pin_mask;
} measure_aqs_data_t;
#endif
typedef struct {
  uint8_t state; //sensor status
  uint64_t value; //resratio equivalent of sensor output, precision in 3 decimal digits
  uint32_t ro; //(in milliohms), sensor resistance in clean air
} aqs_info_t;
static aqs_info_t aqs_info[AQS_SUPPORTED_SENSOR_COUNT];
static uint8_t event_allocated = 0;
/*------------------------------------------------------------------*/
// temperature and humidity compensation values
int16_t aqs_temperature = -32767; //initial value, reject until set to valid, precision in 1st decimal digit
uint8_t aqs_humidity = 255; //initial value, reject until set to valid
//   MQ7 constants
static const int8_t MQ7_TEMP_DEP_BOUNDARIES[] =
{
	MQ7_TEMP_DEP_MIN,
	5,
	20,
	MQ7_TEMP_DEP_MAX
};
static const int16_t MQ7_RESRATIO_DEP_BOUNDARIES_1[] =
{
	MQ7_RESRATIO_DEP_1_MAX,
	1125,
	1000,
	MQ7_RESRATIO_DEP_1_MIN
};
static const int16_t MQ7_RESRATIO_DEP_BOUNDARIES_2[] =
{
	MQ7_RESRATIO_DEP_2_MAX,
	975,
	850,
	MQ7_RESRATIO_DEP_2_MIN
};
static const uint8_t MQ7_DEP_BOUNDARIES_SIZE = 4;
//   MQ131 constants
static const int8_t MQ131_TEMP_DEP_BOUNDARIES[] =
{
	MQ131_TEMP_DEP_MIN,
	-5,
	0,
	5,
	10,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	MQ131_TEMP_DEP_MAX
};
static const int16_t MQ131_RESRATIO_DEP_BOUNDARIES_1[] =
{
	MQ131_RESRATIO_DEP_1_MAX,
	1650,
	1600,
	1500,
	1425,
	1300,
	1250,
	1175,
	1150,
	1125,
	1000,
	925,
	MQ131_RESRATIO_DEP_1_MIN
};
static const int16_t MQ131_RESRATIO_DEP_BOUNDARIES_2[] =
{
	MQ131_RESRATIO_DEP_2_MAX,
	1375,
	1350,
	1275,
	1200,
	1110,
	1075,
	1000,
	975,
	950,
	875,
	800,
	MQ131_RESRATIO_DEP_2_MIN
};
static const int16_t MQ131_RESRATIO_DEP_BOUNDARIES_3[] =
{
	MQ131_RESRATIO_DEP_2_MAX,
	1200,
	1175,
	1100,
	1075,
	975,
	925,
	875,
	850,
	825,
	725,
	675,
	MQ131_RESRATIO_DEP_2_MIN
};
static uint8_t MQ131_DEP_BOUNDARIES_SIZE = 13;
//   MQ135 constants
static const int8_t MQ135_TEMP_DEP_BOUNDARIES[] =
{
		MQ135_TEMP_DEP_MIN,
		5,
		20,
		MQ135_TEMP_DEP_MAX
};
static const int16_t MQ135_RESRATIO_DEP_BOUNDARIES_1[] =
{
		MQ135_RESRATIO_DEP_1_MAX,
		1225,
		1000,
		MQ135_RESRATIO_DEP_1_MIN
};
static const int16_t MQ135_RESRATIO_DEP_BOUNDARIES_2[] =
{
		MQ135_RESRATIO_DEP_2_MAX,
		1175,
		925,
		MQ135_RESRATIO_DEP_2_MIN
};
static const uint8_t MQ135_DEP_BOUNDARIES_SIZE = 4;
/*------------------------------------------------------------------*/
static void
allocate_calibrate_event()
{
	if ( !event_allocated ){
		PRINTF("Allocating event for calibration processes.\n");
		//allocate an event for when calibration is finished
		calibration_finished_event = process_alloc_event();
		event_allocated = 1;
	}
}
/*------------------------------------------------------------------*/
static uint32_t get_linear_function_value(int16_t y_comp_0, int16_t y_comp_1, int16_t x_comp_0, int16_t x_comp_1, float x_value)
{
	return (uint32_t)(y_comp_0 + (( y_comp_1 -  y_comp_0) / (float)( x_comp_1 - x_comp_0 )) * ( x_value - x_comp_0 ));
}
/*------------------------------------------------------------------*/
/* Compensates for environment temperature (temp) and humidity (hum)
 * by multiplying a factor to the computed resistance ratio (res_ratio)
 * Note: res_ratio values that lie outside the maxima and minima are extrapolated
 * @param: temp = temperature in current environment, precision in 1st decimal digit in fixed-point
 * @param: humidity = relative humidity in current environment
 * @param res_ratio: computed resistance ratio
 * @returns: temperature-compensated resistance ratio
 * */
static uint32_t environment_compensate(int16_t temp, uint8_t hum, uint32_t res_ratio, const uint8_t type)
{
	float true_temp = temp / 10.0;
	uint8_t max_idx;
	switch(type){
		case MQ7_SENSOR:
			max_idx = MQ7_DEP_BOUNDARIES_SIZE - 1;
			// make sure that temperature falls within boundary
			if (true_temp < MQ7_TEMP_DEP_MIN || true_temp > MQ7_TEMP_DEP_MAX) {
				PRINTF("environment_compensate: WARNING - temperature falls outside boundaries. Return uncompensated value.\n");
				return res_ratio;
			}
			// falls below 2nd curve
			if ( hum > (MQ7_RH_DEP_CURVE_1 + MQ7_RH_DEP_CURVE_2) / 2) {
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ7_RESRATIO_DEP_BOUNDARIES_2[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_2[max_idx - 1],
							MQ7_RESRATIO_DEP_BOUNDARIES_2[max_idx],
							MQ7_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ7_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ7_RESRATIO_DEP_BOUNDARIES_2[idx])
						return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_2[idx],
								MQ7_RESRATIO_DEP_BOUNDARIES_2[idx + 1],
								MQ7_TEMP_DEP_BOUNDARIES[idx],
								MQ7_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_2[0],
						MQ7_RESRATIO_DEP_BOUNDARIES_2[1],
						MQ7_TEMP_DEP_BOUNDARIES[0],
						MQ7_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;

			} // falls below 1st curve
			else {
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ7_RESRATIO_DEP_BOUNDARIES_1[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_1[max_idx - 1],
							MQ7_RESRATIO_DEP_BOUNDARIES_1[max_idx],
							MQ7_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ7_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ7_RESRATIO_DEP_BOUNDARIES_1[idx])
						return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_1[idx],
								MQ7_RESRATIO_DEP_BOUNDARIES_1[idx + 1],
								MQ7_TEMP_DEP_BOUNDARIES[idx],
								MQ7_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ7_RESRATIO_DEP_BOUNDARIES_1[0],
						MQ7_RESRATIO_DEP_BOUNDARIES_1[1],
						MQ7_TEMP_DEP_BOUNDARIES[0],
						MQ7_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;
			}
			break;
		case MQ131_SENSOR:
			max_idx = MQ131_DEP_BOUNDARIES_SIZE - 1;
			// make sure that temperature falls within boundary
			if (true_temp < MQ131_TEMP_DEP_MIN || true_temp > MQ131_TEMP_DEP_MAX) {
				PRINTF("environment_compensate: WARNING - temperature falls outside boundaries. Return uncompensated value.\n");
				return res_ratio;
			}
			// falls in 1st curve
			if ( hum <= (MQ131_RH_DEP_CURVE_1 + MQ131_RH_DEP_CURVE_2) / 2) {
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_1[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_1[max_idx - 1],
							MQ131_RESRATIO_DEP_BOUNDARIES_1[max_idx],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_1[idx])
						return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_1[idx],
								MQ131_RESRATIO_DEP_BOUNDARIES_1[idx + 1],
								MQ131_TEMP_DEP_BOUNDARIES[idx],
								MQ131_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_1[0],
						MQ131_RESRATIO_DEP_BOUNDARIES_1[1],
						MQ131_TEMP_DEP_BOUNDARIES[0],
						MQ131_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;

			} // falls in 2nd curve
			else if (hum < (MQ131_RH_DEP_CURVE_2 + MQ131_RH_DEP_CURVE_3) / 2) {
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_2[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_2[max_idx - 1],
							MQ131_RESRATIO_DEP_BOUNDARIES_2[max_idx],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_2[idx])
						return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_2[idx],
								MQ131_RESRATIO_DEP_BOUNDARIES_2[idx + 1],
								MQ131_TEMP_DEP_BOUNDARIES[idx],
								MQ131_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_2[0],
						MQ131_RESRATIO_DEP_BOUNDARIES_2[1],
						MQ131_TEMP_DEP_BOUNDARIES[0],
						MQ131_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;
			}
			else { // falls in 3rd curve
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_3[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_3[max_idx - 1],
							MQ131_RESRATIO_DEP_BOUNDARIES_3[max_idx],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ131_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ131_RESRATIO_DEP_BOUNDARIES_3[idx])
						return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_3[idx],
								MQ131_RESRATIO_DEP_BOUNDARIES_3[idx + 1],
								MQ131_TEMP_DEP_BOUNDARIES[idx],
								MQ131_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ131_RESRATIO_DEP_BOUNDARIES_3[0],
						MQ131_RESRATIO_DEP_BOUNDARIES_3[1],
						MQ131_TEMP_DEP_BOUNDARIES[0],
						MQ131_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;
			}
			break;
		case MQ135_SENSOR:
			max_idx = MQ135_DEP_BOUNDARIES_SIZE - 1;
			// make sure that temperature falls within boundary
			if (true_temp < MQ135_TEMP_DEP_MIN || true_temp > MQ135_TEMP_DEP_MAX) {
				PRINTF("environment_compensate: WARNING - temperature falls outside boundaries. Return uncompensated value.\n");
				return res_ratio;
			}
			// falls in 1st curve
			if ( hum <= (MQ135_RH_DEP_CURVE_1 + MQ135_RH_DEP_CURVE_2) / 2) {
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ135_RESRATIO_DEP_BOUNDARIES_1[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_1[max_idx - 1],
							MQ135_RESRATIO_DEP_BOUNDARIES_1[max_idx],
							MQ135_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ135_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ135_RESRATIO_DEP_BOUNDARIES_1[idx])
						return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_1[idx],
								MQ135_RESRATIO_DEP_BOUNDARIES_1[idx + 1],
								MQ135_TEMP_DEP_BOUNDARIES[idx],
								MQ135_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_1[0],
						MQ135_RESRATIO_DEP_BOUNDARIES_1[1],
						MQ135_TEMP_DEP_BOUNDARIES[0],
						MQ135_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;
			}
			else { // falls in 2nd curve
				// case 1: falls below minimum boundary, extrapolate
				if (res_ratio - RESRATIO_TOLERANCE <= MQ135_RESRATIO_DEP_BOUNDARIES_2[max_idx]) {
					PRINTF("environment_compensate: WARNING - resratio falls below boundaries (type: 0x%x).\n", type);
					return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_2[max_idx - 1],
							MQ135_RESRATIO_DEP_BOUNDARIES_2[max_idx],
							MQ135_TEMP_DEP_BOUNDARIES[max_idx - 1],
							MQ135_TEMP_DEP_BOUNDARIES[max_idx],
							true_temp) * res_ratio / 1000;
				}

				// case 2: falls within boundaries
				for (uint8_t idx = max_idx - 1; idx >= 0; idx--){
					if (res_ratio - RESRATIO_TOLERANCE <= MQ135_RESRATIO_DEP_BOUNDARIES_2[idx])
						return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_2[idx],
								MQ135_RESRATIO_DEP_BOUNDARIES_2[idx + 1],
								MQ135_TEMP_DEP_BOUNDARIES[idx],
								MQ135_TEMP_DEP_BOUNDARIES[idx + 1],
								true_temp) * res_ratio / 1000;
				}

				// case 3: falls above maximum boundary, extrapolate
				PRINTF("environment_compensate: WARNING - resratio falls above boundaries (type: 0x%x).\n", type);
				return get_linear_function_value(MQ135_RESRATIO_DEP_BOUNDARIES_2[0],
						MQ135_RESRATIO_DEP_BOUNDARIES_2[1],
						MQ135_TEMP_DEP_BOUNDARIES[0],
						MQ135_TEMP_DEP_BOUNDARIES[1],
						true_temp) * res_ratio / 1000;
			}
			break;
		default:
			PRINTF("environment_compensate: ERROR - invalid type specified. Return uncompensated value.\n");
			return res_ratio;
	}
	PRINTF("environment_compensate: ERROR - Unknown error encountered. Return uncompensated value.\n");
	return res_ratio;
}
/*------------------------------------------------------------------*/
/*
*  Converts raw ADC value in millivolts to its corresponding Ro (sensor resistance) resistance based on the sensor type.
*   @param: type = sensor identifier (MQ7_SENSOR,MQ131_SENSOR,MQ135_SENSOR)
*   @param: value = actual voltage at the load resistor s.f. by 1 decimal digit in fixed-point
*   @returns: unsigned int (32 bits long) Ro in milliohms
*   Note: Computations are s.f. by 1 decimal digit, and are normalized back again to integer-precision
*/
static uint32_t
convert_raw_to_sensor_res(const uint8_t type, uint32_t value)
{	
	if (value == 0) {
		PRINTF("Error for AQS: convert_raw_to_sensor_res function parameter \'value\' cannot be zero.\n");
		return 0;
	}

	switch (type)
	{
	case MQ7_SENSOR:
		/* Refer to datasheet
		*
		*	RS = ( (Vc – VRL) / VRL ) * RL
		*	Where:
		*		Vc = source voltage at time of measurement (5000mV or 1400mV, assume 5000mV)
		*		VRL = reference resistor voltage, output of A0 Pin
		*		*RL = reference resistor
		*
		*/
		value = ( ( 50000 - (float)value ) / ( (float)value ) ) * (MQ7_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		return value;
	case MICS4514_SENSOR_NOX:
		/* Sensor forms a voltage divider with load resistor RL (MICS4514)
		*
		*	RS = ( (Vc – VRL) / VRL ) * RL
		*	Where:
		*		Vc = source voltage at time of measurement (5000mV)
		*		VRL = reference resistor voltage, output of A0 Pin
		*		*RL = reference resistor
		*
		*/
		value = ( ( 50000 - (float)value ) / ( (float)value ) ) * (MICS4514_NOX_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		return value;
		break;
	case MICS4514_SENSOR_RED:
		/* Sensor forms a voltage divider with load resistor RL (MICS4514)
		*
		*	RS = ( (Vc – VRL) / VRL ) * RL
		*	Where:
		*		Vc = source voltage at time of measurement (5000mV)
		*		VRL = reference resistor voltage, output of A0 Pin
		*		*RL = reference resistor
		*
		*/
		value = ( ( 50000 - (float)value ) / ( (float)value ) ) * (MICS4514_RED_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		return value;
		break;
	case MQ131_SENSOR:
		/* Sensor forms a voltage divider with load resistor RL
		*	RS = ( (Vc – VRL) / VRL ) * RL
		*	Where:
		*		Vc = source voltage at time of measurement (5000mV)
		*		VRL = reference resistor voltage, output of A0 Pin
		*		*RL = reference resistor
		*
		*/
		value = ( ( 50000 - (float)value ) / ( (float)value ) ) * (MQ131_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		return value;
		break;
	case MQ135_SENSOR:
		/* Refer to datasheet
		*	RS = ( Vc / VRL - 1 ) × RL
		*	Where:
		*		Vc = source voltage at time of measurement (5000mV)
		*		VRL = reference resistor voltage, output of A0 Pin
		*		*RL = reference resistor
		*
		*/
		value = ( ( 5000 / (float)value / 10 ) - 1 ) * ( MQ135_RL_KOHM * KOHM_SCALETO_MILLIOHM );
		return value;
		break;
	default:
		PRINTF("Error for AQS: convert_raw_to_sensor_res function parameter \'type\' is not valid.\n");
		return 0;
	}
}
/*------------------------------------------------------------------*/
static uint32_t
normalize_resratio(const uint32_t resratio, const uint8_t type){
	switch(type){
		case MQ7_SENSOR:
			if (resratio + RESRATIO_TOLERANCE > MQ7_RESRATIO_MAX)
				return MQ7_RESRATIO_MAX;
			else if (resratio - RESRATIO_TOLERANCE < MQ7_RESRATIO_MIN)
				return MQ7_RESRATIO_MIN;
			return resratio;
			break;
		case MQ131_SENSOR:
			if (resratio + RESRATIO_TOLERANCE > MQ131_RESRATIO_MAX)
				return MQ131_RESRATIO_MAX;
			else if (resratio - RESRATIO_TOLERANCE < MQ131_RESRATIO_MIN)
				return MQ131_RESRATIO_MIN;
			return resratio;
			break;
		case MQ135_SENSOR:
			if (resratio + RESRATIO_TOLERANCE > MQ135_RESRATIO_MAX)
				return MQ135_RESRATIO_MAX;
			else if (resratio - RESRATIO_TOLERANCE < MQ135_RESRATIO_MIN)
				return MQ135_RESRATIO_MIN;
			return resratio;
			break;
		case MICS4514_SENSOR_RED:
			if (resratio + RESRATIO_TOLERANCE > MICS4514_RED_RESRATIO_MAX)
				return MICS4514_RED_RESRATIO_MAX;
			else if (resratio - RESRATIO_TOLERANCE < MICS4514_RED_RESRATIO_MIN)
				return MICS4514_RED_RESRATIO_MIN;
			return resratio;
			break;
		case MICS4514_SENSOR_NOX:
			if (resratio + RESRATIO_TOLERANCE > MICS4514_NOX_RESRATIO_MAX)
				return MICS4514_NOX_RESRATIO_MAX;
			else if (resratio - RESRATIO_TOLERANCE < MICS4514_NOX_RESRATIO_MIN)
				return MICS4514_NOX_RESRATIO_MIN;
			return resratio;
			break;
		default:
			PRINTF("normalize_resratio: Invalid parameter \"type\" with value %hhu \n", type);
			return resratio;
	}
}
/*------------------------------------------------------------------*/
//this function is used during calibration procedures
static uint32_t
measure_aqs_ro(const void* data_ptr)
{
	measure_aqs_data_t* aqs_info_data;
	uint32_t val;
	
	aqs_info_data = (measure_aqs_data_t*)(data_ptr);
	
	/* make sure it is a valid sensor type */
	if (aqs_info_data->sensor_type != MQ7_SENSOR &&
			aqs_info_data->sensor_type != MQ131_SENSOR &&
			aqs_info_data->sensor_type != MQ135_SENSOR &&
			aqs_info_data->sensor_type != MICS4514_SENSOR_RED &&
			aqs_info_data->sensor_type != MICS4514_SENSOR_NOX){
		PRINTF("measure_aqs_ro: sensor(0x%02x) ERROR - unexpected sensor type.\n",
			aqs_info_data->sensor_type);
		return 0;
	}
	
	if ( aqs_info[aqs_info_data->sensor_type].state != AQS_BUSY &&
		aqs_info[aqs_info_data->sensor_type].state != AQS_ENABLED &&
		aqs_info[aqs_info_data->sensor_type].state != AQS_INIT_PHASE){
		PRINTF("measure_aqs_ro: sensor(0x%02x) is DISABLED.\n",
			aqs_info_data->sensor_type);
		return 0;
	}
	
	aqs_info[aqs_info_data->sensor_type].state = AQS_BUSY;
	PRINTF("measure_aqs_ro: sensor(0x%02x) set to BUSY.\n", aqs_info_data->sensor_type);

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	val = adc128s022.value(aqs_info_data->measure_channel);
	if (val == ADC128S022_ERROR){
		PRINTF("measure_aqs_ro: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensor_type);
		return 0;
	}
	/* 12 ENOBs ADC, output is digitized level of analog signal*/
	PRINTF("measure_aqs_ro: sensor(0x%02x) raw ADC value = %lu\n", aqs_info_data->sensor_type, val);
	/* convert to 5v ref */
	val *= AQS_ADC_REF;
	val /= ADC128S022_ADC_MAX_LEVEL;
#else
	val = adc_zoul.value(aqs_info_data->measure_pin_mask);
	if (val == ZOUL_SENSORS_ERROR){
		PRINTF("measure_aqs_ro: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensor_type);
		return 0;
	}
	PRINTF("measure_aqs_ro: sensor(0x%02x) raw ADC value = %lu\n", aqs_info_data->sensor_type, val);

	/* 512 bit resolution, output is in mV already (significant to the tenth decimal place) */
	/* convert 3v ref to 5v ref */
	val *= AQS_ADC_REF;
	val /= AQS_ADC_CROSSREF;
#endif
	PRINTF("measure_aqs_ro: sensor(0x%02x) mv ADC value = %lu.%lu\n", aqs_info_data->sensor_type, val / 10, val % 10);
	
	val = convert_raw_to_sensor_res(aqs_info_data->sensor_type, val);
	PRINTF("measure_aqs_ro: sensor(0x%02x) sensor resistance (milli) = %lu\n", aqs_info_data->sensor_type, val);
	aqs_info[aqs_info_data->sensor_type].state = AQS_INIT_PHASE;
	PRINTF("measure_aqs_ro: sensor(0x%02x) set to AQS_INIT_PHASE.\n", aqs_info_data->sensor_type);

	return val;
}
/*------------------------------------------------------------------*/
static void 
measure_aqs(const void* data_ptr)
{
	measure_aqs_data_t* aqs_info_data;
	uint64_t val;
	
	aqs_info_data = (measure_aqs_data_t*)(data_ptr);
	
	/* make sure it is a valid sensor type */
	if (aqs_info_data->sensor_type != MQ7_SENSOR &&
			aqs_info_data->sensor_type != MQ131_SENSOR &&
			aqs_info_data->sensor_type != MQ135_SENSOR &&
			aqs_info_data->sensor_type != MQ135_SENSOR &&
			aqs_info_data->sensor_type != MICS4514_SENSOR_RED &&
			aqs_info_data->sensor_type != MICS4514_SENSOR_NOX){
		PRINTF("measure_aqs: sensor(0x%02x) ERROR - unexpected sensor type.",
			aqs_info_data->sensor_type);
		return;
	}
	
	/* make sure Ro is initialized */
	if (aqs_info[aqs_info_data->sensor_type].ro == 0) {
		PRINTF("measure_aqs: sensor(0x%02x) ERROR - Ro is not initialized.",
			aqs_info_data->sensor_type);
		return;
	}

	if ( aqs_info[aqs_info_data->sensor_type].state != AQS_BUSY &&
		aqs_info[aqs_info_data->sensor_type].state != AQS_ENABLED ){
		PRINTF("measure_aqs: sensor(0x%02x) is DISABLED.\n",
			aqs_info_data->sensor_type);
		return;
	}
	
	aqs_info[aqs_info_data->sensor_type].state = AQS_BUSY;
	PRINTF("measure_aqs: sensor(0x%02x) set to BUSY.\n", aqs_info_data->sensor_type);
	
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	val = adc128s022.value(aqs_info_data->measure_channel);
	if (val == ADC128S022_ERROR){
		PRINTF("measure_aqs: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensor_type);
		return;
	}
	/* 12 ENOBs ADC, output is digitized level of analog signal*/
	PRINTF("measure_aqs: sensor(0x%02x) raw ADC value = %llu\n", aqs_info_data->sensor_type, val);
	/* convert to 5v ref */
	val *= AQS_ADC_REF;
	val /= ADC128S022_ADC_MAX_LEVEL;
#else
	val = adc_zoul.value(aqs_info_data->measure_pin_mask);
	if (val == ZOUL_SENSORS_ERROR){
		PRINTF("measure_aqs: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensor_type);
		return;
	}
	
	PRINTF("measure_aqs: sensor(0x%02x) raw ADC value = %llu\n", aqs_info_data->sensor_type, val);
	
	/* 512 bit resolution, output is in mV already (significant to the tenth decimal place) */
	/* convert 3v ref to 5v ref */
	val *= AQS_ADC_REF;
	val /= AQS_ADC_CROSSREF;

	PRINTF("measure_aqs: sensor(0x%02x) mv ADC value = %llu.%llu\n", aqs_info_data->sensor_type, val / 10, val % 10);
#endif
	
	PRINTF("measure_aqs: sensor(0x%02x) reference Ro = %lu.%lu\n",
			aqs_info_data->sensor_type,
			aqs_info[aqs_info_data->sensor_type].ro / 1000,
			aqs_info[aqs_info_data->sensor_type].ro % 1000);

	val = convert_raw_to_sensor_res(aqs_info_data->sensor_type, val);

	PRINTF("measure_aqs: sensor(0x%02x) Rs = %llu.%llu\n",
			aqs_info_data->sensor_type,
			val / 1000,
			val % 1000);
	//get resistance ratio at this time, precision in 3 decimal digits
	aqs_info[aqs_info_data->sensor_type].value = val * 1000;
	PRINTF("measure_aqs: sensor(0x%02x) value = %llu\n",
			aqs_info_data->sensor_type,
			aqs_info[aqs_info_data->sensor_type].value);
	aqs_info[aqs_info_data->sensor_type].value /= aqs_info[aqs_info_data->sensor_type].ro;

	//normalize to expected values
	aqs_info[aqs_info_data->sensor_type].value = normalize_resratio(aqs_info[aqs_info_data->sensor_type].value, aqs_info_data->sensor_type);
	PRINTF("measure_aqs: sensor(0x%02x) Rs/Ro value (unnormalized) = %llu.%03llu\n", aqs_info_data->sensor_type,
			aqs_info[aqs_info_data->sensor_type].value / 1000,
			aqs_info[aqs_info_data->sensor_type].value % 1000);

	//compensate the measured value based on environment temperature/humidity (if properly set)
	if (aqs_temperature != -32767 && aqs_humidity != 255)
		aqs_info[aqs_info_data->sensor_type].value = environment_compensate(aqs_temperature, aqs_humidity, aqs_info[aqs_info_data->sensor_type].value,
				aqs_info_data->sensor_type);

	PRINTF("measure_aqs: sensor(0x%02x) Rs/Ro value (normalized) = %llu.%03llu\n", aqs_info_data->sensor_type,
		aqs_info[aqs_info_data->sensor_type].value / 1000,
		aqs_info[aqs_info_data->sensor_type].value % 1000);
	aqs_info[aqs_info_data->sensor_type].state = AQS_ENABLED;
	PRINTF("measure_aqs: sensor(0x%02x) set to ENABLED.\n", aqs_info_data->sensor_type);
}
/*------------------------------------------------------------------*/
static void
turn_on_heater(const uint32_t port_base, const uint32_t pin_mask, const uint16_t duration, struct etimer* et)
{
	GPIO_SET_PIN(port_base, pin_mask);
	etimer_set(et, CLOCK_SECOND * duration);
}
/*------------------------------------------------------------------*/
static void
turn_off_heater(const uint32_t port_base, const uint32_t pin_mask, const uint16_t duration, struct etimer* et)
{
	GPIO_CLR_PIN(port_base, pin_mask);
	etimer_set(et, CLOCK_SECOND * duration);
}
/*------------------------------------------------------------------*/
static int
map_spec_type_to_generic_type
(uint8_t calibType){
	switch(calibType){
		case MQ7_SENSOR_RO:
			return MQ7_SENSOR;
		case MQ131_SENSOR_RO:
			return MQ131_SENSOR;
		case MQ135_SENSOR_RO:
			return MQ135_SENSOR;
		case MICS4514_SENSOR_RED_RO:
			return MICS4514_SENSOR_RED;
		case MICS4514_SENSOR_NOX_RO:
			return MICS4514_SENSOR_NOX;
		default:
			PRINTF("Error for AQS: map_calib_to_sensor_type: ERROR - invalid sensor type specified.\n");
			return AQS_ERROR;
	}
}
/*------------------------------------------------------------------*/
static int
configure(int type, int value)
{
	if(type != AQS_ENABLE && type != AQS_DISABLE) {
		PRINTF("Error for AQS: configure function parameter \'type\' is not AQS_ENABLE or AQS_DISABLE.\n");
		return AQS_ERROR;
	}

	switch (value)
	{
	case MQ7_SENSOR:
		if (type == AQS_DISABLE){
			if ( process_is_running(&aqs_mq7_calibration_process) )
				process_exit(&aqs_mq7_calibration_process);

			if ( process_is_running(&aqs_mq7_measurement_process) )
				process_exit(&aqs_mq7_measurement_process);

			aqs_info[MQ7_SENSOR].state = AQS_DISABLED;
			aqs_info[MQ7_SENSOR].ro = 0;
			aqs_info[MQ7_SENSOR].value = 0;

			return AQS_SUCCESS;
		}

		//do not re-enable the sensor
		if ( aqs_info[MQ7_SENSOR].state == AQS_ENABLED || aqs_info[MQ7_SENSOR].state == AQS_BUSY ||
			aqs_info[MQ7_SENSOR].state == AQS_INIT_PHASE ) {
			PRINTF("AQS: configure function - MQ7_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		// put your own adc definition here
		if (adc128s022.configure(ADC128S022_INIT, MQ7_SENSOR_EXT_ADC_CHANNEL) == ADC128S022_ERROR) {
			PRINTF("Error for AQS: configure function (MQ7_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#else
		if (adc_zoul.configure(SENSORS_HW_INIT, MQ7_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ7_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#endif
		//take control of the heating pins
		GPIO_SOFTWARE_CONTROL(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);
		ioc_set_over(MQ7_SENSOR_HEATING_PORT, MQ7_SENSOR_HEATING_PIN, IOC_OVERRIDE_DIS);
		GPIO_SET_OUTPUT(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);
		GPIO_CLR_PIN(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);

		//set initial values
		aqs_info[MQ7_SENSOR].ro = MQ7_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ7_SENSOR].state = AQS_INIT_PHASE;
		aqs_info[MQ7_SENSOR].value = 0;
		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ7_SENSOR state changed to AQS_INIT_PHASE, proceeding to heating/calibration.\n");

		//begin the measurement cycling process
		process_start(&aqs_mq7_measurement_process, NULL);

		return AQS_SUCCESS;
		break;
	case MQ131_SENSOR:
		if (type == AQS_DISABLE){
			if ( process_is_running(&aqs_mq131_calibration_process) )
				process_exit(&aqs_mq131_calibration_process);

			if ( process_is_running(&aqs_mq131_measurement_process) )
				process_exit(&aqs_mq131_measurement_process);

			aqs_info[MQ131_SENSOR].state = AQS_DISABLED;
			aqs_info[MQ131_SENSOR].ro = 0;
			aqs_info[MQ131_SENSOR].value = 0;

			return AQS_SUCCESS;
		}

		//do not re-enable the sensor
		if (aqs_info[MQ131_SENSOR].state == AQS_ENABLED || aqs_info[MQ131_SENSOR].state == AQS_BUSY || 
			aqs_info[MQ131_SENSOR].state == AQS_INIT_PHASE) {
			PRINTF("AQS: configure function - MQ131_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		// put your own adc definition here
		if (adc128s022.configure(ADC128S022_INIT, MQ131_SENSOR_EXT_ADC_CHANNEL) == ADC128S022_ERROR) {
			PRINTF("Error for AQS: configure function (MQ131_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#else
		if (adc_zoul.configure(SENSORS_HW_INIT, MQ131_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ131_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#endif

		//set initial values
		aqs_info[MQ131_SENSOR].ro = MQ131_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ131_SENSOR].state = AQS_INIT_PHASE;
		aqs_info[MQ131_SENSOR].value = 0;

		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ131_SENSOR state changed to AQS_INIT_PHASE, proceeding to calibration.\n");

		//begin the measurement cycling process
		process_start(&aqs_mq131_measurement_process, NULL);
				
		return AQS_SUCCESS;
		break;
	case MQ135_SENSOR:
		if (type == AQS_DISABLE){
			if ( process_is_running(&aqs_mq135_calibration_process) )
				process_exit(&aqs_mq135_calibration_process);

			if ( process_is_running(&aqs_mq135_measurement_process) )
				process_exit(&aqs_mq135_measurement_process);

			aqs_info[MQ135_SENSOR].state = AQS_DISABLED;
			aqs_info[MQ135_SENSOR].ro = 0;
			aqs_info[MQ135_SENSOR].value = 0;

			return AQS_SUCCESS;
		}

		//do not re-enable the sensor
		if ( aqs_info[MQ135_SENSOR].state == AQS_ENABLED || aqs_info[MQ135_SENSOR].state == AQS_BUSY ||
			aqs_info[MQ135_SENSOR].state == AQS_INIT_PHASE) {
			PRINTF("AQS: configure function - MQ135_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		// put your own adc definition here
		if (adc128s022.configure(ADC128S022_INIT, MQ135_SENSOR_EXT_ADC_CHANNEL) == ADC128S022_ERROR) {
			PRINTF("Error for AQS: configure function (MQ135_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#else
		if (adc_zoul.configure(SENSORS_HW_INIT, MQ135_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ135_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#endif
		//set initial values
		aqs_info[MQ135_SENSOR].ro = MQ135_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ135_SENSOR].state = AQS_INIT_PHASE;
		aqs_info[MQ131_SENSOR].value = 0;

		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ135_SENSOR state changed to AQS_INIT_PHASE, proceeding to calibration.\n");

		//begin the measurement cycling process
		process_start(&aqs_mq135_measurement_process, NULL);
		
		return AQS_SUCCESS;
		break;
	case MICS4514_SENSOR_RED:
	case MICS4514_SENSOR_NOX:
		if (type == AQS_DISABLE){
			if ( process_is_running(&aqs_mics4514_calibration_process) )
				process_exit(&aqs_mics4514_calibration_process);

			if ( process_is_running(&aqs_mics4514_measurement_process) )
				process_exit(&aqs_mics4514_measurement_process);

			aqs_info[MICS4514_SENSOR_NOX].state = AQS_DISABLED;
			aqs_info[MICS4514_SENSOR_NOX].ro = 0;
			aqs_info[MICS4514_SENSOR_NOX].value = 0;

			aqs_info[MICS4514_SENSOR_RED].state = AQS_DISABLED;
			aqs_info[MICS4514_SENSOR_RED].ro = 0;
			aqs_info[MICS4514_SENSOR_RED].value = 0;

			return AQS_SUCCESS;
		}

		//do not re-enable the sensor
		if ( aqs_info[MICS4514_SENSOR_NOX].state == AQS_ENABLED || aqs_info[MICS4514_SENSOR_NOX].state == AQS_BUSY ||
			aqs_info[MICS4514_SENSOR_NOX].state == AQS_INIT_PHASE || aqs_info[MICS4514_SENSOR_RED].state == AQS_ENABLED ||
			aqs_info[MICS4514_SENSOR_RED].state == AQS_BUSY || aqs_info[MICS4514_SENSOR_RED].state == AQS_INIT_PHASE) {
			PRINTF("AQS: configure function - MICS4514 already enabled.\n");
			return AQS_SUCCESS;
		}


#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
		// put your own adc definition here
		if (adc128s022.configure(ADC128S022_INIT, MICS4514_SENSOR_RED_EXT_ADC_CHANNEL) == ADC128S022_ERROR) {
			PRINTF("Error for AQS: configure function (MICS4514_RED) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
		if (adc128s022.configure(ADC128S022_INIT, MICS4514_SENSOR_NOX_EXT_ADC_CHANNEL) == ADC128S022_ERROR) {
			PRINTF("Error for AQS: configure function (MICS4514_NOX) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#else
		if (adc_zoul.configure(SENSORS_HW_INIT, MICS4514_SENSOR_RED_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MICS4514_RED) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
		if (adc_zoul.configure(SENSORS_HW_INIT, MICS4514_SENSOR_NOX_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MICS4514_NOX) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
#endif

		//take control of the heating pins
		GPIO_SOFTWARE_CONTROL(MICS4514_SENSOR_HEATING_PORT_BASE, MICS4514_SENSOR_HEATING_PIN_MASK);
		ioc_set_over(MICS4514_SENSOR_HEATING_PORT, MICS4514_SENSOR_HEATING_PIN, IOC_OVERRIDE_DIS);
		GPIO_SET_OUTPUT(MICS4514_SENSOR_HEATING_PORT_BASE, MICS4514_SENSOR_HEATING_PIN_MASK);
		GPIO_CLR_PIN(MICS4514_SENSOR_HEATING_PORT_BASE, MICS4514_SENSOR_HEATING_PIN_MASK);

		//set initial values
		aqs_info[MICS4514_SENSOR_NOX].ro = MICS4514_NOX_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MICS4514_SENSOR_NOX].state = AQS_INIT_PHASE;
		aqs_info[MICS4514_SENSOR_NOX].value = 0;

		aqs_info[MICS4514_SENSOR_RED].ro = MICS4514_RED_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MICS4514_SENSOR_RED].state = AQS_INIT_PHASE;
		aqs_info[MICS4514_SENSOR_RED].value = 0;

		allocate_calibrate_event();
		PRINTF("AQS: configure function - MICS4514 state changed to AQS_INIT_PHASE, proceeding to calibration.\n");

		//begin the measurement cycling process
		process_start(&aqs_mics4514_measurement_process, NULL);

		return AQS_SUCCESS;
		break;
	default:
		PRINTF("Error for AQS: configure function parameter \'value\' is not valid.\n");
		return AQS_ERROR;
	}
}
/*------------------------------------------------------------------*/
static int
value(int type)
{
	/* make sure it is a valid sensor type */
	if ( type != MQ7_SENSOR &&
			type != MQ131_SENSOR &&
			type != MQ135_SENSOR &&
			type != MQ7_SENSOR_RO &&
			type != MICS4514_SENSOR_RED &&
			type != MICS4514_SENSOR_NOX &&
			type != MQ131_SENSOR_RO &&
			type != MQ135_SENSOR_RO &&
			type != MICS4514_SENSOR_NOX_RO &&
			type != MICS4514_SENSOR_RED_RO){
		PRINTF("AQS value function: ERROR - unexpected sensor type.\n");
		return AQS_ERROR;
	}
	int sensorType = type;
	/* must map to standard type if specific type */
	if (type == MQ7_SENSOR_RO ||
			type == MQ131_SENSOR_RO ||
			type == MQ135_SENSOR_RO ||
			type == MICS4514_SENSOR_NOX_RO ||
			type == MICS4514_SENSOR_RED_RO){
		sensorType = map_spec_type_to_generic_type(sensorType);
	}

	
	switch(aqs_info[sensorType].state){
		case AQS_DISABLED:
			PRINTF("AQS value function: ERROR - sensor is disabled.\n");
			return AQS_ERROR;
		case AQS_INIT_PHASE:
			PRINTF("AQS value function: ERROR - sensor is initializing.\n");
			return AQS_INITIALIZING;
		case AQS_BUSY:
			PRINTF("AQS value function: WARNING - sensor is busy. Measured value may be obsolete.\n");
		case AQS_ENABLED:
		default:
			if (type == MQ7_SENSOR_RO ||
					type == MQ131_SENSOR_RO ||
					type == MQ135_SENSOR_RO ||
					type == MICS4514_SENSOR_NOX_RO ||
					type == MICS4514_SENSOR_RED_RO)
				return aqs_info[sensorType].ro;
			else
				return aqs_info[sensorType].value;
	}
}
/*------------------------------------------------------------------*/
/*
 * Recommended to use this over the internal value function of the sensor.
 * @param: type = type of sensor to get a reading from
 * @returns: int64_t output value from the sensor
 * */
int64_t
aqs_value(int type)
{
	/* make sure it is a valid sensor type */
	if ( type != MQ7_SENSOR &&
			type != MQ131_SENSOR &&
			type != MQ135_SENSOR &&
			type != MQ7_SENSOR_RO &&
			type != MICS4514_SENSOR_RED &&
			type != MICS4514_SENSOR_NOX &&
			type != MQ131_SENSOR_RO &&
			type != MQ135_SENSOR_RO &&
			type != MICS4514_SENSOR_NOX_RO &&
			type != MICS4514_SENSOR_RED_RO){
		PRINTF("AQS value function: ERROR - unexpected sensor type.\n");
		return AQS_ERROR;
	}
	int sensorType = type;
	/* must map to standard type if specific type */
	if (type == MQ7_SENSOR_RO ||
			type == MQ131_SENSOR_RO ||
			type == MQ135_SENSOR_RO ||
			type == MICS4514_SENSOR_NOX_RO ||
			type == MICS4514_SENSOR_RED_RO){
		sensorType = map_spec_type_to_generic_type(sensorType);
	}


	switch(aqs_info[sensorType].state){
		case AQS_DISABLED:
			PRINTF("AQS value function: ERROR - sensor is disabled.\n");
			return AQS_ERROR;
		case AQS_INIT_PHASE:
			PRINTF("AQS value function: ERROR - sensor is initializing.\n");
			return AQS_INITIALIZING;
		case AQS_BUSY:
			PRINTF("AQS value function: WARNING - sensor is busy. Measured value may be obsolete.\n");
		case AQS_ENABLED:
		default:
			if (type == MQ7_SENSOR_RO ||
					type == MQ131_SENSOR_RO ||
					type == MQ135_SENSOR_RO ||
					type == MICS4514_SENSOR_NOX_RO ||
					type == MICS4514_SENSOR_RED_RO)
				return aqs_info[sensorType].ro;
			else
				return aqs_info[sensorType].value;
	}
}
/*------------------------------------------------------------------*/
static int
status(int type){
	/* make sure it is a valid sensor type */
	if (type != MQ7_SENSOR && 
			type != MQ131_SENSOR &&
			type != MQ135_SENSOR &&
			type != MICS4514_SENSOR_RED &&
			type != MICS4514_SENSOR_NOX){
		PRINTF("AQS status function: ERROR - unexpected sensor type.\n");
		return AQS_ERROR;
	}
	
	return aqs_info[type].state;
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq7_calibration_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t sample_count;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint64_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq7_calibration_process started\n");

	//initialization
	rs = 0;
	measure_aqs_data.sensor_type = MQ7_SENSOR;

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ7_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ7_SENSOR_PIN_MASK;
#endif
	aqs_info[MQ7_SENSOR].state = AQS_INIT_PHASE;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ7_SENSOR].ro > 0){
		PRINTF("AQS(MQ7): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ7_SENSOR].ro);
		aqs_info[MQ7_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ7): aqs calibration process - sensor set to ENABLED.\n");
		PRINTF("aqs_mq7_calibration_process: posted calibration_finished_event to aqs_mq7_measurement_process\n");
		PRINTF("AQS(MQ7): aqs calibration process - exiting process\n");
		process_post(&aqs_mq7_measurement_process, calibration_finished_event, NULL);
		PROCESS_EXIT();
	}
	
	aqs_info[MQ7_SENSOR].ro = 0;
	PRINTF("AQS(MQ7): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	while (sample_count != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ7): aqs calibration process - sampling %hhu out of %d.\n", sample_count + 1, CALIBRATION_SAMPLE_COUNT);
		
		//turn the heater on and take measurement at this phase
		turn_on_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_calibration_process: heater turned on\n");

		rs += measure_aqs_ro( &measure_aqs_data );
		sample_count++;

		//wait for the heater on duration to expire
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		
		//turn off the heater and wait for cooldown period
		turn_off_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_LOW_TIME, &et);
		PRINTF("aqs_mq7_calibration_process: heater turned off\n");
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	}
	
	rs /= sample_count;
	aqs_info[MQ7_SENSOR].ro = rs;
	PRINTF("AQS(MQ7): aqs calibration process - calibration finished w/ values.\n");
	PRINTF("Ro at clean air = %lu\n", aqs_info[MQ7_SENSOR].ro);
	
	aqs_info[MQ7_SENSOR].state = AQS_ENABLED;
	PRINTF("AQS(MQ7): aqs calibration process - sensor set to ENABLED.\n");

	process_post(&aqs_mq7_measurement_process, calibration_finished_event, NULL);
	PRINTF("aqs_mq7_calibration_process: posted calibration_finished_event to aqs_mq7_measurement_process\n");
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq131_calibration_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t sample_count;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint64_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq131_calibration_process started\n");

	//initialization
	rs = 0;
	sample_count = 0;
	measure_aqs_data.sensor_type = MQ131_SENSOR;
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ131_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ131_SENSOR_PIN_MASK;
#endif

	aqs_info[MQ131_SENSOR].state = AQS_INIT_PHASE;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ131_SENSOR].ro > 0){
		PRINTF("AQS(MQ131): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ131_SENSOR].ro);
		aqs_info[MQ131_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ131): aqs calibration process - sensor set to ENABLED.\n");
		PRINTF("aqs_mq131_calibration_process: posted calibration_finished_event to aqs_mq131_measurement_process\n");
		PRINTF("AQS(MQ131): aqs calibration process - exiting process\n");
		process_post(&aqs_mq131_measurement_process, calibration_finished_event, NULL);
		PROCESS_EXIT();
	}
	
	aqs_info[MQ131_SENSOR].ro = 0;
	PRINTF("AQS(MQ131): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	//set a sampling duration
	etimer_set(&et, CLOCK_SECOND * MQ131_MEASUREMENT_INTERVAL);
	while (sample_count != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ131): aqs calibration process - sampling %hhu out of %d.\n", sample_count + 1, CALIBRATION_SAMPLE_COUNT);

		rs += measure_aqs_ro( &measure_aqs_data );
		sample_count++;

		//wait for sampling period to lapse
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		etimer_reset(&et);
	}
	
	rs /= sample_count;

	aqs_info[MQ131_SENSOR].ro = rs;
	PRINTF("AQS(MQ131): aqs calibration process - calibration finished w/ values.\n");
	PRINTF("Ro at clean air = %lu\n", aqs_info[MQ131_SENSOR].ro);
	
	aqs_info[MQ131_SENSOR].state = AQS_ENABLED;
	PRINTF("AQS(MQ131): aqs calibration process - sensor set to ENABLED.\n");

	process_post(&aqs_mq131_measurement_process, calibration_finished_event, NULL);
	PRINTF("aqs_mq131_calibration_process: posted calibration_finished_event to aqs_mq131_measurement_process\n");
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq135_calibration_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t sample_count;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint64_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq135_calibration_process started\n");

	//initialization
	rs = 0;
	sample_count = 0;
	measure_aqs_data.sensor_type = MQ135_SENSOR;
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ135_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ135_SENSOR_PIN_MASK;
#endif

	aqs_info[MQ135_SENSOR].state = AQS_INIT_PHASE;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ135_SENSOR].ro > 0){
		PRINTF("AQS(MQ135): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ135_SENSOR].ro);
		aqs_info[MQ135_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ135): aqs calibration process - sensor set to ENABLED.\n");
		PRINTF("aqs_mq135_calibration_process: posted calibration_finished_event to aqs_mq135_measurement_process\n");
		PRINTF("AQS(MQ135): aqs calibration process - exiting process\n");
		process_post(&aqs_mq135_measurement_process, calibration_finished_event, NULL);
		PROCESS_EXIT();
	}
	
	aqs_info[MQ135_SENSOR].ro = 0;
	PRINTF("AQS(MQ135): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	etimer_set(&et, CLOCK_SECOND * MQ135_MEASUREMENT_INTERVAL);
	while (sample_count != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ135): aqs calibration process - sampling %hhu out of %d.\n", sample_count + 1, CALIBRATION_SAMPLE_COUNT);

		rs += measure_aqs_ro( &measure_aqs_data );
		sample_count++;
		
		//wait for sampling period to lapse
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		etimer_reset(&et);
	}
	
	rs /= sample_count;
	aqs_info[MQ135_SENSOR].ro = rs;
	PRINTF("AQS(MQ135): aqs calibration process - calibration finished w/ values.\n");
	PRINTF("Ro at clean air = %lu\n", aqs_info[MQ135_SENSOR].ro);
	
	aqs_info[MQ135_SENSOR].state = AQS_ENABLED;
	PRINTF("AQS(MQ135): aqs calibration process - sensor set to ENABLED.\n");

	process_post(&aqs_mq135_measurement_process, calibration_finished_event, NULL);
	PRINTF("aqs_mq135_calibration_process: posted calibration_finished_event to aqs_mq135_measurement_process\n");

	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mics4514_calibration_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t sample_count_red;
	static uint8_t sample_count_nox;
	/* Rs = sensor resistance in various conditions (in milliohms)*/
	static uint64_t rs_red;
	static uint64_t rs_nox;
	static measure_aqs_data_t measure_aqs_data_red;
	static measure_aqs_data_t measure_aqs_data_nox;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF(aqs_mics4514_calibration_process.name);
	PRINTF(" started\n");

	//initialization
	rs_red = rs_nox = 0;
	sample_count_red = sample_count_nox = 0;
	measure_aqs_data_red.sensor_type = MICS4514_SENSOR_RED;

	measure_aqs_data_nox.sensor_type = MICS4514_SENSOR_NOX;

#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data_red.measure_channel = MICS4514_SENSOR_RED_EXT_ADC_CHANNEL;
	measure_aqs_data_nox.measure_channel = MICS4514_SENSOR_NOX_EXT_ADC_CHANNEL;
#else
	measure_aqs_data_red.measure_pin_mask = MICS4514_SENSOR_RED_PIN_MASK;
	measure_aqs_data_nox.measure_pin_mask = MICS4514_SENSOR_NOX_PIN_MASK;
#endif
	aqs_info[MICS4514_SENSOR_RED].state = AQS_INIT_PHASE;
	aqs_info[MICS4514_SENSOR_NOX].state = AQS_INIT_PHASE;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MICS4514_SENSOR_RED].ro > 0 && aqs_info[MICS4514_SENSOR_NOX].ro){
		PRINTF(aqs_mics4514_calibration_process.name);
		PRINTF(" - sensor has been calibrated already with default value\n");
		PRINTF("Ro (RED) at clean air = %lu\n", aqs_info[MICS4514_SENSOR_RED].ro);
		PRINTF("Ro (NOX) at clean air = %lu\n", aqs_info[MICS4514_SENSOR_NOX].ro);

		aqs_info[MICS4514_SENSOR_RED].state = AQS_ENABLED;
		aqs_info[MICS4514_SENSOR_NOX].state = AQS_ENABLED;
		PRINTF(aqs_mics4514_calibration_process.name);
		PRINTF(" - sensor set to ENABLED.\n");

		PRINTF(aqs_mics4514_calibration_process.name);
		PRINTF(": posted calibration_finished_event to ");
		PRINTF(aqs_mics4514_measurement_process.name);
		PRINTF("\n");

		PRINTF(aqs_mics4514_calibration_process.name);
		PRINTF(": aqs calibration process - exiting process\n");
		process_post(&aqs_mics4514_measurement_process, calibration_finished_event, NULL);
		PROCESS_EXIT();
	}

	aqs_info[MICS4514_SENSOR_RED].ro = 0;
	aqs_info[MICS4514_SENSOR_NOX].ro = 0;
	PRINTF(aqs_mics4514_calibration_process.name);
	PRINTF(" - sensor set to AQS_INIT_PHASE.\n");
	//set a sampling duration
	etimer_set(&et, CLOCK_SECOND * MICS4514_MEASUREMENT_INTERVAL);
	while (sample_count_red != CALIBRATION_SAMPLE_COUNT && sample_count_nox != CALIBRATION_SAMPLE_COUNT){
		PRINTF(aqs_mics4514_calibration_process.name);
		PRINTF(" - sampling %hhu out of %d for RED.\n", sample_count_red + 1, CALIBRATION_SAMPLE_COUNT);

		rs_red += measure_aqs_ro( &measure_aqs_data_red );
		rs_nox += measure_aqs_ro( &measure_aqs_data_nox );
		sample_count_red++;
		sample_count_nox++;

		//wait for sampling period to lapse
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		etimer_reset(&et);
	}

	rs_red /= sample_count_red;
	rs_nox /= sample_count_nox;
	aqs_info[MICS4514_SENSOR_RED].ro = rs_red;
	aqs_info[MICS4514_SENSOR_NOX].ro = rs_nox;

	PRINTF(aqs_mics4514_calibration_process.name);
	PRINTF(": aqs calibration process - calibration finished w/ values.\n");
	PRINTF("(RED)Ro at clean air = %lu\n", aqs_info[MICS4514_SENSOR_RED].ro);
	PRINTF("(NOX)Ro at clean air = %lu\n", aqs_info[MICS4514_SENSOR_NOX].ro);

	aqs_info[MICS4514_SENSOR_RED].state = AQS_ENABLED;
	aqs_info[MICS4514_SENSOR_NOX].state = AQS_ENABLED;

	PRINTF(aqs_mics4514_calibration_process.name);
	PRINTF(": aqs calibration process - sensor set to ENABLED.\n");

	process_post(&aqs_mics4514_measurement_process, calibration_finished_event, NULL);
	PRINTF(aqs_mics4514_calibration_process.name);
	PRINTF(": posted calibration_finished_event to \n");
	PRINTF(aqs_mics4514_measurement_process.name);
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq7_measurement_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t preheatSteps;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq7_measurement_process started\n");

	//make sure sensor is enabled or in init phase
	if ( aqs_info[MQ7_SENSOR].state != AQS_ENABLED && aqs_info[MQ7_SENSOR].state != AQS_INIT_PHASE) {
		PRINTF("aqs_mq7_measurement_process: ERROR - sensor is not enabled.");
		PROCESS_EXIT();
	}

	preheatSteps = MQ7_PREHEAT_STEPS;
	measure_aqs_data.sensor_type = MQ7_SENSOR;
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ7_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ7_SENSOR_PIN_MASK;
#endif

	/* take control of the heating pin */
	GPIO_SOFTWARE_CONTROL(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);
	GPIO_SET_OUTPUT(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);
	/* default state of the pin should be low */
	GPIO_CLR_PIN(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK);
	
	/* pre-heat procedure */
	PRINTF("aqs_mq7_measurement_process: pre-heat procedure started\n");
	while(preheatSteps > 0){
		turn_on_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_measurement_process(pre-heat): heater turned on\n");
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		turn_off_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_measurement_process(pre-heat): heater turned off\n");
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		preheatSteps--;
	}
	PRINTF("aqs_mq7_measurement_process: pre-heat procedure ended\n");
	
	//call the calibration procedure
	process_start(&aqs_mq7_calibration_process, NULL);
	
	PRINTF("aqs_mq7_measurement_process: process paused\n");
	//wait for calibration to finish
	while (ev != calibration_finished_event){
		PROCESS_WAIT_EVENT();
	}
	PRINTF("aqs_mq7_measurement_process: process resumed\n");

	/*heating procedure*/
	PRINTF("aqs_mq7_measurement_process: heating and measurement cycling started\n");
	while(1){
		turn_on_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_measurement_process(cycle): heater turned on\n");
		//take measurement during this phase
		measure_aqs( &measure_aqs_data );
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		turn_off_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_measurement_process(cycle): heater turned off\n");
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	}
	
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq131_measurement_process, ev, data)
{
	//declarations
	static struct etimer et;
	static measure_aqs_data_t measure_aqs_data;	

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq131_measurement_process started\n");

	//make sure sensor is enabled or in init phase
	if ( aqs_info[MQ131_SENSOR].state != AQS_ENABLED && aqs_info[MQ131_SENSOR].state != AQS_INIT_PHASE &&
		aqs_info[MQ131_SENSOR].state != AQS_BUSY) {
		PRINTF("aqs_mq131_measurement_process: ERROR - sensor is not enabled.");
		PROCESS_EXIT();
	}
	
	measure_aqs_data.sensor_type = MQ131_SENSOR;
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ131_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ131_SENSOR_PIN_MASK;
#endif
	
	/* pre-heat procedure */
	PRINTF("aqs_mq131_measurement_process: pre-heat procedure started\n");
	etimer_set(&et, CLOCK_SECOND * MQ131_HEATING_TIME);
	PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	PRINTF("aqs_mq131_measurement_process: pre-heat procedure ended\n");	

	//call the calibration procedure
	process_start(&aqs_mq131_calibration_process, NULL);
	
	PRINTF("aqs_mq131_measurement_process: process paused\n");
	//wait for calibration to finish
	while (ev != calibration_finished_event){
		PROCESS_WAIT_EVENT();
	}
	PRINTF("aqs_mq131_measurement_process: process resumed\n");

	/*measurement procedure*/
	PRINTF("aqs_mq131_measurement_process: measurement cycling started\n");
	while(1){
		etimer_set(&et, CLOCK_SECOND * MQ131_MEASUREMENT_INTERVAL);
		measure_aqs( &measure_aqs_data );
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	}
	
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq135_measurement_process, ev, data)
{	
	//declarations
	static struct etimer et;
	static measure_aqs_data_t measure_aqs_data;
	
	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq135_measurement_process started\n");

	//make sure sensor is enabled or in init phase
	if ( aqs_info[MQ135_SENSOR].state != AQS_ENABLED && aqs_info[MQ135_SENSOR].state != AQS_INIT_PHASE) {
		PRINTF("aqs_mq135_measurement_process: ERROR - sensor is not enabled.");
		PROCESS_EXIT();
	}

	measure_aqs_data.sensor_type = MQ135_SENSOR;
#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_data.measure_channel = MQ135_SENSOR_EXT_ADC_CHANNEL;
#else
	measure_aqs_data.measure_pin_mask = MQ135_SENSOR_PIN_MASK;
#endif
	
	/* pre-heat procedure */
	PRINTF("aqs_mq135_measurement_process: pre-heat procedure started\n");
	etimer_set(&et, CLOCK_SECOND * MQ135_HEATING_TIME);
	PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	PRINTF("aqs_mq135_measurement_process: pre-heat procedure ended\n");

	//call the calibration procedure
	process_start(&aqs_mq135_calibration_process, NULL);
	
	PRINTF("aqs_mq135_measurement_process: process paused\n");
	//wait for calibration to finish
	while (ev != calibration_finished_event){
		PROCESS_WAIT_EVENT();
	}
	PRINTF("aqs_mq135_measurement_process: process resumed\n");

	/*heating procedure*/
	PRINTF("aqs_mq135_measurement_process: measurement cycling started\n");
	while(1){
		etimer_set(&et, CLOCK_SECOND * MQ135_MEASUREMENT_INTERVAL);
		measure_aqs( &measure_aqs_data );
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	}
	
	PROCESS_END();
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mics4514_measurement_process, ev, data)
{
	//declarations
	static struct etimer et;
	static measure_aqs_data_t measure_aqs_red_data;
	static measure_aqs_data_t measure_aqs_nox_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(" started\n");

	//make sure sensor is enabled or in init phase
	if ( aqs_info[MICS4514_SENSOR_NOX].state != AQS_ENABLED && aqs_info[MICS4514_SENSOR_NOX].state != AQS_INIT_PHASE &&
			aqs_info[MICS4514_SENSOR_RED].state != AQS_ENABLED && aqs_info[MICS4514_SENSOR_RED].state != AQS_INIT_PHASE) {
		PRINTF(aqs_mics4514_measurement_process.name);
		PRINTF(": ERROR - sensor is not enabled.");
		PROCESS_EXIT();
	}

	measure_aqs_red_data.sensor_type = MICS4514_SENSOR_RED;


	measure_aqs_nox_data.sensor_type = MICS4514_SENSOR_NOX;


#if ADC_SENSORS_CONF_USE_EXTERNAL_ADC
	measure_aqs_red_data.measure_channel = MICS4514_SENSOR_RED_EXT_ADC_CHANNEL;
	measure_aqs_nox_data.measure_channel = MICS4514_SENSOR_NOX_EXT_ADC_CHANNEL;
#else
	measure_aqs_red_data.measure_pin_mask = MICS4514_SENSOR_RED_PIN_MASK;
	measure_aqs_nox_data.measure_pin_mask = MICS4514_SENSOR_NOX_PIN_MASK;
#endif

	/* pre-heat procedure */
	turn_on_heater(MICS4514_SENSOR_HEATING_PORT_BASE, MICS4514_SENSOR_HEATING_PIN_MASK, MICS4514_HEATING_TIME, &et);
	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(": pre-heat procedure started\n");

	PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	turn_off_heater(MICS4514_SENSOR_HEATING_PORT_BASE, MICS4514_SENSOR_HEATING_PIN_MASK, 0, &et);
	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(": pre-heat procedure ended\n");

	//call the calibration procedure
	process_start(&aqs_mics4514_calibration_process, NULL);

	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(": process paused\n");
	//wait for calibration to finish
	while (ev != calibration_finished_event){
		PROCESS_WAIT_EVENT();
	}
	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(": process resumed\n");

	/* measurement cycling */
	PRINTF(aqs_mics4514_measurement_process.name);
	PRINTF(": measurement cycling started\n");
	while(1){
		etimer_set(&et, CLOCK_SECOND * MICS4514_MEASUREMENT_INTERVAL);
		measure_aqs( &measure_aqs_red_data );
		measure_aqs( &measure_aqs_nox_data );
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
	}

	PROCESS_END();
}
/*------------------------------------------------------------------*/
SENSORS_SENSOR(aqs_sensor, AQS_SENSOR, value, configure, status);
/*------------------------------------------------------------------*/
