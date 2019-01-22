#include <wiringPiI2C.h>
#include <wiringPi.h>
#include "i2c_lcd.h"

int fd;

void lcd_toggle_enable(int bits) {
	// Toggle enable pin on LCD display
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits | ENABLE));
	delayMicroseconds(500);
	wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
	delayMicroseconds(500);
}



void lcd_byte(int bits, int mode) {
	// Send byte to data pin
	
	int bits_high, bits_low;

	bits_high = mode | (bits & 0xf0) | LCD_BACKLIGHT;
	bits_low = mode | ((bits << 4) & 0xf0) | LCD_BACKLIGHT;

	wiringPiI2CReadReg8(fd, bits_high);
	lcd_toggle_enable(bits_high);

	wiringPiI2CReadReg8(fd, bits_low);
	lcd_toggle_enable(bits_low);
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
int lcd_init() {
    /*
	if (wiringPiSetup() == -1) {
		return -1;
	}
    */

	fd = wiringPiI2CSetup(I2C_ADDRESS);

	lcd_byte(0x33, LCD_CMD);
	lcd_byte(0x32, LCD_CMD);
	lcd_byte(0x06, LCD_CMD);
	lcd_byte(0x0c, LCD_CMD);
	lcd_byte(0x28, LCD_CMD);
	lcd_byte(0x01, LCD_CMD);
	delayMicroseconds(500);

	return 0;
}
