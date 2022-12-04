/**
 *    src/main.h
 *
 *    Some handy info for running the main application
 *
 **/

#ifndef __PICOPOST_SRC_MAIN
#define __PICOPOST_SRC_MAIN

// Serial is slow, but I hope we can make do without filling up 512 bytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH 512

typedef enum __picopost_main_progselect {
    PS_None,

    PS_Port80Reader,
    PS_VoltageMonitor,

    // More stuff coming soon?
} ProgramSelect;

typedef enum __picopost_main_operation {
    QO_None,

    QO_Data,
    QO_Reset,

    // More stuff coming soon?
} QueueOperation;

typedef struct __picopost_main_queuedata {
    QueueOperation operation;
    uint16_t address;
    uint8_t data;
    uint64_t timestamp;
} QueueData;

/**
 * @brief Reads I/O port 80h and returns data to the reader queue.
 *
 * @param pio The PIO instance to use for the read operation.
 *
 * @par
 * This is the primary target for this project.
 * To accomplish this task, one of the two PIO engines in the RP2040 is used
 * to listen for the Bus_Ready signal to come up, then it samples the address
 * and data busses in one shot. The read operation itself is so fast, the ISA
 * bus itself could be read 4 times in a single host transaction!
 * Data is then sent to the queue, so the second core can waste time for
 * printing it to USB CDC serial port. OLED output will be implemented in the
 * near future.
 *
 */
void Logic_Port80Reader();

#endif // __PICOPOST_SRC_MAIN