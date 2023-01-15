#include "gpioexp.hpp"

MCP23009::MCP23009(i2c_inst* bus, uint8_t addr)
{
    this->bus = bus;
    this->addr = addr;
}

bool MCP23009::IsConnected()
{
    uint8_t buffer;
    int rc = i2c_read_timeout_us(bus, addr, &buffer, sizeof(buffer), false, 50000);
    return (rc == sizeof(buffer));
}

void MCP23009::SetDirection(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_IODIR, mask);
}

void MCP23009::SetPolarity(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_IPOL, mask);
}

void MCP23009::SetInterruptSource(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_GPINTEN, mask);
}

void MCP23009::SetReferenceState(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_DEFVAL, mask);
}

void MCP23009::SetInterruptEvent(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_INTCON, mask);
}

void MCP23009::Config(bool seqOp, bool irqOpenDrain, bool irqPolarity, bool irqClear)
{
    uint8_t mask = 0x00;
    mask |= (seqOp << 5);
    mask |= (irqOpenDrain << 2);
    mask |= (irqPolarity << 1);
    mask |= irqClear;
    _writeRegister(Registers::MCPREG_IOCON, mask);
}

void MCP23009::SetPullUps(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_GPPU, mask);
}

uint8_t MCP23009::GetInterruptFlag()
{
    return _readRegister(Registers::MCPREG_INTF);
}

uint8_t MCP23009::GetInterruptCapture()
{
    return _readRegister(Registers::MCPREG_INTCAP);
}

bool MCP23009::Get(uint8_t pin)
{
    uint8_t mask = GetAll();
    return (mask & (1 << pin)) > 0;
}

uint8_t MCP23009::GetAll()
{
    return _readRegister(Registers::MCPREG_GPIO);
}

void MCP23009::Set(uint8_t pin, bool value)
{
    uint8_t mask = GetAll();
    if (value) {
        mask |= (1 << pin);
    } else {
        mask &= ~(1 << pin);
    }
    SetAll(mask);
}

void MCP23009::SetAll(uint8_t mask)
{
    _writeRegister(Registers::MCPREG_GPIO, mask);
}

uint8_t MCP23009::_readRegister(Registers reg)
{
    uint8_t buffer[2] = { reg, 0 };

    int rc = 0;
    int retry = 5;
    do {
        rc = 0;
        rc += i2c_write_timeout_us(bus, addr, buffer, 1, false, 50000);
        rc += i2c_read_timeout_us(bus, addr, buffer + 1, 1, false, 50000);
        if (rc != 2) {
            busy_wait_ms(1);
        }
    } while (rc != 2 && retry > 0);
    if (retry == 0) {
        panic("MCP23009: failed to read register %d", reg);
    }

    return buffer[1];
}

bool MCP23009::_writeRegister(Registers reg, uint8_t value)
{
    uint8_t buffer[2] = { reg, value };
    int rc = i2c_write_timeout_us(bus, addr, buffer, sizeof(buffer), false, 50000);
    if (rc == sizeof(buffer)) {
        return true;
    } else {
        panic("MCP23009: failed to write register %d", reg);
        return false;
    }
}