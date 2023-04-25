/**
 * @brief GPIO expander library
 *
 * @par
 * In order to simplify cabling and native GPIO usage, slow or fixed inputs have
 * been moved to the remote control, and they're handled by an MCP23009 I2C
 * GPIO expander.
 *
 */

#ifndef PICOPOST_LIB_GPIOEXP
#define PICOPOST_LIB_GPIOEXP

#include "hardware/i2c.h"

class MCP23009 {
public:
    MCP23009(i2c_inst* bus, uint8_t addr)
        : bus(bus)
        , addr(addr)
    {
    }

    bool IsConnected();

    /**
     * @brief Set working direction for all GPIOs
     *
     * @param mask bitmask for setting pin direction. 0 for output, 1 for input
     */
    void SetDirection(uint8_t mask);

    /**
     * @brief Set working polarity for all GPIOs
     *
     * @param mask bitmask for setting pin polarity. 0 for default, 1 for inverted
     */
    void SetPolarity(uint8_t mask);

    /**
     * @brief Set whether an input will trigger an IRQ event.
     *
     * @param mask bitmask for setting IRQ source. 0 for disabled, 1 for enabled
     */
    void SetInterruptSource(uint8_t mask);

    /**
     * @brief Set default GPIO state. If a pin enabled for IRQ changes state and
     * assumes a value different from the default value, an IRQ event is raised.
     *
     * @param mask bitmask for setting default state. 0 for logic-low, 1 for logic-high
     */
    void SetReferenceState(uint8_t mask);

    /**
     * @brief Set IRQ event mode.
     *
     * @param mask bitmask for setting input IRQ mode. 0 for generic value change, 1 for reference state comparison
     */
    void SetInterruptEvent(uint8_t mask);

    /**
     * @brief Configure GPIO expander
     *
     * @param seqOp address auto-increment: false - enabled, true - disabled
     * @param irqOpenDrain INT pin output mode: false - active driver, true - open-drain
     * @param irqPolarity INT pin driver mode: false - active-low, true - active-high
     * @param irqClear INT event clear: false - read GPIO, true - read INTCAP
     */
    void Config(bool seqOp, bool irqOpenDrain, bool irqPolarity, bool irqClear);

    /**
     * @brief Set internal pin pull-up resistor.
     *
     * @param mask bitmask for setting pull-up. 0 for disabled, 1 for enabled
     */
    void SetPullUps(uint8_t mask);

    /**
     * @brief Reads which pins caused the interrupt event
     *
     * @return uint8_t bitmask
     */
    uint8_t GetInterruptFlag();

    /**
     * @brief Reads the state of all GPIOs when the interrupt occurred
     *
     * @return uint8_t bitmask with the GPIO status image
     */
    uint8_t GetInterruptCapture();

    /**
     * @brief Get current value for specified GPIO
     *
     * @param pin pin to read from
     * @return Current pin status
     */
    bool Get(uint8_t pin);

    /**
     * @brief Get current value for all GPIOs
     *
     * @return uint8_t GPIO mask
     */
    uint8_t GetAll();

    /**
     * @brief Set current value for specified GPIO
     *
     * @param pin pin to write to
     * @param value new pin status
     */
    void Set(uint8_t pin, bool value);

    /**
     * @brief Write GPIO image
     *
     * @param mask bitmask for setting pin. 0 for logic-low, 1 for logic-high
     */
    void SetAll(uint8_t mask);

private:
    enum Registers : uint8_t {
        MCPREG_IODIR = 0x00, // pin direction
        MCPREG_IPOL = 0x01, // input polarity
        MCPREG_GPINTEN = 0x02, // interrupt source config
        MCPREG_DEFVAL = 0x03, // interrupt trigger event config (default value)
        MCPREG_INTCON = 0x04, // interrupt trigger event config (default or change)
        MCPREG_IOCON = 0x05, // chip config
        MCPREG_GPPU = 0x06, // pin pull-up
        MCPREG_INTF = 0x07, // irq trigger
        MCPREG_INTCAP = 0x08, // gpio status @ irq
        MCPREG_GPIO = 0x09, // gpio status
        MCPREG_OLAT = 0x0A // output latches
    };

    i2c_inst* bus { nullptr };
    uint8_t addr { 0x00 };

    uint8_t _readRegister(Registers reg);
    bool _writeRegister(Registers reg, uint8_t value);
};

#endif // PICOPOST_LIB_GPIOEXP
