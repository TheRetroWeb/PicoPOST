/**
 *    lib/voltmon/voltmon.cpp
 *
 *    Actually monitor volts
 *
 **/

#include "voltmon.hpp"

#include "hardware/adc.h"
#include "hardware/gpio.h"

VoltMon::VoltMon()
{
    adc_init();
}

void VoltMon::RegisterSource(const Source src, uint pin, uint ch)
{
    m_sources[src] = { pin, ch };
    adc_gpio_init(pin);
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
    if (auto iter = m_sources.find(Source::VOLT_5VP); iter != m_sources.end()) {
        const auto& cp = iter->second;
        adc_select_input(cp.ch);
        return adc_read();
    }
    return 0;
}

uint16_t VoltMon::Raw12() const
{
    if (auto iter = m_sources.find(Source::VOLT_12VP); iter != m_sources.end()) {
        const auto& cp = iter->second;
        adc_select_input(cp.ch);
        return adc_read();
    }
    return 0;
}
