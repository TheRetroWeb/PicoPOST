#ifndef PICOPOST_COMMON_HPP
#define PICOPOST_COMMON_HPP

#include <stdint.h>

enum Key {
    KE_None = 0x00,
    
    KE_Up = 0x01,
    KE_Down = 0x02,
    KE_Select = 0x04,
    KE_Back = 0x08,
};

/**
 * @brief A list of the available applications
 * 
 */
enum class ProgramSelect {
    Port80Reader, ///< Reads port 80h and outputs data
    Port90Reader, ///< IBM PS/2s output to 90h
    Port84Reader, ///< Early Compaq outputs to 84h
    Port300Reader, ///< Some EISA systems output to 300h
    Port378Reader, ///< Olivettis output to 378h. Can we capture LPT?
    VoltageMonitor, ///< Monitors the 5V and 12V rails

    Info,
    UpdateFW,

    MainMenu
};

enum class QueueOperation : uint8_t {
    None,

    Greetings,
    Credits,

    P80Data,
    P80ResetActive,
    P80ResetCleared,
    Volts,

    // More stuff coming soon?
};

struct __attribute__((packed)) QueueData {
    QueueOperation operation;    
    uint64_t timestamp;

    uint16_t address;
    uint8_t data;

    float volts5;
    float volts12;
    float voltsN12;
};

static const char creditsLine[] =
    "Powered by The Retro Web | HW, fireTwoOneNine | SW, TheRealZago ";

#endif // PICOPOST_COMMON_HPP