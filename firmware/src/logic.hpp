/**
 * @file logic.hpp
 * @brief Each application available on the PicoPOST is defined here.
 *
 */

#ifndef PICOPOST_LOGIC_HPP
#define PICOPOST_LOGIC_HPP

#include "voltmon.hpp"

#include "pico/mutex.h"
#include "pico/util/queue.h"
#include <memory>
#include <type_traits>

class Logic {
public:
    Logic()
    {
        mutex_init(&quitLock);
    }

    /**
     * @brief Initializes the required flags for correct loop execution.
     * 
     */
    void Prepare();

    /**
     * @brief Stops currently running program, by letting it terminate operations
     * gracefully.
     *
     */
    void Stop();

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
     * graphical and serial output.
     *
     * @param baseAddress Address to listen to. Default 80h, some systems output on
     * different ports.
     *
     */
    void AddressReader(queue_t* list, bool newPcb, const uint16_t baseAddress = 0x0080);

    /**
     * @brief Uses the ADC to probe the 5V and 12V supply rails
     *
     * @par
     * Simply reads 5V and 12V monitoring pins for the ADC about every 100 ms, then
     * sends data to the queue for the serial port (or OLED) to display.
     *
     */
    void VoltageMonitor(queue_t* list, bool newPcb);

    template<typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    static T MirrorBytes(T input)
    {
        T out {};
        size_t byte_len = sizeof(T);

        while (byte_len--) {
            out = (out << 1) | (input & 1);
            input >>= 1;
        }

        return out;
    }

private:
    struct PortReaderPIO {
        uint readerOffset { 0 };
        int readerSm { -1 };

#if defined(PICOPOST_RESET_HDLR)
        uint resetOffset { 0 };
        int resetSm { -1 };
#endif
    };

    uint64_t lastReset { 0 };
    volatile bool appRunning { false };
    volatile bool quitLoop { false };
    mutex_t quitLock {};

    PortReaderPIO pioMap {};
    std::unique_ptr<VoltMon> volts {};

    bool GetQuitFlag();
    void SetQuitFlag(bool _flag);
};

#endif // PICOPOST_LOGIC_HPP