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

VoltMon::VoltMon(bool supportsN12) : useN12(supportsN12)
{
    adc_init();
    adc_gpio_init(PIN_5V_MON);
    adc_gpio_init(PIN_12V_MON);
    if (useN12) {
        adc_gpio_init(PIN_N12V_MON);
    }
}

double VoltMon::Read5() const
{
    adc_select_input(ADC_5V_MON);
    return adc_read() * cfg_factor5V * cfg_adjust5V;
}

double VoltMon::Read12() const
{
    adc_select_input(ADC_12V_MON);
    return adc_read() * cfg_factor12V * cfg_adjust12V;
}

double VoltMon::ReadN12() const
{
    if (useN12) {
        adc_select_input(ADC_N12V_MON);
        return adc_read() * cfg_factorN12V * cfg_adjustN12V;
    } else {
        return 0.0;
    }
}