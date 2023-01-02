#ifndef PICOPOST_COMMON_HPP
#define PICOPOST_COMMON_HPP

#include <stdint.h>

typedef enum __picopost_common_phykeys {
    KE_None = 0x00,
    
    KE_Up = 0x01,
    KE_Down = 0x02,
    KE_Select = 0x04,
    KE_Back = 0x08,
} Key;

/**
 * @brief A list of the available applications
 * 
 */
typedef enum __picopost_common_progselect {
    PS_Port80Reader, ///< Reads port 80h and outputs data
    PS_VoltageMonitor, ///< Monitors the 5V and 12V rails

    // More stuff coming soon?
    PS_Info,

    PS_MAX_PROG
} ProgramSelect;

typedef enum __picopost_commmon_operation {
    QO_None,

    QO_Greetings,
    QO_Credits,

    QO_P80Data,
    QO_P80Reset,
    QO_Volts,

    // More stuff coming soon?
} QueueOperation;

typedef struct __attribute__((packed)) __picopost_common_queuedata {
    QueueOperation operation;    
    uint64_t timestamp;

    uint16_t address;
    uint8_t data;

    float volts5;
    float volts12;
} QueueData;

static const char creditsLine[] =
    "Powered by The Retro Web | HW, fireTwoOneNine | SW, TheRealZago ";

#endif // PICOPOST_COMMON_HPP