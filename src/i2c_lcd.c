#include <pigpio.h>
#include "i2c_lcd.h"

int fd;

/*
 * Store content of LCD locally to
 * reprint with backlight off
 */
char content[2][16];

int backlight = 0x08;



void lcd_set_backlight(int bl) {
    backlight = bl ? 0x08 : 0x00;

    // reprint
    lcd_puts(LCD_LINE_1, content[0]);
    lcd_puts(LCD_LINE_2, content[1]);
}


void lcd_toggle_enable(int bits) {
	// Toggle enable pin on LCD display
    gpioDelay(500);
    i2cReadByteData(fd, bits | ENABLE);
    gpioDelay(500);
    i2cReadByteData(fd, bits & ~ENABLE);
    gpioDelay(500);
}


// Send byte to data pins
// @param int bits      The data
// @param int mode      1 = data, 0 = command

void lcd_byte(int bits, int mode) {
	
    int bits_hi, bits_lo;

    bits_hi = mode | (bits & 0xf0) | backlight;
    bits_lo = mode | ((bits << 4) & 0xf0) | backlight;

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
 *
 * @param int           Line (0x80 or 0xc0), use constants LCD_LINE_1 ...
 * @param const char*   The string to print
 */
void lcd_puts(int line, const char *str) {
    int i;
    char ch;
    lcd_loc(line);
    for (i = 0; i < 16 && *str; i++) {
        // Translate Umlaute from UTF-8 to LCD charset codes
        if (*str == 0xc3) {
            str++;
            switch (*str) {
                case 0xa4:
                case 0x84:
                    ch = 0b11100001;
                    break;
                case 0xb6:
                case 0x96:
                    ch = 0b11101111;
                    break;
                case 0xbc:
                case 0x9c:
                    ch = 0b11110101;
                    break;
                case 0x9f:
                    ch = 0b11100010;
                    break;
            }
        }
        else {
            ch = *str;
        }

		lcd_byte(ch, LCD_CHR);
        switch (line) {
            case LCD_LINE_1:
                content[0][i] = ch;
                break;
            case LCD_LINE_2:
                content[1][i] = ch;
                break;
        }
        str++;
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

void lcd_deinit() {
    i2cClose(fd);
}

