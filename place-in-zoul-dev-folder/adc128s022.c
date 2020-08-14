/*
 * adc128s022.c
 *
 *  Created on: Aug 4, 2020
 *      Author: ubuntu-mercado
 */
/* Contiki System Headers */
#include "dev/zoul-sensors.h"
#include "dev/adc-sensors.h"
#include "dev/ioc.h"
#include "spi-arch.h"
#include "dev/gpio.h"
#include "dev/spi.h"
/* Project-specific Headers */
#include "dev/adc128s022.h"
/* C Library  */
#include <stdio.h>
/*------------------------------------------------------------------*/
#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*------------------------------------------------------------------*/
#define ADC128S022_DATA_SIZE                    8
/*------------------------------------------------------------------*/
static int8_t enabled = 0;
static uint8_t used_channels = 0;
/*------------------------------------------------------------------*/
static uint16_t sample(uint8_t channel){
	uint8_t hi_byte, low_byte;
	// initiate a transaction
	SPIX_CS_CLR(ADC128S022_CSN_PORT, ADC128S022_CSN_PIN);

	/* transmit the data (channel to sample)
	 * refer to data sheet, only 3 of the 8 data bits are important, the rest are don't cares
	 *  ____________________________________________________________________
	 * |BIT7(MSB)| BIT6  | BIT5 |  BIT4 |  BIT3  | BIT2 |  BIT1  | BIT0(LSB)|
	 * |---------|-------|------|-------|--------|------|--------|----------|
	 * |DONTC    | DONTC | ADD2 |  ADD1 |  ADD0  | DONTC|  DONTC | DONTC    |
	 * ----------------------------------------------------------------------
	 *
	 * */
	// transmit the data (high byte part)
	SPIX_WAITFORTxREADY(ADC128S022_SPI_INSTANCE);
	SPIX_BUF(ADC128S022_SPI_INSTANCE) = channel << 3;
	SPIX_WAITFOREOTx(ADC128S022_SPI_INSTANCE);

	// receive the data (high byte part)
	SPIX_WAITFOREORx(ADC128S022_SPI_INSTANCE);
	// discard 4 MSBs (zeros or don't cares)
	hi_byte = SPIX_BUF(ADC128S022_SPI_INSTANCE) & 0x0F;


	// transmit the data (low byte part)
	SPIX_WAITFORTxREADY(ADC128S022_SPI_INSTANCE);
	SPIX_BUF(ADC128S022_SPI_INSTANCE) = 0;
	SPIX_WAITFOREOTx(ADC128S022_SPI_INSTANCE);

	// receive the data (low byte part)
	SPIX_WAITFOREORx(ADC128S022_SPI_INSTANCE);
	low_byte = SPIX_BUF(ADC128S022_SPI_INSTANCE);

	//end transaction
	SPIX_CS_SET(ADC128S022_CSN_PORT, ADC128S022_CSN_PIN);

	//return 12 ENOB adc value
	return (hi_byte << 8) | low_byte;
}
/*------------------------------------------------------------------*/
static int value(int type){
	uint8_t sample_count = 2;
	uint16_t sample_value = 0;
	if (!enabled) {
		PRINTF("ADC128S022: value - sensor is not enabled.\n");
		return ADC128S022_ERROR;
	}
	if (type > 7 || type < 0) {
		PRINTF("ADC128S022: value - type (channel) must be between 0 and 7.\n");
		return ADC128S022_ERROR;
	}
	if (!(used_channels & (1 << type))) {
		PRINTF("ADC128S022: value - type (channel) is not initialized.\n");
		return ADC128S022_ERROR;
	}
	/* TODO: try to find a way to sample only once
	 * For some reason, the adc read value is only reliable on second sample, thus requiring a re-sampling
	 * for each sample taken.
	 * */

	while (sample_count != 0) {
		sample_value = sample(type);
		sample_count--;
	}

	return sample_value;
}
/*------------------------------------------------------------------*/
static int configure(int type, int value){
	switch(type){
	case ADC128S022_INIT:
		if (value > 7 || value < 0) {
			PRINTF("ADC128S022: configure - value (channel) must be between 0 and 7.\n");
			return ADC128S022_ERROR;
		}
		if (used_channels & (1 << value)) {
			PRINTF("ADC128S022: configure - value (channel) is already used.\n");
			return ADC128S022_ERROR;
		}
		if (!enabled){
			spix_cs_init(ADC128S022_CSN_PORT, ADC128S022_CSN_PIN);
			spix_init(ADC128S022_SPI_INSTANCE);
			spix_set_clock_freq(ADC128S022_SPI_INSTANCE, ADC128S022_SCLK_FREQ);
			enabled = 1;
		}
		used_channels |= 1 << value;
		return ADC128S022_SUCCESS;
		break;
	case ADC128S022_ACTIVE:
		if (enabled && !value) {
			spix_disable(ADC128S022_SPI_INSTANCE);
			enabled = 0;
		} else if (!enabled && value){
			spix_enable(ADC128S022_SPI_INSTANCE);
			enabled = 1;
		}
		return ADC128S022_SUCCESS;
		break;
	default:
		PRINTF("ADC128S022: configure - incorrect type or value passed.\n");
		return ADC128S022_ERROR;
	}
}
/*------------------------------------------------------------------*/
SENSORS_SENSOR(adc128s022, ADC128S022_NAME, value, configure, NULL);
/*------------------------------------------------------------------*/
