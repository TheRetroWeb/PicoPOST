#include "ssd1306.h"

#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include <string.h>

int OLED_SendCmd(uint8_t _cmd)
{
    uint8_t buf[2] = { 0x80, _cmd };
    return i2c_write_blocking(i2c_default, (OLED_ADDR & SSD1306_WRITE_MODE), buf, 2, false);
}

int OLED_SendBuf(uint8_t* _buf, size_t _len)
{
    uint8_t* localBuf = malloc(_len + 1);
    if (localBuf != NULL) {
        for (size_t idx = 1; idx < _len + 1; idx++) {
            localBuf[idx] = _buf[idx - 1];
        }
        localBuf[0] = 0x40;
        int ret = i2c_write_blocking(i2c_default, (OLED_ADDR & SSD1306_WRITE_MODE), localBuf, _len + 1, false);
        free(localBuf);
        return ret;
    } else {
        return -4; // Out of memory
    }
}

void OLED_Init() {
    i2c_init(i2c_default, 400000); // 400 kHz, doable?
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // TODO: Some magic numbers down here vary depending on the display used.
    //      I'm using defaults from a known working example, YMMV.
    //      Feel free to edit some of them to match your specific display.
    //      BEFORE USING RANDOM NUMBER, REFER TO THE CONTROLLER DATASHEET!

    OLED_SendCmd(SSD1306_SET_DISP_OFF);

    OLED_SendCmd(SSD1306_SET_MEM_ADDR);
    OLED_SendCmd(0x00);

    OLED_SendCmd(SSD1306_SET_DISP_START_LINE | 0x00);

    OLED_SendCmd(SSD1306_SET_SEG_REMAP | 0x01);

    OLED_SendCmd(SSD1306_SET_MUX_RATIO);
    OLED_SendCmd(OLED_HEIGHT - 1);

    OLED_SendCmd(SSD1306_SET_COM_OUT_DIR | OLED_DRAW_MODE);

    OLED_SendCmd(SSD1306_SET_DISP_OFFSET);
    OLED_SendCmd(0x00);

    OLED_SendCmd(SSD1306_SET_COM_PIN_CFG);
    OLED_SendCmd(0x02);

    OLED_SendCmd(SSD1306_SET_DISP_CLK_DIV);
    OLED_SendCmd(0x80); // controller defaults

    OLED_SendCmd(SSD1306_SET_PRECHARGE);
    OLED_SendCmd(0xF1); // TODO: are we sure about this?

    OLED_SendCmd(SSD1306_SET_VCOM_DESEL);
    OLED_SendCmd(0x30); // 0.83xVcc

    OLED_SetContrast(0xFF); // TODO: Read from flash parameters

    OLED_SendCmd(SSD1306_SET_SHOW_RAM);

    OLED_SendCmd(SSD1306_SET_NORM);

    OLED_SendCmd(SSD1306_SET_CHARGE_PUMP);
    OLED_SendCmd(0x14);

    OLED_SendCmd(SSD1306_SET_SCROLL | 0x00);

    OLED_SendCmd(SSD1306_SET_DISP_ON);
}

void OLED_SetContrast(uint8_t _level) {
    OLED_SendCmd(SSD1306_SET_CONTRAST);
    OLED_SendCmd(_level);
}

void OLED_RenderPage(uint8_t* _buf, size_t _len, uint8_t _startCol, uint8_t _page) {
    OLED_SendCmd(SSD1306_SET_MEM_ADDR);
    OLED_SendCmd(0x02); // set page mode

    OLED_SendCmd(SSD1306_SET_PAGE | _page);

    OLED_SendCmd(SSD1306_SET_PAGE_COLADDR_LSB |  (_startCol & 0x0F));
    OLED_SendCmd(SSD1306_SET_PAGE_COLADDR_MSB | ((_startCol & 0xF0) >> 4) );

    OLED_SendBuf(_buf, _len);
}

void OLED_ClearScreen() {
    const size_t pageSize = (OLED_WIDTH * OLED_PAGE_HEIGHT) / 8;
    uint8_t* page = malloc(pageSize);
    memset(page, 0x00, pageSize);

    for (size_t i = 0; i < OLED_NUM_PAGES; i++)
        OLED_RenderPage(page, pageSize, 0, i);
    free(page);
}

void OLED_PrintLine(const char* _text, OLED_Font _font, uint8_t _line) {
    const size_t byteWidth = OLED_WIDTH / 8;
    const size_t pageSize = (byteWidth * OLED_PAGE_HEIGHT);
    size_t lineLen = strlen(_text);
    if (lineLen > 18) { // arbitrary max char per line
        lineLen = 18;
    }

    uint8_t* page = malloc(pageSize);
    for (size_t i = 0; i < lineLen; i++) {
        const char c = _text[i];
        if (c >= _font.startAscii && c <= _font.endAscii) {
            for (uint8_t j = 0; j < _font.height; j++) {
                memcpy(
                    page + j + (i * _font.width),
                    _font.bitmap + ((c - _font.startAscii) * _font.height) + j,
                    1
                );
            }
        }
    }
    OLED_RenderPage(page, pageSize, 0, _line % OLED_NUM_PAGES);
    free(page);
}

