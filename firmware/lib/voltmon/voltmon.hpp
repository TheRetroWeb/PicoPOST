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

class VoltMon {
public:
    explicit VoltMon(bool supportsN12);

    double Read5() const;
    double Read12() const;
    double ReadN12() const;

private:
    const int cfg_adcVref { 3260 };

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
    const double cfg_factor5V { cfg_adcVref * 0.0004467254 };
    const double cfg_adjust5V { CALIBADJ_5V };

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
    const double cfg_factor12V { cfg_adcVref * 0.0012003581 };
    const double cfg_adjust12V { CALIBADJ_12V };

    /**
     * @brief Voltage divider coefficient for the -12V rail
     *
     * @par
     * Handling a negative voltage on an input that only expects positive values
     * can be a bit tricky.
     * 
     *
     * @par
     * As of the ATX Specification 2.1, the acceptable range for the -12V rail is
     * [10800 mV, 13200 mV]. You probably don't have anything running off the -12V
     * rail anyway, so not much to worry about :3
     *
     */
    const double cfg_factorN12V { cfg_adcVref * 0.0012003581 };
    const double cfg_adjustN12V { CALIBADJ_N12V };

    bool useN12 { false };
};

#endif // PICOPOST_LIB_VOLTMON