#include "logic.hpp"

#include "hardware/pio.h"

#include "common.hpp"
#include "fastread.pio.h"
#include "voltmon.hpp"

static volatile uint64_t lastReset = 0;
static volatile bool quitLoop = false;

void Logic_Stop()
{
    quitLoop = true;
}

void Logic_Port80Reader(queue_t* list, bool newPcb, const uint16_t baseAddress)
{
    uint resetOffset = pio_add_program(pio1, &Bus_FastReset_program);
    uint resetSm = pio_claim_unused_sm(pio1, true);
    Bus_FastReset_init(pio1, resetSm, resetOffset, newPcb);

    uint readerOffset = pio_add_program(pio0, &Bus_FastRead_program);
    uint readerSm = pio_claim_unused_sm(pio0, true);
    Bus_FastRead_init(pio0, readerSm, readerOffset);

    uint8_t data = 0xFF;
    uint32_t fullRead = 0x0000;
    quitLoop = false;
    lastReset = time_us_64();
    QueueData qd;
    int lastResetSts = 0;
    while (!quitLoop) {
        // If reset is active, this read operation blocks the loop until reset
        // goes low. Returns true only if a proper reset pulse occurred.
        // Returns false if no reset event is happening
        int resetPulse = Bus_FastReset_PinImage(pio1, resetSm);
        switch (resetPulse) {

        case 0: { // Reset is not asserted. Clear reset cycle (if needed) and read bus.
            if (lastResetSts != resetPulse) {
                lastResetSts = resetPulse;
                lastReset = time_us_64();
                qd.operation = QO_P80ResetCleared;
                queue_try_add(list, &qd);
            }

            // The readout from the FIFO should look a bit like this
            // |  A[7:0]  |  D[7:0]  |  A[15:8]  |  DC  |
            fullRead = Bus_FastRead_PinImage(pio0, readerSm, &quitLoop);
            uint16_t addr = (fullRead & 0x0000FF00) + ((fullRead & 0xFF000000) >> 24);
            if (addr == baseAddress) {
                uint8_t data = (fullRead & 0x00FF0000) >> 16;
                qd.operation = QO_P80Data;
                qd.timestamp = time_us_64() - lastReset;
                qd.address = addr;
                qd.data = data;
                queue_try_add(list, &qd);
            }
        } break;

        case 1: { // Reset is asserted.
            if (lastResetSts != resetPulse) {
                lastResetSts = resetPulse;
                qd.operation = QO_P80ResetActive;
                queue_try_add(list, &qd);
            }
        } break;
        }
    }

    pio_sm_set_enabled(pio0, readerSm, false);
    pio_sm_restart(pio0, readerSm);
    pio_sm_unclaim(pio0, readerSm);
    pio_remove_program(pio0, &Bus_FastRead_program, readerOffset);

    pio_sm_set_enabled(pio1, resetSm, false);
    pio_sm_restart(pio1, resetSm);
    pio_sm_unclaim(pio1, resetSm);
    pio_remove_program(pio1, &Bus_FastReset_program, resetOffset);

    sleep_ms(100);
}

void Logic_VoltageMonitor(queue_t* list, bool newPcb)
{
    gpio_init(PICO_SMPS_MODE_PIN);
    gpio_set_dir(PICO_SMPS_MODE_PIN, GPIO_OUT);
    gpio_put(PICO_SMPS_MODE_PIN, true);

    VoltMon* voltObj = new VoltMon(newPcb);

    QueueData qd = {
        .operation = QO_Volts
    };
    quitLoop = false;
    lastReset = time_us_64();

    float readFive = 0.0, readTwelve = 0.0, readNTwelve = 0.0;

    while (!quitLoop) {
        readFive = voltObj->Read5();
        readTwelve = voltObj->Read12();
        readNTwelve = voltObj->ReadN12();

        uint64_t tstamp = time_us_64() - lastReset;
        qd.timestamp = tstamp;
        qd.volts5 = readFive / 1000;
        qd.volts12 = readTwelve / 1000;
        qd.voltsN12 = readNTwelve / 1000;
        queue_try_add(list, &qd);

        sleep_ms(100);
    }

    delete voltObj;

    gpio_put(PICO_SMPS_MODE_PIN, false);
}
