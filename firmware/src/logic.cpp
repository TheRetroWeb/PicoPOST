#include "logic.hpp"

#include "hardware/pio.h"

#include "common.hpp"
#include "fastread.pio.h"

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

    pioMap.readerOffset = pio_add_program(pio0, &Bus_FastRead_program);
    pioMap.readerSm = pio_claim_unused_sm(pio0, true);
    Bus_FastRead_init(pio0, pioMap.readerSm, pioMap.readerOffset);

    lastReset = time_us_64();

    const uint32_t c_rstMask = (1UL << c_rstPin);
    const uint32_t c_fifoMask = (1UL << (PIO_FSTAT_RXEMPTY_LSB + pioMap.readerSm));
    const uint16_t c_addrMask = baseAddress == AllAddresses ? 0xFFFF : baseAddress;

    bool resetActive = gpio_get(c_rstPin);
    bool resetLastActive = resetActive;
    uint64_t resetTimer = timer_hw->timerawl;
    bool resetDebounce = false;

    QueueData qd;
    qd.printToOled = (baseAddress != AllAddresses);

    while (!GetQuitFlag()) {
#if defined(PICOPOST_SIMPLE_RESET_HDLR)
        if (sio_hw->gpio_in & c_rstMask)
            continue;
#else
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
#endif

        if (pio0->fstat & c_fifoMask)
            continue;

        const uint32_t fullRead = pio0->rxf[pioMap.readerSm];
        if (fullRead == 0)
            continue;

        // The readout from the FIFO should look a bit like this
        // |  A[15:8]  |  Don't care  |  A[7:0]  |  D[7:0]
        const auto readPtr = reinterpret_cast<const uint8_t*>(&fullRead);
        const uint16_t addr = static_cast<uint16_t>((readPtr[3] << 8) | (readPtr[1]));
        const uint8_t data = readPtr[0];
        if (addr & c_addrMask) {
            qd.operation = QueueOperation::P80Data;
            qd.timestamp = time_us_64() - lastReset;
            qd.address = addr;
            qd.data = data;
            queue_add_blocking(list, &qd);
            // TODO: Convert this routine to a DMA reader. Adding to queue is slow enough to stall the PIO
        }
    }

    gpio_deinit(c_rstPin);

    sleep_ms(100);
    appRunning = false;
}

void Logic::VoltageMonitor(queue_t* list, bool newPcb)
{
    if (appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    appRunning = true;
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, true);

    if (volts == nullptr) {
        volts = std::make_unique<VoltMon>(newPcb);
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
