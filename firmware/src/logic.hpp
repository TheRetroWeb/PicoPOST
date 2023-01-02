/**
 * @file logic.hpp
 * @brief Each application available on the PicoPOST is defined here.
 * 
 */

#ifndef PICOPOST_LOGIC_HPP
#define PICOPOST_LOGIC_HPP

#include "pico/util/queue.h"

/**
 * @brief Stops currently running program, by letting it terminate operations
 * gracefully.
 * 
 */
void Logic_Stop();

/**
 * @brief Reads I/O port and pushes data to the reader queue.
 *
 * @par
 * This is the primary target for this project.
 * To accomplish this task, one of the two PIO engines in the RP2040 is used
 * to listen for the Bus_Ready signal to come up, then it samples the address
 * and data busses in one shot. The read operation itself is so fast, the ISA
 * bus itself could be read 4 times in a single host transaction!
 * Data is then sent to the queue, so the second core can waste time for
 * printing it to USB CDC serial port. OLED output will be implemented in the
 * near future.
 * 
 * @param baseAddress Address to listen to. Default 80h, some systems output on
 * different ports.
 *
 */
void Logic_Port80Reader(queue_t* list, const uint16_t baseAddress = 0x0080);

/**
 * @brief Uses the ADC to probe the 5V and 12V supply rails
 *
 * @par
 * Simply reads 5V and 12V monitoring pins for the ADC about every 100 ms, then
 * sends data to the queue for the serial port (or OLED) to display.
 *
 */
void Logic_VoltageMonitor(queue_t* list);

#endif // PICOPOST_LOGIC_HPP