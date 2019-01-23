#include <pigpio.h>
#include "i2c_lcd.h"

int fd;

void lcd_toggle_enable(int bits) {
	// Toggle enable pin on LCD display
    gpioDelay(500);
    i2cReadByteData(fd, bits | ENABLE);
    gpioDelay(500);
    i2cReadByteData(fd, bits & ~ENABLE);
    gpioDelay(500);
}



void lcd_byte(int bits, int mode) {
	// Send byte to data pin
	
    int bits_hi, bits_lo;

    bits_hi = mode | (bits & 0xf0) | LCD_BACKLIGHT;
    bits_lo = mode | ((bits << 4) & 0xf0) | LCD_BACKLIGHT;

    i2cReadByteData(fd, bits_hi);
    lcd_toggle_enable(bits_hi);
    i2cReadByteData(fd, bits_lo);
    lcd_toggle_enable(bits_lo);
}



/**
 * Clear the display
 */
void lcd_clear() {
	lcd_byte(0x01, LCD_CMD);
	lcd_byte(0x02, LCD_CMD);
}



/**
 * Locate crsr at line
 */
void lcd_loc(int line) {
    lcd_byte(line, LCD_CMD);
}



/**
 * Output string on LCD display
 */
void lcd_puts(const char *str) {
    int i;
    for (i = 0; i < 16 && *str; i++) {
		lcd_byte(*(str++), LCD_CHR);
	}
}


/**
 * Initialize LCD display
 */
void lcd_init() {

    fd = i2cOpen(I2C_BUS, I2C_ADDRESS, 0);

    // Init LCD
    int i;
    unsigned const char init_sequence[] = {
        0x33,
        0x32,
        0x06,
        0x0c,
        0x28,
        0x01
    };
    for (i = 0; i < 6; i++) {
        lcd_byte(init_sequence[i], LCD_CMD);
    }
    gpioDelay(500);
}
