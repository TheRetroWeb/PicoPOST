#ifndef PICOPOST_COMMON_HPP
#define PICOPOST_COMMON_HPP

#include <cstdint>

static const uint16_t c_maxBmpPayload { 1024 };
static const uint8_t c_ui_iconSize { 8 };
static const uint8_t c_ui_yIconAlign { 127 - c_ui_iconSize };
static const uint8_t c_maxFrames { 10 };

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
    BusDump, ///< Output all IO writes
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
    QueueOperation operation { QueueOperation::None };
    uint64_t timestamp { 0 };

    uint16_t address { 0 };
    uint8_t data { 0 };

    float volts5 { 0.f };
    float volts12 { 0.f };
    float voltsN12 { 0.f };

    bool printToOled { true };
};

using Bitmap = uint8_t[c_maxBmpPayload];

struct Icon {
    const uint8_t width;
    const uint8_t height;
    const Bitmap image;
};

struct Sprite {
    const uint8_t width;
    const uint8_t height;
    const uint8_t frameCount;
    const uint16_t frameDurationMs;
    const Bitmap images[c_maxFrames];
};

static const char creditsLine[] = "Powered by The Retro Web | HW, fireTwoOneNine | SW, TheRealZago ";

#endif // PICOPOST_COMMON_HPP