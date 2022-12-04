/**
 *    lib/voltmon/voltmon.h
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

#ifndef __PICOPOST_SRC_VOLTMON
#define __PICOPOST_SRC_VOLTMON

#define ADC_VREF 3260

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
#define FACTOR_5V (ADC_VREF * 0.0004467254)

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
#define FACTOR_12V (ADC_VREF * 0.0012003581)

/**
 * @brief Initializes the ADC and the required inputs
 *
 */
void VoltMon_Init();

/**
 * @brief Get the ADC readout for the 5V rail
 *
 * @return (float) 5V rail readout, in millivolts
 */
float VoltMon_Read5();

/**
 * @brief Get the ADC readout for the 12V rail
 *
 * @return (float) 12V rail readout, in millivolts
 */
float VoltMon_Read12();

#endif // __PICOPOST_SRC_VOLTMON