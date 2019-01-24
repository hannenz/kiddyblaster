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
#include <stdbool.h>
#include <sys/types.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include <mpd/client.h>
#include <mpd/connection.h>

#include <pigpio.h>

#include "i2c_lcd.h"
#include "player.h"
#include "mfrc522.h"

typedef void (*sighandler_t)(int);


#define PRG_NAME "kiddyblaster"
#define DAEMON_NAME "kiddyblasterd"

#define BUTTON_1_PIN 26 
#define BUTTON_2_PIN 4

#define SLEEP_TIMER 30 * 60

extern Uid uid;

int timer;  // seconds 
int micros;
bool is_sleeping = false;


// Prototypes
void update_lcd();
static void start_daemon(const char*, int);

static void reset_timer() {
    if (gpioTime(PI_TIME_RELATIVE, &timer, &micros) < 0) {
        syslog(LOG_ERR, "Failed to get time\n");
    }
}

static void goto_sleep() {
    is_sleeping = true;
    syslog(LOG_NOTICE, "ZZ Going to sleep\n");
    lcd_clear();
    lcd_loc(LCD_LINE_1);
    lcd_puts("Gute Nacht  [ZZ]");
    player_pause();
}




static void on_button_pressed(int pin, int level, uint32_t tick) {
    uint32_t duration[2];
    static uint32_t t0[2];

    if (level == 0) {
        //button has been pressed
        t0[pin] = tick;
    }
    else {
        // button has been released
        duration[pin] = (tick - t0[pin]) / 1000;
        t0[pin] = tick;

        if (duration[pin] < 700) {
            switch (pin) {
                case BUTTON_1_PIN:
                    syslog(LOG_NOTICE, "|| TOGGLE\n");
                    player_toggle();
                    break;
                case BUTTON_2_PIN:
                    syslog(LOG_NOTICE, ">> NEXT\n");
                    player_next();
                    break;
            }
        }
        else if (duration[pin] < 5000) {
            switch (pin) {
                case BUTTON_1_PIN:
                    // Not implemented yet... later maybe "Card Write Mode"
                    break;
                case BUTTON_2_PIN:
                    syslog(LOG_NOTICE, ">> PREV\n");
                    player_previous();
                    break;
            }
        }
        else {
            switch (pin) {
                case BUTTON_1_PIN:
                    break;
                case BUTTON_2_PIN:
                    syslog(LOG_NOTICE, "() RESTART\n");
                    exit(0);
                    break;
            }
        }
 
        // Reset sleep timer
        reset_timer();

        // "wake up"
        //if (is_sleeping) {
            is_sleeping = false;
        //}


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


void* read_cards(void *udata) {
    while(1) {
        // Check for a new rfid card
        if (!mfrc522_picc_is_new_card_present()) {
            continue;
        }

        if (!mfrc522_picc_read_card_serial()) {
            continue;
        }


        // This is the default key for authentication
        MIFARE_Key key = {{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }};
        
        // Authenticate
        if(mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, 8, &key, &uid) != STATUS_OK) {
            syslog(LOG_ERR, "Failed to authenticate\n");
            continue;
        }

        byte data[4];
        mfrc522_pcd_read_register_multi(8, sizeof(data), data, 1);
        mfrc522_pcd_stop_crypto_1();
        syslog(LOG_NOTICE, "%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);

        gpioDelay(500000);
    }
}

int main() {

    if (false)
        start_daemon(DAEMON_NAME, LOG_LOCAL0);

    // Setup WiringPi Lib

    if (gpioInitialise() < 0) {
        syslog(LOG_ERR, "Failed to initializ GPIO\n");
    }

    // Init LCD display
    lcd_init();
    update_lcd();

    // Init MFRC522 card reader
    mfrc522_init();
    mfrc522_pcd_init();

    // Setup buttons
    gpioSetMode(BUTTON_1_PIN, PI_INPUT);
    gpioSetMode(BUTTON_2_PIN, PI_INPUT);
    gpioSetPullUpDown(BUTTON_1_PIN, PI_PUD_UP);
    gpioSetPullUpDown(BUTTON_2_PIN, PI_PUD_UP);

    // Wait a second to avoid false button triggers
    gpioDelay(1000000);

    // Register callbacks on button press
    gpioGlitchFilter(BUTTON_1_PIN, 150000);
    gpioGlitchFilter(BUTTON_2_PIN, 150000);

    gpioSetAlertFunc(BUTTON_1_PIN, &on_button_pressed);
    gpioSetAlertFunc(BUTTON_2_PIN, &on_button_pressed);

    // Setup sleep timer
    reset_timer();

    // Start an own thread for reading RFID cards
    pthread_t *card_reader = gpioStartThread(read_cards, NULL);

    // Start an endless loop
    for (;;) {
        int now, seconds_left;
        //gpioDelay(500000);
        gpioSleep(PI_TIME_RELATIVE, 2, 0);

        if (is_sleeping) {
            continue;
        }
        if (gpioTime(PI_TIME_RELATIVE, &now, &micros) < 0) {
            syslog(LOG_ERR, "Failed to get time\n");
            continue;
        }

        seconds_left = SLEEP_TIMER - (now - timer);
        syslog(LOG_NOTICE, "%02u:%02u until sleep\n", seconds_left / 60, seconds_left % 60);
        if (now - timer >= SLEEP_TIMER) {
            goto_sleep();
            timer = now;
        }
    }

    // Clean up and terminate
    gpioStopThread(card_reader);
    syslog(LOG_NOTICE, "Daemon has terminated\n");
    closelog();
    return 0;
}
