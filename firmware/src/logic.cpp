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
            irq_set_enabled(m_pioMap.pioIrq, false);
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

    // Configure PIO
    m_pioMap.hwBase = pio0;
    m_pioMap.readerOffset = pio_add_program(m_pioMap.hwBase, &Bus_FastRead_program);
    m_pioMap.readerSm = 0;
    pio_sm_claim(m_pioMap.hwBase, m_pioMap.readerSm);
    m_pioMap.pioIrq = PIO0_IRQ_0;
    m_pioMap.rstIrq = IO_IRQ_BANK0;
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

    irq_set_enabled(m_pioMap.pioIrq, false);
    irq_set_priority(m_pioMap.pioIrq, PICO_HIGHEST_IRQ_PRIORITY + 5);
    pio_set_irq0_source_enabled(m_pioMap.hwBase, pis_sm0_rx_fifo_not_empty, true);
    if (baseAddress == AllAddresses)
        irq_set_exclusive_handler(m_pioMap.pioIrq, &Logic::BusReaderNoFilterISR);
    else
        irq_set_exclusive_handler(m_pioMap.pioIrq, &Logic::BusReaderISR);

    m_lastReset = time_us_64();

    m_resetPin = newPcb ? PIN_ISA_RST_R6 : PIN_ISA_RST_R5;
    gpio_init(m_resetPin);
    gpio_set_dir(m_resetPin, GPIO_IN);
    gpio_pull_down(m_resetPin);

    irq_set_enabled(m_pioMap.rstIrq, false);
    irq_set_priority(m_pioMap.rstIrq, PICO_HIGHEST_IRQ_PRIORITY + 5);
    gpio_set_irq_enabled_with_callback(m_resetPin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &Logic::ResetPulseISR);
    pio_sm_set_enabled(m_pioMap.hwBase, m_pioMap.readerSm, true);
    irq_set_enabled(m_pioMap.pioIrq, true);
    QueueData qd;
    while (!GetQuitFlag()) {
        if (auto newData = m_ringBuffer.pop()) {
            const auto& entry = newData.value();
            if (entry.type == TimelineEntry::Type::Data) {
                qd.address = entry.busData.Address();
                qd.operation = QueueOperation::P80Data;
                qd.timestamp = time_us_64() - m_lastReset;
                qd.data = entry.busData.data;
            } else if (entry.type == TimelineEntry::Type::Reset) {
                m_lastReset = time_us_64();
                qd.address = 0;
                qd.data = 0;
                qd.operation = entry.resetEvent;
                qd.timestamp = m_lastReset;
            }
            queue_add_blocking(list, &qd);
        }
    }

    irq_set_enabled(IO_IRQ_BANK0, false);
    gpio_set_irq_enabled_with_callback(m_resetPin, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, false, nullptr);
    pio_set_irq0_source_enabled(m_pioMap.hwBase, pis_sm0_rx_fifo_not_empty, true);
    if (baseAddress == AllAddresses)
        irq_remove_handler(m_pioMap.pioIrq, &Logic::BusReaderNoFilterISR);
    else
        irq_remove_handler(m_pioMap.pioIrq, &Logic::BusReaderISR);
    gpio_deinit(m_resetPin);

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

void Logic::BusReaderISR(void)
{
    const auto& pioMap = s_instance->m_pioMap;
    AddressDecoding::TargetType temp {};
    while (!(pioMap.hwBase->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + pioMap.readerSm)))) {
        temp = AddressDecoding::ParseBusRead(pioMap.hwBase->rxf[pioMap.readerSm]);
        if (pioMap.filterAddress == temp.Address()) {
            gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
            s_instance->m_ringBuffer.push({
                .type = TimelineEntry::Type::Data,
                .busData = temp,
            });
        }
    }
    irq_clear(pioMap.pioIrq);
}

void Logic::BusReaderNoFilterISR(void)
{
    const auto& pioMap = s_instance->m_pioMap;
    AddressDecoding::TargetType temp {};
    while (!(pioMap.hwBase->fstat & (1u << (PIO_FSTAT_RXEMPTY_LSB + pioMap.readerSm)))) {
        temp = AddressDecoding::ParseBusRead(pioMap.hwBase->rxf[pioMap.readerSm]);
        s_instance->m_ringBuffer.push({
            .type = TimelineEntry::Type::Data,
            .busData = temp,
        });
        gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
    }
    irq_clear(pioMap.pioIrq);
}

void Logic::ResetPulseISR(uint gpio, uint32_t event_mask)
{
    if (gpio != s_instance->m_resetPin)
        return;

    if (event_mask & GPIO_IRQ_EDGE_RISE) {
        s_instance->m_ringBuffer.push({
            .type = TimelineEntry::Type::Reset,
            .resetEvent = QueueOperation::P80ResetActive,
        });
    } else if (event_mask & GPIO_IRQ_EDGE_FALL) {
        s_instance->m_ringBuffer.push({
            .type = TimelineEntry::Type::Reset,
            .resetEvent = QueueOperation::P80ResetCleared,
        });
    }
}

bool Logic::GetQuitFlag()
{
    return m_quitLoop;
}

void Logic::SetQuitFlag(bool _flag)
{
    m_quitLoop = _flag;
}
