#include "logic.hpp"

#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/nvic.h"

#include "cfg/pins.h"
#include "common.hpp"
#include "fastread.pio.h"

#include <stdio.h>

#define PICOPOST_BUS_TIMING_DEBUG

void Logic::Prepare()
{
    SetQuitFlag(false);
    appRunning = false;
}

void Logic::Stop()
{
    while (appRunning) {
        SetQuitFlag(true);

        if (pioMap.readerSm != -1) {
            pio_sm_set_enabled(pio0, pioMap.readerSm, false);
            pio_sm_clear_fifos(pio0, pioMap.readerSm);
            pio_sm_restart(pio0, pioMap.readerSm);
            pio_sm_unclaim(pio0, pioMap.readerSm);
            pio_remove_program(pio0, &Bus_FastRead_program, pioMap.readerOffset);
            pioMap.readerSm = -1;
            pioMap.readerOffset = 0;
        }
    }
}

void Logic::AddressReader(queue_t* list, bool newPcb, const uint16_t baseAddress)
{
    if (appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    appRunning = true;

    const int c_rstPin = newPcb ? PIN_ISA_RST_R6 : PIN_ISA_RST_R5;
    gpio_init(c_rstPin);
    gpio_set_dir(c_rstPin, GPIO_IN);
    gpio_pull_down(c_rstPin);

    // Configure PIO
    pioMap.readerOffset = pio_add_program(pio0, &Bus_FastRead_program);
    pioMap.readerSm = pio_claim_unused_sm(pio0, true);

    pio_gpio_init(pio0, PIN_ADDRESS_BANK);
    pio_sm_set_consecutive_pindirs(pio0, pioMap.readerSm, PIN_ADDRESS_BANK, 1, true);
    pio_sm_set_consecutive_pindirs(pio0, pioMap.readerSm, PIN_ISA_D0, 17, false);
    pio_sm_config pioCfg = Bus_FastRead_program_get_default_config(pioMap.readerOffset);
    sm_config_set_sideset_pins(&pioCfg, PIN_ADDRESS_BANK);
    sm_config_set_in_pins(&pioCfg, PIN_ISA_D0);
    sm_config_set_in_shift(&pioCfg, true, true, 32);
    sm_config_set_fifo_join(&pioCfg, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&pioCfg, 1.8f);
    pio_sm_init(pio0, pioMap.readerSm, pioMap.readerOffset, &pioCfg);
    pio_sm_clear_fifos(pio0, pioMap.readerSm);

    // Setup DMA
    const uint c_dmaCh = 0;
    const uint c_dmaTriggerMask = 1UL << c_dmaCh;
    volatile uint32_t dmaCapture = 0;
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    dma_channel_claim(c_dmaCh);
    dma_channel_config dmaCfg = dma_channel_get_default_config(c_dmaCh);
    channel_config_set_read_increment(&dmaCfg, false);
    channel_config_set_write_increment(&dmaCfg, true);
    channel_config_set_dreq(&dmaCfg, pio_get_dreq(pio0, pioMap.readerSm, false));
    channel_config_set_irq_quiet(&dmaCfg, false);
    dma_channel_configure(c_dmaCh, &dmaCfg,
        &dmaCapture, // Destination pointer
        &pio0->rxf[pioMap.readerSm], // Source pointer
        1, // Number of transfers
        false // Start immediately
    );

    dma_channel_set_irq0_enabled(c_dmaCh, true);

    lastReset = time_us_64();

    const uint32_t c_rstMask = (1UL << c_rstPin);
    const uint32_t c_fifoMask = (1UL << (PIO_FSTAT_RXEMPTY_LSB + pioMap.readerSm));
    const uint16_t c_addrMask = baseAddress == AllAddresses ? 0x03FF : baseAddress;
#if defined(PICOPOST_BUS_TIMING_DEBUG)
    const uint32_t c_timingMask = (1UL << 26);
    gpio_init(26);
    gpio_set_dir(26, GPIO_OUT);
#endif

    bool resetActive = gpio_get(c_rstPin);
    bool resetLastActive = resetActive;
    uint64_t resetTimer = timer_hw->timerawl;
    bool resetDebounce = false;

    QueueData qd;
    qd.printToOled = (baseAddress != AllAddresses);

    // Start PIO & DMA
    dma_hw->ch[c_dmaCh].ctrl_trig |= DMA_CH0_CTRL_TRIG_EN_BITS;
    pio_sm_set_enabled(pio0, pioMap.readerSm, true);

    while (!GetQuitFlag()) {
        // Retrigger DMA at each round
        dma_hw->ch[c_dmaCh].ctrl_trig |= DMA_CH0_CTRL_TRIG_EN_BITS;

        resetActive = sio_hw->gpio_in & c_rstMask;
        if (!resetDebounce) {
            // Enable the pulse debouncer if the reset pin changes state while standing asserted or deasserted
            if (resetActive != resetLastActive) {
                resetDebounce = true;
                resetTimer = timer_hw->timerawl + 50000;
                resetLastActive = resetActive;
                continue;
            }
        } else {
            // Save reset event once properly debounced
            if (timer_hw->timerawl > resetTimer && resetActive == resetLastActive) {
                resetDebounce = false;
                lastReset = time_us_64();
                qd.operation = resetActive ? QueueOperation::P80ResetActive : QueueOperation::P80ResetCleared;
                qd.timestamp = lastReset;
                queue_add_blocking(list, &qd);
            } else {
                // If reset is active or debouncing, wait for it to go low before reading bus activity
                continue;
            }
        }
        resetLastActive = resetActive;

        // If DMA IRQ is set, continue parsing
        if (!(dma_hw->ints0 & c_dmaTriggerMask))
            continue;

#if defined(PICOPOST_BUS_TIMING_DEBUG)
        sio_hw->gpio_togl = c_timingMask;
#endif

        const auto readPtr = reinterpret_cast<volatile uint8_t*>(&dmaCapture);

        // The readout from the FIFO should look a bit like this
        // |  A[15:8]  |  Don't care  |  A[7:0]  |  D[7:0]
        const uint16_t addr = static_cast<uint16_t>((readPtr[3] << 8) | (readPtr[1]));
        const uint8_t data = readPtr[0];        
        printf("add %x %x ", addr, data);
        if (addr & c_addrMask) {
            qd.operation = QueueOperation::P80Data;
            qd.timestamp = time_us_64() - lastReset;
            qd.address = addr;
            qd.data = data;
            putchar('*');
            queue_add_blocking(list, &qd);
        }
        putchar('\n');

        dma_hw->ints0 = c_dmaTriggerMask;
        dmaCapture = 0;
    }

    gpio_deinit(c_rstPin);
    dma_channel_abort(c_dmaCh);
    dma_channel_cleanup(c_dmaCh);
    dma_channel_unclaim(c_dmaCh);

    sleep_ms(100);
    appRunning = false;
}

void Logic::VoltageMonitor(queue_t* list)
{
    if (appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    appRunning = true;
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, true);

    if (volts == nullptr) {
        volts = std::make_unique<VoltMon>();
    }

    QueueData qd = {
        .operation = QueueOperation::Volts
    };
    lastReset = time_us_64();

    double readFive = 0.0;
    double readTwelve = 0.0;
    uint64_t readerDelay = time_us_64();

    while (!GetQuitFlag()) {
        if (time_us_64() >= readerDelay) {
            readFive = volts->Read5();
            readTwelve = volts->Read12();

            uint64_t tstamp = time_us_64() - lastReset;
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

    appRunning = false;
}

bool Logic::GetQuitFlag()
{
#if defined(PICOPOST_LOGIC_USE_MUTEX)
    mutex_enter_blocking(&quitLock);
    bool _flag = quitLoop;
    mutex_exit(&quitLock);
    return _flag;
#else
    return quitLoop;
#endif
}

void Logic::SetQuitFlag(bool _flag)
{
#if defined(PICOPOST_LOGIC_USE_MUTEX)
    mutex_enter_blocking(&quitLock);
#endif
    quitLoop = _flag;
#if defined(PICOPOST_LOGIC_USE_MUTEX)
    mutex_exit(&quitLock);
#endif
}
