/**
 *    lib/voltmon/voltmon.c
 *
 *    Actually monitor volts
 *
 **/

#include "voltmon.h"

#include "calib.h"
#include "pins.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"

void VoltMon_Init()
{
    adc_init();
    adc_gpio_init(PIN_5V_MON);
    adc_gpio_init(PIN_12V_MON);
}

float VoltMon_Read5()
{
    adc_select_input(ADC_5V_MON);
    return adc_read() * FACTOR_5V * CALIBADJ_5V;
}

float VoltMon_Read12()
{
    adc_select_input(ADC_12V_MON);
    return adc_read() * FACTOR_12V * CALIBADJ_12V;
}