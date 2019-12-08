//include files
#include "dev/air-quality-sensor.h"
#include "sys/etimer.h"
#include "dev/zoul-sensors.h"
#include "dev/adc-sensors.h"
#include <math.h>
/*------------------------------------------------------------------*/
#define MQ7_SENSOR_PIN_MASK             GPIO_PIN_MASK(MQ7_SENSOR_CTRL_PIN)
#define MQ131_SENSOR_PIN_MASK           GPIO_PIN_MASK(MQ131_SENSOR_CTRL_PIN)
#define MQ135_SENSOR_PIN_MASK           GPIO_PIN_MASK(MQ135_SENSOR_CTRL_PIN)
#define MQ7_SENSOR_HEATING_PIN_MASK     GPIO_PIN_MASK(MQ7_SENSOR_HEATING_PIN)
#define MQ7_SENSOR_HEATING_PORT_BASE    GPIO_PORT_TO_BASE(MQ7_SENSOR_HEATING_PORT)
/*------------------------------------------------------------------*/
//values for calibration
#define CALIBRATION_SAMPLE_COUNT 100 //number of samples before finalizing calibration values
/*------------------------------------------------------------------*/
/*Scaling Factors*/
//scaling factors for resistances
#define OHM_SCALETO_MILLIOHM     1000
#define KOHM_SCALETO_MILLIOHM    1000000
//scaling factors for PPM/PPB
#define PPM_SCALETO_PPB          1000
/*------------------------------------------------------------------*/
//tolerance values
#define RESRATIO_TOLERANCE       500
/*------------------------------------------------------------------*/
#define AQS_ADC_REF      5000
#define AQS_ADC_MAX_VAL  32768  //15-bits ENOB ADC
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
PROCESS(aqs_mq7_measurement_process, "AQS Measurement (MQ7) Process Handler");
PROCESS(aqs_mq131_measurement_process, "AQS Measurement (MQ131) Process Handler");
PROCESS(aqs_mq135_measurement_process, "AQS Measurement (MQ135) Process Handler");
/*------------------------------------------------------------------*/
static process_event_t calibration_finished_event;
/*------------------------------------------------------------------*/
typedef struct{
	uint8_t sensorType;
	uint32_t measure_pin_mask;
} measure_aqs_data_t;
typedef struct {
  uint8_t state; //sensor status
  uint16_t value; //resratio equivalent of sensor output
  uint32_t ro; //(in milliohms), sensor resistance in clean air
} aqs_info_t;
static aqs_info_t aqs_info[3];
static uint8_t event_allocated;
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
/* Converts raw ADC value in millivolts to its corresponding Rs/Ro ratio value based on the sensor type.
@param: type = sensor identifier (MQ7_SENSOR,MQ131_SENSOR,MQ135_SENSOR)
@param: value = ADC value to translate
@returns: unsigned int (32 bits long) Rs/Ro in fixed point (significant to the 3rd decimal digit)
*/
static uint32_t
ConvertRawValToResRatio(const uint8_t type, uint32_t value)
{
	float val; //will be Rs/Ro in datasheet
	/* rs = sensor resistance in various conditions*/
	uint32_t rs; //(in milliohms)
	val = value;
	
	switch (type)
	{
	case MQ7_SENSOR:	
		/* Refer to datasheet
					RS = ( (Vc – VRL) / VRL ) * RL
					Where:
						Vc = source voltage at time of measurement (5V or 1.4V)
						VRL = reference resistor voltage, output of A0 Pin

						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
			*/
		if (aqs_info[MQ7_SENSOR].ro == 0) {
			PRINTF("ERROR: @AQS:ConvertRawValToResRatio(MQ7) - Ro is not calibrated.\n");
			return 0;
		}
		rs = ( ( 5000 - val ) / (float)val ) * (MQ7_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		val = (float)rs / aqs_info[MQ7_SENSOR].ro * 1000; //preserve decimal part by 3 digits
		
		//make sure valid is within range of sensor specs else warn user
		if (val + RESRATIO_TOLERANCE <= MQ7_RESRATIO_MIN && 
			val - RESRATIO_TOLERANCE >= MQ7_RESRATIO_MAX) {
			PRINTF("WARNING: @AQS:ConvertRawValToResRatio(MQ7) - \'val\' is out of bounds. ");
			PRINTF("Computed value: %u.%03u\n", (uint16_t)val, (uint16_t)( (val - (uint16_t)val) * 1000) );
		}
		
		return (uint32_t)val;
		break;
	case MQ131_SENSOR:
		/* Refer to datasheet
					RS = ( Vc / VRL - 1 ) × RL

					Where:

						Vc = source voltage at time of measurement (5V)
						VRL = reference resistor voltage, output of A0 Pin
						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
				*/
		if (aqs_info[MQ131_SENSOR].ro == 0) {
			PRINTF("ERROR: @AQS:ConvertRawValToResRatio(MQ131) - Ro is not calibrated.\n");
			return 0;
		}
		rs = ( ( 5000 / (float)val ) - 1 ) * ( MQ131_RL_KOHM * KOHM_SCALETO_MILLIOHM );
		val = (float)rs / aqs_info[MQ131_SENSOR].ro * 1000; //preserve decimal part by 3 digits
		
		//make sure valid is within range of sensor specs else warn user
		if (val + RESRATIO_TOLERANCE <= MQ131_RESRATIO_MIN * 1000 ||
			val - RESRATIO_TOLERANCE >= MQ131_RESRATIO_MAX * 1000) {
			PRINTF("WARNING: @AQS:ConvertRawValToResRatio(MQ131) - \'val\' is out of bounds. ");
			PRINTF("Computed value: %u.%03u\n", (uint16_t)val, (uint16_t)( (val - (uint16_t)val) * 1000) );
		}
		return (uint32_t)val;
		break;
	case MQ135_SENSOR:
		/* Refer to datasheet
					RS = ( Vc / VRL - 1 ) × RL
					Where:
						Vc = source voltage at time of measurement (5V)
						VRL = reference resistor voltage, output of A0 Pin
						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
				*/
		if (aqs_info[MQ135_SENSOR].ro == 0) {
			PRINTF("ERROR: @AQS:ConvertRawValToResRatio(MQ135) - Ro is not calibrated.\n");
			return 0;
		}
		rs = ( ( 5000 / (float)val ) - 1 ) * ( MQ135_RL_KOHM * KOHM_SCALETO_MILLIOHM );
		val = (float)rs / aqs_info[MQ135_SENSOR].ro * 1000; //preserve decimal part by 3 digits
		
		//make sure valid is within range of sensor specs else warn user
		if (val + RESRATIO_TOLERANCE <= MQ135_RESRATIO_MIN * 1000 || 
			val - RESRATIO_TOLERANCE >= MQ135_RESRATIO_MAX * 1000) {
			PRINTF("WARNING: @AQS:ConvertRawValToResRatio(MQ135) - \'val\' is out of bounds. ");
			PRINTF("Computed value: %u.%03u\n", (uint16_t)val, (uint16_t)( (val - (uint16_t)val) * 1000) );
		}
		return (uint32_t)val;
		break;
	default:
		PRINTF("Error for AQS: ConvertRawValToResRatio function parameter \'type\' is not valid.\n");
		return 0;
	}
}
/*------------------------------------------------------------------*/
/* (this function is used during calibration procedures)
*  Converts raw ADC value in millivolts to its corresponding Ro (sensor resistance) resistance based on the sensor type.
*   @param: type = sensor identifier (MQ7_SENSOR,MQ131_SENSOR,MQ135_SENSOR)
*   @param: value = ADC value to translate
*   @returns: unsigned int (32 bits long) Ro in milliohms
*/
static uint32_t
ConvertRawValToSensorRes(const uint8_t type, uint32_t value)
{	
	switch (type)
	{
	case MQ7_SENSOR:	
		/* Refer to datasheet
					RS = ( (Vc – VRL) / VRL ) * RL
					Where:
						Vc = source voltage at time of measurement (5V or 1.4V)
						VRL = reference resistor voltage, output of A0 Pin
						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
			*/
		value = ( ( 5000 - value ) / (float)value ) * (MQ7_RL_KOHM * KOHM_SCALETO_MILLIOHM);
		return value;
	case MQ131_SENSOR:
		/* Refer to datasheet
					RS = ( Vc / VRL - 1 ) × RL
					Where:
						Vc = source voltage at time of measurement (5V)
						VRL = reference resistor voltage, output of A0 Pin
						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
				*/
		value = ( ( 5000 / (float)value ) - 1 ) * ( MQ131_RL_KOHM * KOHM_SCALETO_MILLIOHM );
		return value;
		break;
	case MQ135_SENSOR:
		/* Refer to datasheet
					RS = ( Vc / VRL - 1 ) × RL
					Where:
						Vc = source voltage at time of measurement (5V)
						VRL = reference resistor voltage, output of A0 Pin
						*RL = reference resistor
						
						*assumed to be valued around ~5 - ~20 Ω
				*/
		value = ( ( 5000 / (float)value ) - 1 ) * ( MQ135_RL_KOHM * KOHM_SCALETO_MILLIOHM );
		return value;
		break;
	default:
		PRINTF("Error for AQS: ConvertRawValToResRatio function parameter \'type\' is not valid.\n");
		return 0;
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
	if (aqs_info_data->sensorType != MQ7_SENSOR && 
			aqs_info_data->sensorType != MQ131_SENSOR &&
			aqs_info_data->sensorType != MQ135_SENSOR){
		PRINTF("measure_aqs_ro: sensor(0x%02x) ERROR - unexpected sensor type.",
			aqs_info_data->sensorType);
		return 0;
	}
	
	if ( aqs_info[aqs_info_data->sensorType].state != AQS_BUSY &&
		aqs_info[aqs_info_data->sensorType].state != AQS_ENABLED &&
		aqs_info[aqs_info_data->sensorType].state != AQS_INIT_PHASE){
		PRINTF("measure_aqs_ro: sensor(0x%02x) is DISABLED.\n",
			aqs_info_data->sensorType);
		return 0;
	}
	
	aqs_info[aqs_info_data->sensorType].state = AQS_BUSY;
	PRINTF("measure_aqs_ro: sensor(0x%02x) set to BUSY.\n", aqs_info_data->sensorType);
	
	val = adc_zoul.value(aqs_info_data->measure_pin_mask);
	if (val == ZOUL_SENSORS_ERROR){
		PRINTF("measure_aqs_ro: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensorType);
		return 0;
	}
	PRINTF("measure_aqs: sensor(0x%02x) raw ADC value = %lu\n", aqs_info_data->sensorType, val);
	if (val > AQS_ADC_MAX_VAL)
		val = AQS_ADC_MAX_VAL;

	val *= AQS_ADC_REF;
	val /= AQS_ADC_MAX_VAL;
	PRINTF("measure_aqs_ro: sensor(0x%02x) mv ADC value = %lu\n", aqs_info_data->sensorType, val);
	
	val = ConvertRawValToSensorRes(aqs_info_data->sensorType, val);
	PRINTF("measure_aqs_ro: sensor(0x%02x) sensor resistance (milli) = %lu\n", aqs_info_data->sensorType, val);
	aqs_info[aqs_info_data->sensorType].state = AQS_INIT_PHASE;
	PRINTF("measure_aqs_ro: sensor(0x%02x) set to AQS_INIT_PHASE.\n", aqs_info_data->sensorType);

	return val;
}
/*------------------------------------------------------------------*/
static void 
measure_aqs(const void* data_ptr)
{
	measure_aqs_data_t* aqs_info_data;
	uint32_t val;
	
	aqs_info_data = (measure_aqs_data_t*)(data_ptr);
	
	/* make sure it is a valid sensor type */
	if (aqs_info_data->sensorType != MQ7_SENSOR && 
			aqs_info_data->sensorType != MQ131_SENSOR &&
			aqs_info_data->sensorType != MQ135_SENSOR){
		PRINTF("measure_aqs: sensor(0x%02x) ERROR - unexpected sensor type.",
			aqs_info_data->sensorType);
		return;
	}
	
	if ( aqs_info[aqs_info_data->sensorType].state != AQS_BUSY &&
		aqs_info[aqs_info_data->sensorType].state != AQS_ENABLED ){
		PRINTF("measure_aqs: sensor(0x%02x) is DISABLED.\n",
			aqs_info_data->sensorType);
		return;
	}
	
	aqs_info[aqs_info_data->sensorType].state = AQS_BUSY;
	PRINTF("measure_aqs: sensor(0x%02x) set to BUSY.\n", aqs_info_data->sensorType);
		
	val = adc_zoul.value(aqs_info_data->measure_pin_mask);
	if (val == ZOUL_SENSORS_ERROR){
		PRINTF("measure_aqs: sensor(0x%02x) failed to get value from ADC sensor\n", aqs_info_data->sensorType);
		return;
	}
	PRINTF("measure_aqs: sensor(0x%02x) raw ADC value = %lu\n", aqs_info_data->sensorType, val);
	if (val > AQS_ADC_MAX_VAL)
		val = AQS_ADC_MAX_VAL;

	val *= AQS_ADC_REF;
	val /= AQS_ADC_MAX_VAL;

	PRINTF("measure_aqs: sensor(0x%02x) mv ADC value = %lu\n", aqs_info_data->sensorType, val);
	
	aqs_info[aqs_info_data->sensorType].value = ConvertRawValToResRatio(aqs_info_data->sensorType, val);
	PRINTF("measure_aqs: sensor(0x%02x) ResRatio value = %u\n", aqs_info_data->sensorType, aqs_info[aqs_info_data->sensorType].value);
	aqs_info[aqs_info_data->sensorType].state = AQS_ENABLED;
	PRINTF("measure_aqs: sensor(0x%02x) set to ENABLED.\n", aqs_info_data->sensorType);
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
configure(int type, int value)
{
	if(type != SENSORS_ACTIVE) {
		PRINTF("Error for AQS: configure function parameter \'type\' is not SENSORS_ACTIVE.\n");
		return AQS_ERROR;
	}

	switch (value)
	{
	case MQ7_SENSOR:
		//do not re-enable the sensor
		if ( aqs_info[MQ7_SENSOR].state == AQS_ENABLED || aqs_info[MQ7_SENSOR].state == AQS_BUSY ||
			aqs_info[MQ7_SENSOR].state == AQS_INIT_PHASE ) {
			PRINTF("AQS: configure function - MQ7_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}
		
		if (adc_zoul.configure(SENSORS_HW_INIT, MQ7_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ7_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
		//set initial values
		aqs_info[MQ7_SENSOR].ro = MQ7_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ7_SENSOR].state = AQS_INIT_PHASE;
		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ7_SENSOR state changed to AQS_INIT_PHASE, proceeding to heating/calibration.\n");

		//begin the heating process
		process_start(&aqs_mq7_measurement_process, NULL);

		return AQS_SUCCESS;
		break;
	case MQ131_SENSOR:
		//do not re-enable the sensor
		if (aqs_info[MQ131_SENSOR].state == AQS_ENABLED || aqs_info[MQ131_SENSOR].state == AQS_BUSY || 
			aqs_info[MQ131_SENSOR].state == AQS_INIT_PHASE) {
			PRINTF("AQS: configure function - MQ131_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}

		if(adc_zoul.configure(SENSORS_HW_INIT, MQ131_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ131_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
		//set initial values
		aqs_info[MQ131_SENSOR].ro = MQ131_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ131_SENSOR].state = AQS_INIT_PHASE;
		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ131_SENSOR state changed to AQS_INIT_PHASE, proceeding to heating/calibration.\n");

		//begin the heating process
		process_start(&aqs_mq131_measurement_process, NULL);
				
		return AQS_SUCCESS;
		break;
	case MQ135_SENSOR:
		//do not re-enable the sensor
		if ( aqs_info[MQ135_SENSOR].state == AQS_ENABLED || aqs_info[MQ135_SENSOR].state == AQS_BUSY ||
			aqs_info[MQ135_SENSOR].state == AQS_INIT_PHASE) {
			PRINTF("AQS: configure function - MQ135_SENSOR already enabled.\n");
			return AQS_SUCCESS;
		}

		if(adc_zoul.configure(SENSORS_HW_INIT, MQ135_SENSOR_PIN_MASK) == ZOUL_SENSORS_ERROR) {
			PRINTF("Error for AQS: configure function (MQ135_SENSOR) - Failed to configure ADC sensor.\n");
			return AQS_ERROR;
		}
		//set initial values
		aqs_info[MQ135_SENSOR].ro = MQ135_RO_CLEAN_AIR * OHM_SCALETO_MILLIOHM;
		aqs_info[MQ135_SENSOR].state = AQS_INIT_PHASE;
		allocate_calibrate_event();
		PRINTF("AQS: configure function - MQ135_SENSOR state changed to AQS_INIT_PHASE, proceeding to heating/calibration.\n");

		//begin the heating process
		process_start(&aqs_mq135_measurement_process, NULL);
		
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
	if (type != MQ7_SENSOR && 
			type != MQ131_SENSOR &&
			type != MQ135_SENSOR){
		PRINTF("AQS value function: ERROR - unexpected sensor type.");
		return AQS_ERROR;
	}
	
	/* make sure sensor is enabled or not initializating*/
	if ( aqs_info[type].state != AQS_ENABLED && aqs_info[type].state != AQS_INIT_PHASE 
		&& aqs_info[type].state != AQS_BUSY) {
		PRINTF("AQS value function: ERROR - sensor is not enabled.");
		return AQS_ERROR;
	}

	/* warn when sensor is measuring */
	if ( aqs_info[type].state == AQS_BUSY ) {
		PRINTF("AQS value function: WARNING - sensor is currently measuring.");
	}

	return aqs_info[type].value;
}
/*------------------------------------------------------------------*/
static int
status(int type){
	/* make sure it is a valid sensor type */
	if (type != MQ7_SENSOR && 
			type != MQ131_SENSOR &&
			type != MQ135_SENSOR){
		PRINTF("AQS status function: ERROR - unexpected sensor type.");
		return AQS_ERROR;
	}
	
	return aqs_info[type].state;
}
/*------------------------------------------------------------------*/
PROCESS_THREAD(aqs_mq7_calibration_process, ev, data)
{
	//declarations
	static struct etimer et;
	static uint8_t sampleCount;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint32_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq7_calibration_process started\n");

	//initialization
	rs = 0;
	sampleCount = 0;
	measure_aqs_data.sensorType = MQ7_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ7_SENSOR_PIN_MASK;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ7_SENSOR].ro > 0){
		PRINTF("AQS(MQ7): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ7_SENSOR].ro);
		aqs_info[MQ7_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ7): aqs calibration process - sensor set to ENABLED.\n");
		process_post(&aqs_mq7_measurement_process, calibration_finished_event, NULL);
		PRINTF("aqs_mq7_calibration_process: posted calibration_finished_event to aqs_mq7_measurement_process\n");
		PRINTF("AQS(MQ7): aqs calibration process - exiting process\n");
		PROCESS_EXIT();
	}
	
	aqs_info[MQ7_SENSOR].state = AQS_INIT_PHASE;
	aqs_info[MQ7_SENSOR].ro = 0;
	PRINTF("AQS(MQ7): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	while (sampleCount != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ7): aqs calibration process - sampling %hhu out of %d.\n", sampleCount + 1, CALIBRATION_SAMPLE_COUNT);
		
		//turn the heater on and take measurement at this phase
		turn_on_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_HIGH_TIME, &et);
		PRINTF("aqs_mq7_calibration_process: heater turned on\n");
		rs = measure_aqs_ro( &measure_aqs_data );
		aqs_info[MQ7_SENSOR].ro += rs;
		
		//wait for the heater on duration to expire
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		
		//turn off the heater and wait for cooldown period
		turn_off_heater(MQ7_SENSOR_HEATING_PORT_BASE, MQ7_SENSOR_HEATING_PIN_MASK, MQ7_HEATING_LOW_TIME, &et);
		PRINTF("aqs_mq7_calibration_process: heater turned off\n");
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		
		sampleCount++;
	}
	
	aqs_info[MQ7_SENSOR].ro /= sampleCount;
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
	static uint8_t sampleCount;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint32_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq131_calibration_process started\n");

	//initialization
	rs = 0;
	sampleCount = 0;
	measure_aqs_data.sensorType = MQ131_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ131_SENSOR_PIN_MASK;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ131_SENSOR].ro > 0){
		PRINTF("AQS(MQ131): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ131_SENSOR].ro);
		aqs_info[MQ131_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ131): aqs calibration process - sensor set to ENABLED.\n");
		process_post(&aqs_mq131_measurement_process, calibration_finished_event, NULL);
		PRINTF("aqs_mq131_calibration_process: posted calibration_finished_event to aqs_mq131_measurement_process\n");
		PRINTF("AQS(MQ131): aqs calibration process - exiting process\n");
		PROCESS_EXIT();
	}
	
	aqs_info[MQ131_SENSOR].state = AQS_INIT_PHASE;
	aqs_info[MQ131_SENSOR].ro = 0;
	PRINTF("AQS(MQ131): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	while (sampleCount != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ131): aqs calibration process - sampling %hhu out of %d.\n", sampleCount + 1, CALIBRATION_SAMPLE_COUNT);
		
		rs = measure_aqs_ro( &measure_aqs_data );
		aqs_info[MQ131_SENSOR].ro += rs;
		//set a sampling duration
		etimer_set(&et, CLOCK_SECOND * MQ131_MEASUREMENT_INTERVAL);
		//wait for sampling period to lapse
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		sampleCount++;
	}
	
	aqs_info[MQ131_SENSOR].ro /= sampleCount;
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
	static uint8_t sampleCount;
	/* Rs = sensor resistance in various conditions (common for MQ Sensors, in milliohms)*/
	static uint32_t rs;
	static measure_aqs_data_t measure_aqs_data;

	PROCESS_EXITHANDLER();
	PROCESS_BEGIN();
	PRINTF("aqs_mq135_calibration_process started\n");

	//initialization
	rs = 0;
	sampleCount = 0;
	measure_aqs_data.sensorType = MQ135_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ135_SENSOR_PIN_MASK;

	//check if sensor has already been initialized/calibrated
	if (aqs_info[MQ135_SENSOR].ro > 0){
		PRINTF("AQS(MQ135): aqs calibration process - sensor has been calibrated already with default value\n");
		PRINTF("Ro at clean air = %lu\n", aqs_info[MQ135_SENSOR].ro);
		aqs_info[MQ135_SENSOR].state = AQS_ENABLED;
		PRINTF("AQS(MQ135): aqs calibration process - sensor set to ENABLED.\n");
		process_post(&aqs_mq135_measurement_process, calibration_finished_event, NULL);
		PRINTF("aqs_mq135_calibration_process: posted calibration_finished_event to aqs_mq135_measurement_process\n");
		PRINTF("AQS(MQ135): aqs calibration process - exiting process\n");
		PROCESS_EXIT();
	}
	
	aqs_info[MQ135_SENSOR].state = AQS_INIT_PHASE;
	aqs_info[MQ135_SENSOR].ro = 0;
	PRINTF("AQS(MQ135): aqs calibration process - sensor set to AQS_INIT_PHASE.\n");
	while (sampleCount != CALIBRATION_SAMPLE_COUNT){
		PRINTF("AQS(MQ135): aqs calibration process - sampling %hhu out of %d.\n", sampleCount + 1, CALIBRATION_SAMPLE_COUNT);
		
		rs = measure_aqs_ro( &measure_aqs_data );
		aqs_info[MQ135_SENSOR].ro += rs;

		//set a sampling duration
		etimer_set(&et, CLOCK_SECOND * MQ135_MEASUREMENT_INTERVAL);
		//wait for sampling period to lapse
		PROCESS_WAIT_EVENT_UNTIL( etimer_expired(&et) );
		sampleCount++;
	}
	
	aqs_info[MQ135_SENSOR].ro /= sampleCount;
	PRINTF("AQS(MQ135): aqs calibration process - calibration finished w/ values.\n");
	PRINTF("Ro at clean air = %lu\n", aqs_info[MQ135_SENSOR].ro);
	
	aqs_info[MQ135_SENSOR].state = AQS_ENABLED;
	PRINTF("AQS(MQ135): aqs calibration process - sensor set to ENABLED.\n");

	process_post(&aqs_mq135_measurement_process, calibration_finished_event, NULL);
	PRINTF("aqs_mq135_calibration_process: posted calibration_finished_event to aqs_mq135_measurement_process\n");

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
	measure_aqs_data.sensorType = MQ7_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ7_SENSOR_PIN_MASK;

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
	
	measure_aqs_data.sensorType = MQ131_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ131_SENSOR_PIN_MASK;
	
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

	measure_aqs_data.sensorType = MQ135_SENSOR;
	measure_aqs_data.measure_pin_mask = MQ135_SENSOR_PIN_MASK;
	
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
SENSORS_SENSOR(aqs_sensor, AQS_SENSOR, value, configure, status);
/*------------------------------------------------------------------*/
