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

    QueueData qd;

    lastReset = time_us_64();
    uint64_t resetDebounce = time_us_64();
    ResetStage resetStage = gpio_get(c_rstPin) ? ResetStage::Active : ResetStage::Inactive;

    while (!GetQuitFlag()) {
        bool resetActive = gpio_get(c_rstPin);

        // Enable the pulse debouncer if the reset pin changes state while standing asserted or deasserted
        if ((!resetActive && resetStage == ResetStage::Active)
            || (resetActive && resetStage == ResetStage::Inactive)) {
            resetStage = resetActive ? ResetStage::DebounceActive : ResetStage::DebounceInactive;
            resetDebounce = time_us_64() + 50000;
        }

        // Save reset event once properly debounced
        if (time_us_64() > resetDebounce
            && ((resetStage == ResetStage::DebounceInactive && !resetActive) || (resetStage == ResetStage::DebounceActive && resetActive))) {
            resetStage = resetActive ? ResetStage::Active : ResetStage::Inactive;
            lastReset = time_us_64();
            qd.operation = resetActive ? QueueOperation::P80ResetActive : QueueOperation::P80ResetCleared;
            qd.timestamp = lastReset;
            queue_try_add(list, &qd);
        }

        // If reset is active or debouncing, wait for it to go low before reading bus activity
        if (resetStage != ResetStage::Inactive)
            continue;

        // The readout from the FIFO should look a bit like this
        // |  A[15:8]  |  Don't care  |  A[7:0]  |  D[7:0]  |
        const uint32_t fullRead = Bus_FastRead_PinImage(pio0, pioMap.readerSm);
        const uint16_t addr = ((fullRead & 0xFF000000) >> 16) | ((fullRead & 0x0000FF00) >> 8);
        const uint8_t data = (fullRead & 0x000000FF);
        if (fullRead != 0 && (baseAddress == AllAddresses || addr == baseAddress)) {
            qd.operation = QueueOperation::P80Data;
            qd.timestamp = time_us_64() - lastReset;
            qd.address = addr;
            qd.data = data;
            qd.printToOled = (baseAddress != AllAddresses);
            queue_try_add(list, &qd);
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
    mutex_enter_blocking(&quitLock);
    bool _flag = quitLoop;
    mutex_exit(&quitLock);
    return _flag;
}

void Logic::SetQuitFlag(bool _flag)
{
    mutex_enter_blocking(&quitLock);
    quitLoop = _flag;
    mutex_exit(&quitLock);
}
