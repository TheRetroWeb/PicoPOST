/**
 *    lib/voltmon/voltmon.cpp
 *
 *    Actually monitor volts
 *
 **/

#include "voltmon.hpp"

#include "pins.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"

VoltMon::VoltMon()
{
    adc_init();
    adc_gpio_init(PIN_5V_MON);
    adc_gpio_init(PIN_12V_MON);
}

double VoltMon::Read5() const
{
    return Raw5() * cfg_5V_factor;
}

double VoltMon::Read12() const
{
    return Raw12() * cfg_12V_factor;
}

uint16_t VoltMon::Raw5() const
{
    adc_select_input(ADC_5V_MON);
    return adc_read();
}

uint16_t VoltMon::Raw12() const
{
    adc_select_input(ADC_12V_MON);
    return adc_read();
}
