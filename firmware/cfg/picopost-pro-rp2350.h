// Board config file for overclocked Pico

// The below lines aren't just comments - they're directives to the Pico SDK cmake system
// pico_cmake_set PICO_PLATFORM        = rp2350
// pico_cmake_set PICO_CYW43_SUPPORTED = 0

#ifndef _BOARDS_PICOPOST_PRO_RP2350_FAST_H
#define _BOARDS_PICOPOST_PRO_RP2350_FAST_H

// Allow extra time for xosc to start.
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64

// Slower flash to assist restarts when flashing on the fly
#define PICO_FLASH_SPI_CLKDIV 4

#include "boards/pico.h"

#endif // _BOARDS_PICOPOST_PRO_RP2350_FAST_H
