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
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>

#include <mpd/client.h>
#include <mpd/connection.h>

#include <pigpio.h>

#include "i2c_lcd.h"
#include "player.h"
#include "card_reader.h"
#include "card.h"

typedef void (*sighandler_t)(int);


#define PRG_NAME "kiddyblaster"
#define DAEMON_NAME "kiddyblasterd"

#define BUTTON_1_PIN 23     // PLAY
#define BUTTON_2_PIN 26     // PREV
#define BUTTON_3_PIN 4      // NEXT

#define SLEEP_TIMER 30 * 60

// Timer NRs for different gpioSetTimer calls
enum {
    TIMER_NR_BACKLIGHT,
    TIMER_NR_CHECK_SONG,
    TIMER_NR_SLEEP
};

#define BACKLIGHT_OFF_TIMEOUT 15000

// Globals
int timer = 0;  // seconds 
int micros;     // dummy, unused but we need it to pass to gpioTime()
bool is_sleeping = false;
int current_song = 0;



// Prototypes
void update_lcd();
static void start_daemon(const char*, int);



static void backlight_off() {
    syslog(LOG_NOTICE, "CB: Backlight Off\n");
    lcd_set_backlight(false);
    // Turn off timer
    gpioSetTimerFunc(TIMER_NR_BACKLIGHT, BACKLIGHT_OFF_TIMEOUT, NULL);
}



/**
 * Callback which is called periodically to update
 * the LCD display in case a new track has started
 */
static void check_new_song() {
    // Only check, if player is actually playing...
    if (!player_is_playing()) {
        return;
    }

    int n = player_get_current_song_nr();
    if (n == current_song) {
        return;
    }

    current_song = n;
    update_lcd();
}



/**
 * Reset the sleep timeout timer
 */
static void reset_timer() {
    if (gpioTime(PI_TIME_RELATIVE, &timer, &micros) < 0) {
        syslog(LOG_ERR, "Failed to get time\n");
    }
}



/**
 * Go to sleep mode
 */
static void goto_sleep() {
    is_sleeping = true;
    syslog(LOG_NOTICE, "ZZ Going to sleep\n");
    lcd_clear();
    lcd_puts(LCD_LINE_1, "Gute Nacht  [ZZ]");
    lcd_set_backlight(false);
    player_pause();
}




