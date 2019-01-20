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
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include <mpd/client.h>
#include <mpd/connection.h>

#include <wiringPi.h>

#include "i2c_lcd.h"
#include "player.h"
#include "mfrc522.h"

typedef void (*sighandler_t)(int);



#define BUTTON_1_PIN 26 
#define BUTTON_2_PIN 4



unsigned long btn1_t0;
unsigned long btn2_t0;

extern Uid uid;


// Prototypes
void update_lcd();



void on_button_1_pressed() {
    unsigned long t1 = millis();
    if (t1 - btn1_t0 < 300) {
        return;
    }
    btn1_t0 = t1;
    printf("Button 1 has been pressed\n");
    syslog(LOG_NOTICE, "Button 1 has been pressed\n");
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

    //if (btn2_is_pressed) {
      //  return;
    // }

    // Debounce button
    t1 = millis();
    if (t1 - btn2_t0 < 700) {
        return;
    }
    btn2_t0 = t1;

    // btn2_is_pressed = TRUE;

    // Wait for button being released
    while (digitalRead(BUTTON_2_PIN) == LOW) {
        delay(100);
    }

    // btn2_is_pressed = FALSE;

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



/**
 * Update the LCD display
 */
void update_lcd() {
    struct mpd_connection *mpd;
    char str[17], *states[] = { "??", "..", "|>", "||" };
    int n, m;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "Failed to connect to mpd\n");
        return;
    }

    struct mpd_status *status = mpd_run_status(mpd);
    if (status == NULL) {
        syslog(LOG_ERR, "Failed to get mpd status\n");
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



static sighandler_t handle_signal(int sig_nr, sighandler_t signalhandler) {
    struct sigaction new_sig, old_sig;

    new_sig.sa_handler = signalhandler;
    sigemptyset(&new_sig.sa_mask);
    new_sig.sa_flags = SA_RESTART;
    if (sigaction(sig_nr, &new_sig, &old_sig) < 0) {
        return SIG_ERR;
    }
    return old_sig.sa_handler;
}



static void start_daemon(const char *log_name, int facility) {
    int i;
    pid_t pid;

    // Terminate parent process, creating a widow which will be handled by init
    if ((pid = fork()) != 0) {
        exit (EXIT_FAILURE);
    }

    // child process becomes session leader
    if (setsid() < 0) {
        printf("%s can not become session leader\n", log_name);
        exit(EXIT_FAILURE);
    }

    // Ignore SIGHUP
    handle_signal(SIGHUP, SIG_IGN);

    // Terminate the child
    if ((pid = fork()) != 0) {
        exit(EXIT_FAILURE);
    }

    chdir("/");
    umask(0);
    for (i = sysconf(_SC_OPEN_MAX); i > 0; i--) {
        close(i);
    }

    openlog(log_name, LOG_PID | LOG_CONS | LOG_NDELAY, facility);
}



int main() {

//    start_daemon("kiddyblasterd", LOG_LOCAL0);

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


    // Init MFRC522
    mfrc522_init();
    mfrc522_pcd_init();

    // Start an endless loop
    for (;;) {
        if (!mfrc522_picc_is_new_card_present()) {
            continue;
        }
        if (!mfrc522_picc_read_card_serial()) {
            continue;
        }

        int i;
        for (i = 0; i < uid.size; i++) {
            if (uid.uidByte[i] < 0x10) {
                printf(" 0");
                printf("%X", uid.uidByte[i]);
            }
            else {
                printf(" ");
                printf("%X", uid.uidByte[i]);
            }
        }
        printf("\n");


        delay(500);
    }

    syslog(LOG_NOTICE, "daemon has terminated\n");
    closelog();
    return EXIT_SUCCESS;
}
