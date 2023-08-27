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

#if defined(PICOPOST_RESET_HDLR)
        if (pioMap.resetSm != -1) {
            pio_sm_set_enabled(pio1, pioMap.resetSm, false);
            pio_sm_clear_fifos(pio1, pioMap.resetSm);
            pio_sm_restart(pio1, pioMap.resetSm);
            pio_sm_unclaim(pio1, pioMap.resetSm);
            pio_remove_program(pio1, &Bus_FastReset_program, pioMap.resetOffset);
            pioMap.resetSm = -1;
            pioMap.resetOffset = 0;
        }
#endif
    }
}

void Logic::AddressReader(queue_t* list, bool newPcb, const uint16_t baseAddress)
{
    if (appRunning) {
        panic("Someone forgot to initialize some stuff...");
    }

    appRunning = true;

#if defined(PICOPOST_RESET_HDLR)
    pioMap.resetOffset = pio_add_program(pio1, &Bus_FastReset_program);
    pioMap.resetSm = pio_claim_unused_sm(pio1, true);
    Bus_FastReset_init(pio1, pioMap.resetSm, pioMap.resetOffset, newPcb);
#endif

    pioMap.readerOffset = pio_add_program(pio0, &Bus_FastRead_program);
    pioMap.readerSm = pio_claim_unused_sm(pio0, true);
    Bus_FastRead_init(pio0, pioMap.readerSm, pioMap.readerOffset);

    uint8_t data = 0xFF;
    uint32_t fullRead = 0x0000;
    lastReset = time_us_64();
    QueueData qd;
    int lastResetSts = 0;

    // TODO Reset pulse detection is fucked up. Bypass for now
    while (!GetQuitFlag()) {
#if defined(PICOPOST_RESET_HDLR)
        // If reset is active, this read operation blocks the loop until reset
        // goes low. Returns true only if a proper reset pulse occurred.
        // Returns false if no reset event is happening
        auto resetPulse = Bus_FastReset_IsHigh(pio1, pioMap.resetSm);
        switch (resetPulse) {

        case 0: { // Reset is not asserted. Clear reset cycle (if needed) and read bus.
            if (lastResetSts != resetPulse) {
                lastResetSts = resetPulse;
                lastReset = time_us_64();
                qd.operation = QueueOperation::P80ResetCleared;
                queue_try_add(list, &qd);
            }
#endif

            // The readout from the FIFO should look a bit like this
            // |  A[7:0]  |  D[7:0]  |  A[15:8]  |  DC  |
            fullRead = Bus_FastRead_PinImage(pio0, pioMap.readerSm);
            uint16_t addr = (fullRead & 0x0000FF00) + ((fullRead & 0xFF000000) >> 24);
            if (addr == baseAddress) {
                data = (fullRead & 0x00FF0000) >> 16;
                qd.operation = QueueOperation::P80Data;
                qd.timestamp = time_us_64() - lastReset;
                qd.address = addr;
                qd.data = data;
                queue_try_add(list, &qd);
            }
#if defined(PICOPOST_RESET_HDLR)
        } break;

        case 1: { // Reset is asserted.
            if (lastResetSts != resetPulse) {
                lastResetSts = resetPulse;
                qd.operation = QueueOperation::P80ResetActive;
                queue_try_add(list, &qd);
            }
        } break;

        default: {
            // dunno
        } break;
        }
#endif
    }
/*
    if (pioMap.readerSm != -1) {
        pio_sm_set_enabled(pio0, pioMap.readerSm, false);
        pio_sm_restart(pio0, pioMap.readerSm);
        pio_sm_unclaim(pio0, pioMap.readerSm);
        pio_remove_program(pio0, &Bus_FastRead_program, pioMap.readerOffset);
        pioMap.readerSm = -1;
    }

#if defined(PICOPOST_RESET_HDLR)
    if (pioMap.resetSm != -1) {
        pio_sm_set_enabled(pio1, pioMap.resetSm, false);
        pio_sm_restart(pio1, pioMap.resetSm);
        pio_sm_unclaim(pio1, pioMap.resetSm);
        pio_remove_program(pio1, &Bus_FastReset_program, pioMap.resetOffset);
        pioMap.resetSm = -1;
    }
#endif
*/

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
    double readNTwelve = 0.0;
    uint64_t readerDelay = time_us_64();

    while (!GetQuitFlag()) {
        if (time_us_64() >= readerDelay) {
            readFive = volts->Read5();
            readTwelve = volts->Read12();
            readNTwelve = volts->ReadN12();

            uint64_t tstamp = time_us_64() - lastReset;
            qd.timestamp = tstamp;
            qd.volts5 = static_cast<float>(readFive);
            qd.volts12 = static_cast<float>(readTwelve);
            qd.voltsN12 = static_cast<float>(readNTwelve);
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
