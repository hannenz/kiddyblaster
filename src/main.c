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


#define PRG_NAME "kiddyblaster"
#define DAEMON_NAME "kiddyblasterd"

#define BUTTON_1_PIN 26 
#define BUTTON_2_PIN 4



extern Uid uid;



// Prototypes
void update_lcd();
static void start_daemon(const char*, int);



static void on_button_1_pressed() {

    unsigned long duration;
    static unsigned long button_pressed_timestamp;
    int level;

    delay(100);

    level = digitalRead(BUTTON_1_PIN);

    if (level == LOW) {
        button_pressed_timestamp = millis();
    }
    else {
        duration = millis() - button_pressed_timestamp;
        button_pressed_timestamp = millis();

        if (duration < 100) {
            // debounce...
            return;
        }
        else if (duration < 700) {
            syslog(LOG_NOTICE, "|| TOGGLE\n");
            player_toggle();
        }
        else if (duration < 5000) {
            // Not implemented yet... later maybe "Card Write Mode"
        }
        else {
            //
        }

        update_lcd();
    }
}



/**
 * Callback, on button 2 pressed
 *
 * Performs either a next / previous
 * depending on the press duration
 */
static void on_button_2_pressed() {
    
    unsigned long duration;
    static unsigned long button_pressed_timestamp;
    int level;

    delay(100);

    level = digitalRead(BUTTON_2_PIN);

    if (level == LOW) {
        button_pressed_timestamp = millis();
    }
    else {
        duration = millis() - button_pressed_timestamp;
        button_pressed_timestamp = millis();

        if (duration < 100) {
            // debounce...
            return;
        }
        else if (duration < 700) {
            syslog(LOG_NOTICE, ">> NEXT\n");
            //printf( ">> NEXT\n");
            player_next();
        }
        else if (duration < 3000) {
            syslog(LOG_NOTICE, "<< PREV\n");
            //printf( "<< PREV\n");
            player_previous();
        }
        else {
            syslog(LOG_NOTICE, "!! RESTART\n");
            //printf( "!! RESTART\n");
 
            //systemd will respawn us!
            exit(0);
        }
        update_lcd();
    }
}



/**
 * Update the LCD display
 */
void update_lcd() {
    struct mpd_connection *mpd;
    char str[16], *states[] = { "??", "..", "|>", "||" };
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
    int song_id = mpd_status_get_song_id(status);

    struct mpd_song *song = mpd_run_get_queue_song_id(mpd, song_id);
    char *title;
    title = (char*)mpd_song_get_tag(song, MPD_TAG_TITLE, 0);

    // Truncate titles longer than 16 characters
    if (strlen(title) > 16) {
        title[13] = '.';
        title[14] = '.';
        title[15] = '\0';
    }

    n = player_get_current_song_nr() + 1;
    m = mpd_status_get_queue_length(status);
    snprintf(str, sizeof(str), "%02u/%02u       [%s]", n, m, states[state]);

    lcd_clear();
    lcd_loc(LCD_LINE_1);
    lcd_puts(str);
    lcd_loc(LCD_LINE_2);
    lcd_puts(title);

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
    syslog(LOG_NOTICE, "Daemon has been started\n");
}



int main() {
    int check;

    if (FALSE)
        start_daemon(DAEMON_NAME, LOG_LOCAL0);

    // Setup WiringPi Lib
    check = wiringPiSetupGpio();
    if (check != 0) {
        syslog(LOG_ERR, "Failed to initializ GPIO\n");
    }

    pinMode(BUTTON_1_PIN, INPUT);
    pinMode(BUTTON_2_PIN, INPUT);

    pullUpDnControl(BUTTON_1_PIN, PUD_UP);
    pullUpDnControl(BUTTON_2_PIN, PUD_UP);


    // Register callbacks on button press
    check = wiringPiISR(BUTTON_1_PIN, INT_EDGE_BOTH, &on_button_1_pressed);
    if (check != 0) {
        syslog(LOG_ERR, "Failed to register callback for button #1\n");
        //fprintf(stderr, "wiringPiISR() failed\n");
    }
    check = wiringPiISR(BUTTON_2_PIN, INT_EDGE_BOTH, &on_button_2_pressed);
    if (check != 0) {
        syslog(LOG_ERR, "Failed to register callback for button #2\n");
        //fprintf(stderr, "wiringPiISR() failed\n");
    }

    // Init LCD display
    lcd_init();
    update_lcd();


    // Init MFRC522 card reader
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
        char b[4], buffer[32] = "";
        for (i = 0; i < uid.size; i++) {
            if (uid.uidByte[i] < 0x10) {
                snprintf(b, 4, " 0%X", uid.uidByte[i]);
            }
            else {
                snprintf(b, 4, " %X", uid.uidByte[i]);
            }
            strncat(buffer, b, 32);
        }
        syslog(LOG_NOTICE, buffer);

        delay(500);
    }

    syslog(LOG_NOTICE, "Daemon has terminated\n");
    closelog();
    return 0;
}
