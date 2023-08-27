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

VoltMon::VoltMon(bool supportsN12)
    : useN12(supportsN12)
{
    adc_init();
    adc_gpio_init(PIN_5V_MON);
    adc_gpio_init(PIN_12V_MON);
    if (useN12) {
        adc_gpio_init(PIN_N12V_MON_R6);
    }
}

double VoltMon::Read5() const
{
    return Raw5() * cfg_5V_factor;
}

double VoltMon::Read12() const
{
    return Raw12() * cfg_12V_factor;
}

double VoltMon::ReadN12() const
{
    const auto raw = RawN12();
    if (raw >= 0x0FFF)
        return 0.0;

#if defined(PICOPOST_NEGATIVE12_APPROX)
    return ((raw * cfg_N12V_slope) + cfg_N12V_offset) * cfg_N12V_adjust;
#else
    return raw * cfg_N12V_factor + cfg_N12V_offset;
#endif
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

uint16_t VoltMon::RawN12() const
{
    if (useN12) {
        adc_select_input(ADC_N12V_MON_R6);
        return adc_read();
    } else {
        return 0x0FFF;
    }
}
