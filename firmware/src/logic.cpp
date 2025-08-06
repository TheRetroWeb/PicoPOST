#include "logic.hpp"

#include "hardware/pio.h"

#include "cfg/pins.h"
#include "common.hpp"
#include "fastread.pio.h"

#include <stdio.h>

static constexpr float IOR_CLKDIV { (float)REQ_CLOCK_KHZ / 183000 };

Logic::Logic()
{
    s_instance = this;
    SetQuitFlag(false);
    m_appRunning = false;
}

void Logic::Stop()
{
    while (m_appRunning) {
        SetQuitFlag(true);

        if (m_pioMap.readerSm != -1) {
            irq_set_enabled(m_pioMap.irq, false);
            pio_sm_set_enabled(m_pioMap.hwBase, m_pioMap.readerSm, false);
            pio_sm_clear_fifos(m_pioMap.hwBase, m_pioMap.readerSm);
            pio_sm_restart(m_pioMap.hwBase, m_pioMap.readerSm);
            pio_sm_unclaim(m_pioMap.hwBase, m_pioMap.readerSm);
            pio_remove_program(m_pioMap.hwBase, &Bus_FastRead_program, m_pioMap.readerOffset);
            m_pioMap.readerSm = -1;
            m_pioMap.readerOffset = 0;
        }
    }
}

