#include "dev/anemometer-sensor.h"
#include "dev/zoul-sensors.h"
/*--------------------------------------------------------------------------------*/
//pin and port configuration
#define WIND_SPEED_SENSOR_PIN_MASK GPIO_PIN_MASK(WIND_SPEED_SENSOR_CTRL_PIN)
#define WIND_DIR_SENSOR_PIN_MASK   GPIO_PIN_MASK(WIND_DIR_SENSOR_CTRL_PIN)
/*--------------------------------------------------------------------------------*/
//Scaling factors
#define VOLT_SCALETO_MILLIV        1000
#define MS_SCALETO_CMS             100
/*--------------------------------------------------------------------------------*/
#define WIND_SPEED_TOLERANCE       100 //(in cm/s)
/*--------------------------------------------------------------------------------*/
#define WIND_SENSOR_ADC_REF        5000
#define WIND_SENSOR_ADC_MAX_VAL    32768  //15-bits ENOB ADC
/*--------------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif
/*--------------------------------------------------------------------------------*/
typedef struct {
  uint8_t state; //sensor status
  uint16_t value; //converted output of sensor
} anem_info_t;
/*--------------------------------------------------------------------------------*/
static uint16_t init_pos_value;
static anem_info_t anem_info[2];
/*--------------------------------------------------------------------------------*/
/*Converts raw ADC value (assumed to be in millivolts) to its corresponding wind sensor output
@param: type = sensor type
@param: value = ADC output
@returns: unsigned int (16 bits long) equivalent wind sensor value
*/
static uint16_t
ConvertValToWindOutput(uint8_t type, uint16_t val){
	switch(type){
		case WIND_SPEED_SENSOR:	
			//Convert voltage value to wind speed using range of max and min voltages and wind speed for the
			//anemometer
			if (val <= WIND_ANALOG_V_MIN * VOLT_SCALETO_MILLIV){
				val = WIND_MIN_SPEED;
			}
			else {
				//For voltages above minimum value, use the linear relationship to calculate wind speed.
				float fVal = val;
				fVal = ( fVal - WIND_ANALOG_V_MIN * VOLT_SCALETO_MILLIV ) * WIND_MAX_SPEED / ( WIND_ANALOG_V_MAX * VOLT_SCALETO_MILLIV - WIND_ANALOG_V_MIN * VOLT_SCALETO_MILLIV );
				
				val = fVal * MS_SCALETO_CMS;
				
				return val;
			}
			
			return val;
			break;
		case WIND_DIR_SENSOR:
			//scale value from 5000mV reference to 360
			val *= 360;
			val /= WIND_SENSOR_ADC_REF;
			
			//consider the initial position of the sensor
			int direction;
			direction = (int)val - init_pos_value;
			
			if      (direction > 360)
				direction -= 360;
			else if (direction < 0)
				direction += 360;
			
			if		(direction < 45)  val = WIND_DIR_NORTH;
			else if (direction < 90)  val = WIND_DIR_NORTH | WIND_DIR_EAST;
			else if (direction < 135) val = WIND_DIR_EAST;
			else if (direction < 180) val = WIND_DIR_SOUTH | WIND_DIR_EAST;
			else if (direction < 225) val = WIND_DIR_SOUTH;
			else if (direction < 270) val = WIND_DIR_SOUTH | WIND_DIR_WEST;
			else if (direction < 315) val = WIND_DIR_WEST;
			else if (direction < 360) val = WIND_DIR_NORTH | WIND_DIR_WEST;
			else 				val = WIND_DIR_NORTH;
			
			return val;
			break;
		default:
			PRINTF("ERROR@WIND_SENSOR: ConvertValToWindOutput function parameter \'type\' is not valid.\n");
			return 0;
	}
}
/*--------------------------------------------------------------------------------*/
static void
configure_init_pos(uint16_t direction)
{
	switch(direction){
		case WIND_DIR_NORTH | WIND_DIR_EAST:
			init_pos_value = 45;
			break;
		case WIND_DIR_EAST:
			init_pos_value = 90;
			break;
		case WIND_DIR_SOUTH | WIND_DIR_EAST:
			init_pos_value = 135;
			break;
		case WIND_DIR_SOUTH:
			init_pos_value = 180;
			break;
		case WIND_DIR_SOUTH | WIND_DIR_WEST:
			init_pos_value = 225;
			break;
		case WIND_DIR_WEST:
			init_pos_value = 270;
			break;
		case WIND_DIR_NORTH | WIND_DIR_WEST:
			init_pos_value = 315;
			break;
		case WIND_DIR_NORTH:
		default:
			init_pos_value = 0;
	}
}
/*--------------------------------------------------------------------------------*/
static int
configure(int type, int value){
	if(type != SENSORS_ACTIVE) {
		PRINTF("ERROR@WIND_SENSOR: configure function parameter \'type\' is not SENSORS_ACTIVE.\n");
		return WIND_SENSOR_ERROR;
	}
	
	switch(value){
		case WIND_SPEED_SENSOR:
			//prevent re-enabling of sensors
			if (anem_info[WIND_SPEED_SENSOR].state == WIND_SENSOR_ENABLED) {
				PRINTF("WARNING@WIND_SENSOR(SPEED): Sensor is already enabled.\n");
				return WIND_SENSOR_SUCCESS;
			}

			if(adc_zoul.configure(SENSORS_HW_INIT, WIND_SPEED_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
				PRINTF("Error for Wind Sensor (SPEED): configure function - Failed to configure ADC sensor.\n");
				return WIND_SENSOR_ERROR;
			}
			
			PRINTF("Wind Sensor (SPEED): configure function - WIND_SPEED_SENSOR has been configured.\n");
			//init
			anem_info[WIND_SPEED_SENSOR].value = 0;
			anem_info[WIND_SPEED_SENSOR].state = WIND_SENSOR_ENABLED;
			return WIND_SENSOR_SUCCESS;
			break;
		case WIND_DIR_SENSOR:
			//prevent re-enabling of sensors
			if (anem_info[WIND_DIR_SENSOR].state == WIND_SENSOR_ENABLED) {
				PRINTF("WARNING@WIND_SENSOR(DRCTN): Sensor is already enabled.\n");
				return WIND_SENSOR_SUCCESS;
			}

			if(adc_zoul.configure(SENSORS_HW_INIT, WIND_DIR_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
				PRINTF("Error for Wind Sensor (DRCTN): configure function - Failed to configure ADC sensor.\n");
				return WIND_SENSOR_ERROR;
			}
			
			PRINTF("Wind Sensor (DRCTN): configure function - WIND_DIR_SENSOR has been configured.\n");
			//init
			anem_info[WIND_DIR_SENSOR].value = 0;
			anem_info[WIND_DIR_SENSOR].state = WIND_SENSOR_ENABLED;
			configure_init_pos(WIND_DIR_SENSOR_INIT_POS);
			return WIND_SENSOR_SUCCESS;
			break;
		default:
			PRINTF("Error for Wind Sensor (DRCTN): configure function parameter \'value\' is not valid.\n");
			return WIND_SENSOR_ERROR;
	}
}
/*--------------------------------------------------------------------------------*/
static int
value(int type){
	
	switch(type){
		case WIND_SPEED_SENSOR:
			//make sure sensor is enabled
			if (anem_info[WIND_SPEED_SENSOR].state != WIND_SENSOR_ENABLED) {
				PRINTF("ERROR@WIND_SENSOR(SPEED): Sensor is disabled. Enable with configure function.\n");
				return WIND_SENSOR_ERROR;
			}

			/*512 bit resolution, output is in mV already*/
			anem_info[WIND_SPEED_SENSOR].value = adc_zoul.value(WIND_SPEED_SENSOR_PIN_MASK);
			if (anem_info[WIND_SPEED_SENSOR].value == ZOUL_SENSORS_ERROR){
				PRINTF("Error@WIND_SENSOR(SPEED): value function - failed to get value from ADC sensor\n");
				anem_info[WIND_SPEED_SENSOR].value = 0;
				return WIND_SENSOR_ERROR;
			}
			PRINTF("Wind Sensor (SPEED): value function - raw ADC value = %u\n", anem_info[WIND_SPEED_SENSOR].value);

			if (anem_info[WIND_SPEED_SENSOR].value > WIND_SENSOR_ADC_MAX_VAL)
				anem_info[WIND_SPEED_SENSOR].value = WIND_SENSOR_ADC_MAX_VAL;

			anem_info[WIND_SPEED_SENSOR].value *= WIND_SENSOR_ADC_REF;
			anem_info[WIND_SPEED_SENSOR].value /= WIND_SENSOR_ADC_MAX_VAL;
			PRINTF("Wind Sensor (SPEED): value function - mv ADC value = %u\n", anem_info[WIND_SPEED_SENSOR].value);
			
			anem_info[WIND_SPEED_SENSOR].value = ConvertValToWindOutput(type, anem_info[WIND_SPEED_SENSOR].value);

			if (anem_info[WIND_SPEED_SENSOR].value > WIND_SPEED_TOLERANCE && anem_info[WIND_SPEED_SENSOR].value - WIND_SPEED_TOLERANCE > WIND_MAX_SPEED ) {
				PRINTF("WARNING@WIND_SENSOR(SPEED): value function - Wind Speed exceeded maximum spec. value.\n");
			}
			PRINTF("Wind Sensor (SPEED): value function - value = %u cm/s\n", anem_info[WIND_SPEED_SENSOR].value);
			return anem_info[WIND_SPEED_SENSOR].value;
			break;
		case WIND_DIR_SENSOR:
			//make sure sensor is enabled
			if (anem_info[WIND_DIR_SENSOR].state != WIND_SENSOR_ENABLED) {
				PRINTF("ERROR@WIND_SENSOR(DRCTN): Sensor is disabled. Enable with configure function.\n");
				return WIND_SENSOR_ERROR;
			}
			/*512 bit resolution, output is in mV already*/
			anem_info[WIND_DIR_SENSOR].value = adc_zoul.value(WIND_DIR_SENSOR_PIN_MASK);
			if (anem_info[WIND_DIR_SENSOR].value == ZOUL_SENSORS_ERROR){
				PRINTF("Error for Wind Sensor (DRCTN): value function - failed to get value from ADC sensor\n");
				anem_info[WIND_DIR_SENSOR].value = 0;
				return WIND_SENSOR_ERROR;
			}
			PRINTF("Wind Sensor (DRCTN): value function - raw ADC value = %u\n", anem_info[WIND_SPEED_SENSOR].value);

			if (anem_info[WIND_DIR_SENSOR].value > WIND_SENSOR_ADC_MAX_VAL)
				anem_info[WIND_DIR_SENSOR].value = WIND_SENSOR_ADC_MAX_VAL;
			anem_info[WIND_DIR_SENSOR].value *= WIND_SENSOR_ADC_REF;
			anem_info[WIND_DIR_SENSOR].value /= WIND_SENSOR_ADC_MAX_VAL;
			PRINTF("Wind Sensor (DRCTN): value function - mv ADC value = %u\n", anem_info[WIND_DIR_SENSOR].value);
			
			anem_info[WIND_DIR_SENSOR].value = ConvertValToWindOutput(type, anem_info[WIND_DIR_SENSOR].value);
			PRINTF("Wind Sensor (DRCTN): value function - value = 0x%04x\n", anem_info[WIND_DIR_SENSOR].value);
			
			return anem_info[WIND_DIR_SENSOR].value;
			break;
		default:
			PRINTF("Error for Wind Sensor: configure function parameter \'value\' is not valid.\n");
			return WIND_SENSOR_ERROR;
	}
}
/*--------------------------------------------------------------------------------*/
SENSORS_SENSOR(anem_sensor, ANEM_SENSOR, value, configure, NULL);
/*--------------------------------------------------------------------------------*/
