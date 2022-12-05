/**
 *    src/main.h
 *
 *    Some handy info for running the main application
 *
 **/

#ifndef __PICOPOST_SRC_MAIN
#define __PICOPOST_SRC_MAIN

// Serial is slow, but I hope we can make do without filling up 9.5 kBytes of
// bus data in less than we can empty the serial buffer at 115200 bps
#define MAX_QUEUE_LENGTH 512

#define DISPLAY_TEXT_BUFFER 10

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
    QO_Volts,

    // More stuff coming soon?
} QueueOperation;

typedef struct __attribute__((packed)) __picopost_main_queuedata {
    QueueOperation operation;    
    uint64_t timestamp;

    uint16_t address;
    uint8_t data;

    float volts;
} QueueData;

/**
 * @brief Reads I/O port 80h and pushes data to the reader queue.
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

/**
 * @brief Uses the ADC to probe the 5V and 12V supply rails
 *
 * @par
 * Simply reads 5V and 12V monitoring pins for the ADC about every 100 ms, then
 * sends data to the queue for the serial port (or OLED) to display.
 *
 */
void Logic_VoltageMonitor();

#endif // __PICOPOST_SRC_MAIN