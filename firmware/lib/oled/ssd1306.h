/**
  *    oled/ssd1306.h
  * 
  *    Generic definitions from SSD1306 datasheet
  * 
  */

#ifndef __PICOPOST_OLED_SSD1306
#define __PICOPOST_OLED_SSD1306

#include "common.h"

#define SSD1306_SET_PAGE_COLADDR_LSB _u(0x00) // | (col address LSB nibble 0~F)
#define SSD1306_SET_PAGE_COLADDR_MSB _u(0x10) // | (col address MSB nibble 0~F)
#define SSD1306_SET_MEM_ADDR        _u(0x20)
#define SSD1306_SET_COL_ADDR        _u(0x21)
#define SSD1306_SET_PAGE_ADDR       _u(0x22)
#define SSD1306_SET_HORIZ_SCROLL    _u(0x26)
#define SSD1306_SET_SCROLL          _u(0x2E)
#define SSD1306_SET_DISP_START_LINE _u(0x40) // | (row address 00~3F), Essentially row remap
#define SSD1306_SET_CONTRAST        _u(0x81)
#define SSD1306_SET_CHARGE_PUMP     _u(0x8D)
#define SSD1306_SET_SEG_REMAP       _u(0xA0)
#define SSD1306_SET_SHOW_RAM        _u(0xA4)
#define SSD1306_SET_ALL_ON          _u(0xA5)
#define SSD1306_SET_NORM            _u(0xA6)
#define SSD1306_SET_INVERTED        _u(0xA7)
#define SSD1306_SET_MUX_RATIO       _u(0xA8)
#define SSD1306_SET_DISP_OFF        _u(0xAE)
#define SSD1306_SET_DISP_ON         _u(0xAF)
#define SSD1306_SET_PAGE            _u(0xB0) // | (page index 0~7)
#define SSD1306_SET_COM_OUT_DIR     _u(0xC0)
#define SSD1306_SET_DISP_OFFSET     _u(0xD3)
#define SSD1306_SET_DISP_CLK_DIV    _u(0xD5)
#define SSD1306_SET_PRECHARGE       _u(0xD9)
#define SSD1306_SET_COM_PIN_CFG     _u(0xDA)
#define SSD1306_SET_VCOM_DESEL      _u(0xDB)

#define OLED_PAGE_HEIGHT            _u(8)
#define OLED_NUM_PAGES              (OLED_HEIGHT / OLED_PAGE_HEIGHT)
#define OLED_BUF_LEN                (OLED_NUM_PAGES * OLED_WIDTH)

#define SSD1306_WRITE_MODE           _u(0xFE)
#define SSD1306_READ_MODE            _u(0xFF)

#endif // __PICOPOST_OLED_SSD1306