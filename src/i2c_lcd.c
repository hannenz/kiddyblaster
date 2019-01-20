#include <wiringPiI2C.h>
#include <wiringPi.h>
#include "i2c_lcd.h"

#define I2C_ADDRESS 0x27 // I2C device address
#define LCD_CHR 1 // Mode - sending data
#define LCD_CMD 0 // Mode - sending command

#define LINE_1 0x80 
#define LINE_2 0xc0 

#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100 // Enable bit

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


void lcd_clear() {
	lcd_byte(0x01, LCD_CMD);
	lcd_byte(0x02, LCD_CMD);
}

void lcd_puts(const char *str) {
	while (*str) {
		lcd_byte(*(str++), LCD_CHR);
	}
}


int lcd_init() {
	if (wiringPiSetup() == -1) {
		return -1;
	}

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
