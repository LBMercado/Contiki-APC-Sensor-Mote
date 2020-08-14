/*
 * adc128s022.h
 *
 *  Created on: Aug 4, 2020
 *      Author: ubuntu-mercado
 */

#ifndef PLATFORM_ZOUL_DEV_ADC128S022_H_
#define PLATFORM_ZOUL_DEV_ADC128S022_H_
#include "ssi.h"
#include "dev/gpio.h"
/*------------------------------------------------------------------*/
#define ADC128S022_ACTIVE             SENSORS_ACTIVE
#define ADC128S022_INIT               SENSORS_HW_INIT
#define ADC128S022_ERROR              (-1)
#define ADC128S022_SUCCESS            0x00
/*------------------------------------------------------------------*/
#define ADC128S022_ENABLE             1
#define ADC128S022_DISABLE            0
/*------------------------------------------------------------------*/
#define ADC128S022_SCLK_FREQ          1600000
/*------------------------------------------------------------------*/
/* 12 ENOB ADC */
#define ADC128S022_ADC_MAX_LEVEL      4095
/*------------------------------------------------------------------*/
#define ADC128S022_ADC_MAX_CHANNEL    7
/*------------------------------------------------------------------*/
#ifdef ADC128S022_CONF_SPI_INSTANCE
#if ADC128S022_CONF_SPI_INSTANCE > (SSI_INSTANCE_COUNT - 1)
#error "SPI instance must be 0 or 1."
#endif
#define ADC128S022_SPI_INSTANCE       ADC128S022_CONF_SPI_INSTANCE
#else
#define ADC128S022_SPI_INSTANCE       1
#endif
/*------------------------------------------------------------------*/
#ifdef ADC128S022_CONF_CSN_PORT
#define ADC128S022_CSN_PORT           ADC128S022_CONF_CSN_PORT
#else
#define ADC128S022_CSN_PORT           GPIO_D_NUM
#endif
#ifdef ADC128S022_CONF_CSN_PIN
#define ADC128S022_CSN_PIN            ADC128S022_CONF_CSN_PIN
#else
#define ADC128S022_CSN_PIN            1
#endif
/*------------------------------------------------------------------*/
#define ADC128S022_NAME "ADC128S022 12-bit ADC"
extern const struct sensors_sensor adc128s022;
/*------------------------------------------------------------------*/
#endif /* PLATFORM_ZOUL_DEV_ADC128S022_H_ */
