/**
 * Kiddyblaster
 *
 * RFID driven Music box with raspberypi for children
 *
 * https://github.com/hannenz/kiddyblaster
 *
 * @author Johannes Braun <johannes.braun@hannenzn.de>
 * @package kiddyblaster
 * @version Sun Jan 20 10:04:57 UTC 2019
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpd/client.h>
#include <mpd/connection.h>
#include <wiringPi.h>
#include "i2c_lcd.h"
#include "player.h"

#define BUTTON_1_PIN 26 
#define BUTTON_2_PIN 4

unsigned long btn1_t0;
unsigned long btn2_t0;
unsigned long btn2_t1;

void update_lcd();



void on_button_1_pressed() {
    unsigned long t1 = millis();
    if (t1 - btn1_t0 < 300) {
        return;
    }
    btn1_t0 = t1;
    printf("Button 1 has been pressed\n");
    player_toggle();
    update_lcd();
}

int btn2_is_pressed = FALSE;


/**
 * Callback, on button 2 pressed
 *
 * Performs either a next / previous
 * depending on the press duration
 */
static void on_button_2_pressed() {
    unsigned long duration;
    unsigned long t1;
    int level;

    level = digitalRead(BUTTON_2_PIN);
    printf("%u\n", level);

    if (btn2_is_pressed) {
        return;
    }

    // Debounce button
    t1 = millis();
    if (t1 - btn2_t0 < 600) {
        return;
    }
    btn2_t0 = t1;

    btn2_is_pressed = TRUE;

    // Wait for button being released
    while (level == LOW) {
        delay(100);
        level = digitalRead(BUTTON_2_PIN);
    }

    btn2_is_pressed = FALSE;

    duration = millis() - t1;
    printf("D: %lu\n", duration);

    // Action depends on duration...
    if (duration > 5000) {
        printf("Self destruction sequence initiated!\n");
    }
    else if (duration > 700) {
        printf("<< PREV\n");
        player_previous();
    }
    else {
        printf(">> NEXT\n");
        player_next();
    }
    update_lcd();
}


void update_lcd() {
    struct mpd_connection *mpd;
    char str[17], *states[] = { "??", "..", "|>", "||" };
    int n, m;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "Failed to connect to mpd\n");
        return;
    }

    struct mpd_status *status = mpd_run_status(mpd);
    if (status == NULL) {
        fprintf(stderr, "Failed to get mpd status\n");
        return;
    }

    int state = mpd_status_get_state(status);

    n = player_get_current_song_nr() + 1;
    m = mpd_status_get_queue_length(status);
//    unsigned int elapsed = mpd_status_get_elapsed_time(status);
    snprintf(str, 17, "%02u/%02u       [%2s]", n, m, states[state]);
    lcd_clear();
    lcd_puts(str);

    mpd_status_free(status);
    mpd_connection_free(mpd);
}



int main() {

    // Setup WiringPi Lib
    wiringPiSetupGpio();

    pinMode(BUTTON_1_PIN, INPUT);
    pinMode(BUTTON_2_PIN, INPUT);

    // Register callbacks on button press
    wiringPiISR(BUTTON_1_PIN, INT_EDGE_FALLING, on_button_1_pressed);
    wiringPiISR(BUTTON_2_PIN, INT_EDGE_FALLING, on_button_2_pressed);

    // Init LCD
    lcd_init();
    update_lcd();

    // Start an endless loop
    for (;;) {
        delay(500);
    }

    return 0;
}
