/*****************************************************************************
 * | File      	:	LCD_Touch.h
 * | Author      :   Waveshare team, Memotech-Bill
 * | Function    :	LCD Touch Pad Driver and Draw
 * | Info        :
 *   Image scanning
 *      Please use progressive scanning to generate images or fonts
 *----------------
 * |	This version:   V1.0
 * | Date        :   Jan 2025
 * | Info        :   PicoBB version
 *
 ******************************************************************************/
#ifndef __LCD_TOUCH_H_
#define __LCD_TOUCH_H_

#include "DEV_Config.h"
#include <stdbool.h>

/*******************************************************************************
function:
		2 times to read the touch screen IC, and the two can not exceed the deviation,
		ERR_RANGE, meet the conditions, then that the correct reading, otherwise the reading error.
parameter:
	Channel_Cmd :	pYCh_Adc = 0x90 :Read channel Y +
					pXCh_Adc = 0xd0 :Read channel x +
*******************************************************************************/
bool TP_Read_TwiceADC (uint16_t *pXCh_Adc, uint16_t *pYCh_Adc);

#endif
