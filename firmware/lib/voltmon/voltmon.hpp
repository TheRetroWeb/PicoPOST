/**
 *    lib/voltmon/voltmon.hpp
 *
 *    Monitor volts. We're checking for +5 and +12V rails.
 *
 **/

/**
 * @brief Voltage monitoring library
 *
 * @par
 * The RP2040's ADC is a handy way to get some relatively decent voltage read-
 * outs. It has a few bugs and quirks, especially on the Raspebrry Pi Pico
 * board, which we have to take into account.
 *
 * @par
 * The PicoPOST uses a simple voltage divider and voltage clamp setup to avoid
 * sending straight 5V and 12V into the poor thing's input pins.
 * Below, you can find the magic numbers needed to convert the 12-bit ADC output
 * into more human-readable millivolts.
 *
 * @par
 * There aren't enough ADC pins for the whole 4 voltage rails of the ISA bus.
 * -5V and -12V are relegated to simple LEDs on the PCB.
 *
 */

#ifndef PICOPOST_LIB_VOLTMON
#define PICOPOST_LIB_VOLTMON

#include "calib.h"

#include <cstdint>

class VoltMon {
public:
    VoltMon();

    double Read5() const;
    double Read12() const;

    uint16_t Raw5() const;
    uint16_t Raw12() const;

private:
    static constexpr double cfg_adc_mVref { 3.260 };
    static constexpr double cfg_adc_mVStep { cfg_adc_mVref / 4096 };

    /**
     * @brief Voltage divider coefficient for the 5V rail
     *
     * @par
     * The Raspberry Pi Pico uses a 3.3V ADC reference voltage. When the ADC returns
     * the max value (0xFFF, 4095), it's equal to 3.3 V.
     * Except the actual reference voltage is 3.0V, due to an R-C filtering stage...
     *
     * @par
     * Considered the 39 kOhm and 47 kOhm divider, we can expect the maximum input
     * voltage at 3.3 V to be 6040 mV.
     *
     * @par
     * As of the ATX Specification 2.1, the acceptable range for the 5V rail is
     * [4750 mV, 5250 mV]. If your PSU is outputting 6 V, start worrying.
     *
     */
    static constexpr double cfg_5V_R1 { 39000.0 };
    static constexpr double cfg_5V_R2 { 47000.0 };
    static constexpr double cfg_5V_coefficient { (cfg_5V_R2 + cfg_5V_R1) / cfg_5V_R2 };
    static constexpr double cfg_5V_adjust { CALIBADJ_5V };
    static constexpr double cfg_5V_factor { (cfg_adc_mVStep * cfg_5V_coefficient) * cfg_5V_adjust };

    /**
     * @brief Voltage divider coefficient for the 12V rail
     *
     * @par
     * Considered the 47 kOhm and 12 kOhm divider, we can expect the maximum input
     * voltage at 3.3 V to be 16225 mV.
     *
     * @par
     * As of the ATX Specification 2.1, the acceptable range for the 12V rail is
     * [11400 mV, 12600 mV] normally and [10800 mV, 13200 mV] at peak rail load.
     * If your PSU is outputting 16 V, start worrying.
     *
     */
    static constexpr double cfg_12V_R1 { 47000.0 };
    static constexpr double cfg_12V_R2 { 12000.0 };
    static constexpr double cfg_12V_coefficient { (cfg_12V_R2 + cfg_12V_R1) / cfg_12V_R2 };
    static constexpr double cfg_12V_adjust { CALIBADJ_12V };
    static constexpr double cfg_12V_factor { (cfg_adc_mVStep * cfg_12V_coefficient) * cfg_12V_adjust };
};

#endif // PICOPOST_LIB_VOLTMON