/*****************************************************************************
 * | File      	:	DEV_Config.c
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
#include "DEV_Config.h"
#include "hardware/pwm.h"

/**
 * GPIO Mode
 **/
void DEV_GPIO_Mode(UWORD Pin, UWORD Mode)
    {
    gpio_init(Pin);
    if(Mode == 0 || Mode == GPIO_IN)
        {
        gpio_set_dir(Pin, GPIO_IN);
        }
    else
        {
        gpio_set_dir(Pin, GPIO_OUT);
        }
    }

void DEV_GPIO_Init(void)
    {
	DEV_GPIO_Mode(LCD_RST_PIN,GPIO_OUT);
    DEV_GPIO_Mode(LCD_DC_PIN, GPIO_OUT);
    DEV_GPIO_Mode(LCD_BKL_PIN, GPIO_OUT);
    DEV_GPIO_Mode(LCD_CS_PIN, GPIO_OUT);
    DEV_GPIO_Mode(TP_CS_PIN,GPIO_OUT);
    DEV_GPIO_Mode(TP_IRQ_PIN,GPIO_IN);
    DEV_GPIO_Mode(SD_CS_PIN,GPIO_OUT);
	gpio_set_pulls(TP_IRQ_PIN,true,false);

    DEV_Digital_Write(TP_CS_PIN, 1);
    DEV_Digital_Write(LCD_CS_PIN, 1);
    DEV_Digital_Write(LCD_BKL_PIN, 1);
    DEV_Digital_Write(SD_CS_PIN, 1);
    }

void PWM_SetValue (int value)
    {
    if (value < 0) value = 0;
    else if (value > 100) value = 100;
    pwm_set_chan_level (PWM_GPIO_SLICE_NUM (LCD_BKL_PIN), pwm_gpio_to_channel (LCD_BKL_PIN), 100 * value);
    }

void PWM_Init (void)
    {
    pwm_config cfg = pwm_get_default_config ();
    pwm_config_set_clkdiv_int (&cfg, 100);
    pwm_config_set_wrap (&cfg, 10000);
    pwm_init (PWM_GPIO_SLICE_NUM (LCD_BKL_PIN), &cfg, true);
    gpio_set_function (LCD_BKL_PIN, GPIO_FUNC_PWM);
    PWM_SetValue (100);
    }

/********************************************************************************
function:	System Init
note:
	Initialize the communication method
********************************************************************************/
uint8_t System_Init(void)
    {
	DEV_GPIO_Init();
    PWM_Init ();
	spi_init(LCD_SPI_PORT,4000000);
	gpio_set_function(LCD_CLK_PIN,GPIO_FUNC_SPI);
	gpio_set_function(LCD_MOSI_PIN,GPIO_FUNC_SPI);
	gpio_set_function(LCD_MISO_PIN,GPIO_FUNC_SPI);

    return 0;
    }