void Logic::AddressReader(queue_t* list, bool newPcb, const uint16_t baseAddress)
{
    if (m_appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    m_appRunning = true;

    const int c_rstPin = newPcb ? PIN_ISA_RST_R6 : PIN_ISA_RST_R5;
    gpio_init(c_rstPin);
    gpio_set_dir(c_rstPin, GPIO_IN);
    gpio_pull_down(c_rstPin);

    // Configure PIO
    m_pioMap.hwBase = pio0;
    m_pioMap.readerOffset = pio_add_program(m_pioMap.hwBase, &Bus_FastRead_program);
    m_pioMap.readerSm = 0;
    pio_sm_claim(m_pioMap.hwBase, m_pioMap.readerSm);
    m_pioMap.irq = PIO0_IRQ_0;
    m_pioMap.filterAddress = baseAddress;

    pio_gpio_init(m_pioMap.hwBase, PIN_ADDRESS_BANK);
    pio_sm_set_consecutive_pindirs(m_pioMap.hwBase, m_pioMap.readerSm, PIN_ADDRESS_BANK, 1, true);
    pio_sm_set_consecutive_pindirs(m_pioMap.hwBase, m_pioMap.readerSm, PIN_ISA_D0, 16, false);
    pio_sm_config pioCfg = Bus_FastRead_program_get_default_config(m_pioMap.readerOffset);
    sm_config_set_sideset_pins(&pioCfg, PIN_ADDRESS_BANK);
    sm_config_set_in_pins(&pioCfg, PIN_ISA_D0);
    sm_config_set_in_shift(&pioCfg, true, true, 32);
    sm_config_set_fifo_join(&pioCfg, PIO_FIFO_JOIN_RX);
    if (IOR_CLKDIV > 1.f)
        sm_config_set_clkdiv(&pioCfg, IOR_CLKDIV);
    pio_sm_init(m_pioMap.hwBase, m_pioMap.readerSm, m_pioMap.readerOffset, &pioCfg);
    pio_sm_clear_fifos(m_pioMap.hwBase, m_pioMap.readerSm);

    irq_set_enabled(m_pioMap.irq, false);
    irq_set_priority(m_pioMap.irq, PICO_HIGHEST_IRQ_PRIORITY);
    pio_set_irq0_source_enabled(m_pioMap.hwBase, pis_sm0_rx_fifo_not_empty, true);
    irq_set_exclusive_handler(m_pioMap.irq, &Logic::AddressReaderISR);

    m_lastReset = time_us_64();

    // const uint32_t c_rstMask = (1UL << c_rstPin);

    // bool resetActive = gpio_get(c_rstPin);
    // bool resetLastActive = resetActive;
    // uint64_t resetTimer = timer_hw->timerawl;
    // bool resetDebounce = false;

    QueueData qd;
    qd.printToOled = (baseAddress != AllAddresses);

    // Start PIO
    pio_sm_set_enabled(m_pioMap.hwBase, m_pioMap.readerSm, true);
    irq_set_enabled(m_pioMap.irq, true);
    while (!GetQuitFlag()) {
        // resetActive = sio_hw->gpio_in & c_rstMask;
        // if (!resetDebounce) {
        //     // Enable the pulse debouncer if the reset pin changes state while standing asserted or deasserted
        //     if (resetActive != resetLastActive) {
        //         resetDebounce = true;
        //         resetTimer = timer_hw->timerawl + 50000;
        //         resetLastActive = resetActive;
        //         continue;
        //     }
        // } else {
        //     // Save reset event once properly debounced
        //     if (timer_hw->timerawl > resetTimer && resetActive == resetLastActive) {
        //         resetDebounce = false;
        //         m_lastReset = time_us_64();
        //         qd.operation = resetActive ? QueueOperation::P80ResetActive : QueueOperation::P80ResetCleared;
        //         qd.timestamp = m_lastReset;
        //         queue_add_blocking(list, &qd);
        //     } else {
        //         // If reset is active or debouncing, wait for it to go low before reading bus activity
        //         continue;
        //     }
        // }
        // resetLastActive = resetActive;

        // The readout from the FIFO should look a bit like this
        // |  A[15:8]  |  Don't care  |  A[7:0]  |  D[7:0]
        while (auto newData = m_ringBuffer.pop()) {
            const auto& entry = newData.value();
            qd.address = (entry.addrHi << 8 | entry.addrLo);
            qd.operation = QueueOperation::P80Data;
            qd.timestamp = time_us_64() - m_lastReset;
            qd.data = entry.data;
            queue_add_blocking(list, &qd);
        }

        // if (pio_sm_is_rx_fifo_full(m_pioMap.hwBase, m_pioMap.readerSm)) {
        //     panic("fifo full :(");
        // }
    }

    pio_set_irq0_source_enabled(m_pioMap.hwBase, pis_sm0_rx_fifo_not_empty, true);
    irq_remove_handler(m_pioMap.irq, &Logic::AddressReaderISR);
    gpio_deinit(c_rstPin);

    sleep_ms(100);
    m_appRunning = false;
}

void Logic::VoltageMonitor(queue_t* list)
{
    if (m_appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    m_appRunning = true;
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, true);

    if (m_volts == nullptr) {
        m_volts = std::make_unique<VoltMon>();
    }

    QueueData qd = {
        .operation = QueueOperation::Volts
    };
    m_lastReset = time_us_64();

    double readFive = 0.0;
    double readTwelve = 0.0;
    uint64_t readerDelay = time_us_64();

    while (!GetQuitFlag()) {
        if (time_us_64() >= readerDelay) {
            readFive = m_volts->Read5();
            readTwelve = m_volts->Read12();

            uint64_t tstamp = time_us_64() - m_lastReset;
            qd.timestamp = tstamp;
            qd.volts5 = static_cast<float>(readFive);
            qd.volts12 = static_cast<float>(readTwelve);
            queue_try_add(list, &qd);

            readerDelay = time_us_64() + 100000; // 100ms read delay
        } else {
            tight_loop_contents();
        }
    }

    gpio_put(PICO_SMPS_MODE_PIN, false);

    m_appRunning = false;
}

Logic* Logic::s_instance { nullptr };

void Logic::AddressReaderISR(void)
{
    AddressDecoding temp {};
    const auto& pioMap = s_instance->m_pioMap;
    while (!(pioMap.hwBase->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + pioMap.readerSm)))) {
        temp.raw = pioMap.hwBase->rxf[pioMap.readerSm];
        if (pioMap.filterAddress == AllAddresses || pioMap.filterAddress == (temp.addrHi << 8 | temp.addrLo))
            s_instance->m_ringBuffer.push(temp);
    }
    gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
    irq_clear(pioMap.irq);
}

bool Logic::GetQuitFlag()
{
    return m_quitLoop;
}

void Logic::SetQuitFlag(bool _flag)
{
    m_quitLoop = _flag;
}
