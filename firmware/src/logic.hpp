/**
 * @file logic.hpp
 * @brief Each application available on the PicoPOST is defined here.
 *
 */

#ifndef PICOPOST_LOGIC_HPP
#define PICOPOST_LOGIC_HPP

#include "voltmon.hpp"

#include "hardware/pio.h"
#include "pico/util/queue.h"

#include <array>
#include <atomic>
#include <memory>
#include <optional>

class Logic {
public:
    static constexpr uint16_t AllAddresses { 0x0000 };

    Logic();

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
    void VoltageMonitor(queue_t* list);

private:
    enum class ResetStage : uint8_t {
        Inactive,
        DebounceActive,
        Active,
        DebounceInactive
    };

    struct PortReaderPIO {
        PIO hwBase;
        uint readerOffset { 0 };
        int readerSm { -1 };
        uint irq { 0 };
        uint16_t filterAddress {};
    };

    union AddressDecoding {
        uint32_t raw { 0 };
        struct {
            uint8_t dataCopy;
            uint8_t addrLo;
            uint8_t data;
            uint8_t addrHi;
        };
    };

    template <typename T, size_t Size>
    class RingBuffer {
        static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");

    public:
        // Insert an element. Returns false and drops the insertion if buffer is full.
        bool push(const T& item)
        {
            const size_t currWriteHead = writeHead.load(std::memory_order_relaxed);
            const size_t currReadHead = readHead.load(std::memory_order_acquire);

            if (full.load(std::memory_order_acquire)) {
                return false;
            }

            buffer[currWriteHead] = item;
            const size_t nextHead = increment(currWriteHead);
            writeHead.store(nextHead, std::memory_order_release);

            if (nextHead == currReadHead) {
                full.store(true, std::memory_order_release);
            }

            return true;
        }

        // Extract an element. Returns std::nullopt if buffer is empty.
        std::optional<T> pop()
        {
            const size_t currReadHead = readHead.load(std::memory_order_relaxed);
            const size_t currWriteHead = writeHead.load(std::memory_order_acquire);

            if (!full.load(std::memory_order_acquire) && currReadHead == currWriteHead) {
                return std::nullopt;
            }

            T item = buffer[currReadHead];
            const size_t nextTail = increment(currReadHead);
            readHead.store(nextTail, std::memory_order_release);
            full.store(false, std::memory_order_release);

            return item;
        }

    private:
        constexpr size_t increment(size_t index) const
        {
            return (index + 1) & (Size - 1); // Wrap-around
        }

        std::array<T, Size> buffer {};
        std::atomic<size_t> writeHead { 0 }; // Producer (ISR)
        std::atomic<size_t> readHead { 0 }; // Consumer (main)
        std::atomic<bool> full { false };
    };

    static Logic* s_instance;

    uint64_t m_lastReset { 0 };
    volatile bool m_appRunning { false };
    std::atomic<bool> m_quitLoop { false };
    PortReaderPIO m_pioMap {};
    std::unique_ptr<VoltMon> m_volts {};
    RingBuffer<AddressDecoding, QUEUE_DEPTH> m_ringBuffer {};

    static void AddressReaderISR(void);

    __force_inline bool GetQuitFlag();
    void SetQuitFlag(bool _flag);
};

#endif // PICOPOST_LOGIC_HPP