static void on_button_pressed(int pin, int level, uint32_t tick) {
    uint32_t duration;
    static uint32_t t0;

    if (level == 0) {
        // button pressed, start counting ...
        t0 = tick;
    }
    else {
        // button has been released
        duration = (tick - t0) / 1000;
        t0 = tick;

        /*
         * Short button press (less than 700 ms)
         */
        if (duration < 700) {
            switch (pin) {
                case BUTTON_1_PIN:
                    syslog(LOG_NOTICE, "|| TOGGLE\n");
                    player_toggle();
                    break;

                case BUTTON_2_PIN:
                    syslog(LOG_NOTICE, "<< PREV\n");
                    player_previous();
                    break;

                case BUTTON_3_PIN:
                    syslog(LOG_NOTICE, ">> NEXT\n");
                    player_next();
                    break;
            }
        }
        /*
         * Long button press (700 - 5000 ms)
         */
        else if (duration < 5000) {
            switch (pin) {
                case BUTTON_1_PIN:
                    // Not implemented yet... later maybe "Card Write Mode"
                    break;

                case BUTTON_2_PIN:
                    // Not implemented yet... 
                    break;

                case BUTTON_3_PIN:
                    // Not implemented yet... 
                    break;
            }
        }
        /*
         * Really long press (> 5 s)
         */
        else {
            switch (pin) {
                case BUTTON_1_PIN:
                    // Power off the musicbox
                    /* syslog(LOG_NOTICE, ".. SHUTDOWN\n"); */
                    lcd_clear();
                    lcd_puts(LCD_LINE_1, "Tschüß..!");
                    /* sync(); */
                    /* reboot(LINUX_REBOOT_CMD_POWER_OFF); */
                    break;

                case BUTTON_2_PIN:
                    // restart the software 
                    // (we just exit, the application will
                    // be respawned by systemd)
                    syslog(LOG_NOTICE, "() RESTART\n");
                    exit(0);
                    break;

                case BUTTON_3_PIN:
                    // Not implemented yet...
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
        lcd_set_backlight(true);
        gpioSetTimerFunc(TIMER_NR_BACKLIGHT, BACKLIGHT_OFF_TIMEOUT, &backlight_off);
    }
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
    int song_id = mpd_status_get_song_id(status);

    struct mpd_song *song = mpd_run_get_queue_song_id(mpd, song_id);
    char *title;
    title = (char*)mpd_song_get_tag(song, MPD_TAG_TITLE, 0);

    // Truncate titles longer than 16 characters
    /* if (strlen(title) > 16) { */
    /*     title[16] = '\0'; */
    /* } */

    n = mpd_status_get_song_pos(status); //player_get_current_song_nr() + 1;


    m = mpd_status_get_queue_length(status);

    mpd_status_free(status);
    mpd_connection_free(mpd);

    snprintf(str, sizeof(str), "%02u/%02u       [%s]", n, m, states[state]);
    lcd_puts(LCD_LINE_1, str);

    snprintf(str, sizeof(str), "%-16s", title);
    lcd_puts(LCD_LINE_2, str);
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



static void on_card_detected(int card_id) {

    Card *card = card_read(card_id);
    if (card != NULL) {
        syslog(LOG_NOTICE, "Card #%u has been detected: %s!\n", card_id, card->name);
        if (card->uri[strlen(card->uri) - 1] == '/') {
            card->uri[strlen(card->uri)] = '\0';
        }

        player_play_uri(card->uri);
        update_lcd();
    }
    else {
        syslog(LOG_ERR, "No card found with id #%u\n", card_id);
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
    card_reader_init();

    // Setup buttons
    gpioSetMode(BUTTON_1_PIN, PI_INPUT);
    gpioSetMode(BUTTON_2_PIN, PI_INPUT);
    gpioSetMode(BUTTON_3_PIN, PI_INPUT);
    gpioSetPullUpDown(BUTTON_1_PIN, PI_PUD_UP);
    gpioSetPullUpDown(BUTTON_2_PIN, PI_PUD_UP);
    gpioSetPullUpDown(BUTTON_3_PIN, PI_PUD_UP);

    // Wait a second to avoid false button triggers
    gpioDelay(1000000);

    // Register callbacks on button press
    gpioGlitchFilter(BUTTON_1_PIN, 100000);
    gpioGlitchFilter(BUTTON_2_PIN, 100000);
    gpioGlitchFilter(BUTTON_3_PIN, 100000);

    gpioSetAlertFunc(BUTTON_1_PIN, &on_button_pressed);
    gpioSetAlertFunc(BUTTON_2_PIN, &on_button_pressed);
    gpioSetAlertFunc(BUTTON_3_PIN, &on_button_pressed);

    // Periodically check if a new song has started
    // and update the LCD
    gpioSetTimerFunc(TIMER_NR_CHECK_SONG, 5000, &check_new_song);

    // Switch backlight off after N seconds
    gpioSetTimerFunc(TIMER_NR_BACKLIGHT, BACKLIGHT_OFF_TIMEOUT, &backlight_off);

    // Setup sleep timer
    reset_timer();

    // Start an own thread for reading RFID cards
    pthread_t *card_reader;
    card_reader = gpioStartThread(read_cards, &on_card_detected);

    // Register clean-up function
    /* atexit(clean_up); */

    // Start an endless loop
    for (;;) {
        int now, seconds_left;
        /* gpioDelay(5000000); */
        gpioSleep(PI_TIME_RELATIVE, 5, 0);

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

    lcd_clear();
    lcd_puts(LCD_LINE_1, "Bye bye!");

    // Clean-up and terminate
    gpioStopThread(card_reader);
    closelog();

    return 0;
}

