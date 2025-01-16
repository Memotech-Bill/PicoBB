/*****************************************************************************
 * | File      	:	DEV_Config.h
 * | Author      :   Waveshare team, Memotech-Bill
 * | Function    :	Low level routines
 * | Info        :
 *   Provide the hardware underlying interface	 
 *----------------
 * |	This version:   V1.0
 * | Date        :   Jan 2025
 * | Info        :   PicoBB version
 *
 ******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "stdio.h"
#include <stdbool.h>

bool SPI_Int_Claim (void);
void SPI_Int_Release (void);
void SPI_Claim (void);
void SPI_Release (void);

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t
/*------------------------------------------------------------------------------------------------------*/

static inline void DEV_Digital_Write(UWORD Pin, UBYTE Value)
    {
	gpio_put(Pin, Value);
    }

static inline UBYTE DEV_Digital_Read(UWORD Pin)
    {
	return gpio_get(Pin);
    }

void DEV_GPIO_Mode(UWORD Pin, UWORD Mode);
void DEV_GPIO_Init(void);
void PWM_SetValue (int value);

/********************************************************************************
function:	System Init
note:
	Initialize the communication method
********************************************************************************/

uint8_t System_Init(void);

static inline void System_Exit(void)
    {
    }

/*********************************************
function:	Hardware interface
note:
	SPI4W_Write_Byte(value) : 
		Register hardware SPI
*********************************************/	

static inline uint8_t SPI4W_Write_Byte(uint8_t value)                                    
    {   
	uint8_t rxDat;
	spi_write_read_blocking(LCD_SPI_PORT, &value, &rxDat, 1);
    return rxDat;
    }

static inline uint8_t SPI4W_Read_Byte(uint8_t value)                                    
    {
	return SPI4W_Write_Byte(value);
    }

/********************************************************************************
function:	Delay function
note:
	Driver_Delay_ms(xms) : Delay x ms
	Driver_Delay_us(xus) : Delay x us
********************************************************************************/

static inline void Driver_Delay_ms(uint32_t xms)
    {
	sleep_ms(xms);
    }

static inline void Driver_Delay_us(uint32_t xus)
    {
    busy_wait_us_32 (xus);
    }

#endif
