#ifndef __I2C_LCD_H__
#define __I2C_LCD_H__

#define I2C_BUS 1
#define I2C_ADDRESS 0x27 // I2C device address
#define LCD_CHR 1 // Mode - sending data
#define LCD_CMD 0 // Mode - sending command

#define LCD_LINE_1 0x80 
#define LCD_LINE_2 0xc0 

#define LCD_BACKLIGHT 0x08
#define ENABLE 0b00000100 // Enable bit



void lcd_init();
void lcd_clear();
void lcd_loc(int line);
void lcd_puts(int line, const char *str);
void lcd_toggle_enable(int bits);
void lcd_set_backlight(int backlight);
void lcd_refresh();

#endif
