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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h>
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
#include "network_info.h"
#include "browser.h"

/* typedef void (*sighandler_t)(int); */


#define BUTTON_1_PIN 23     // PLAY
#define BUTTON_2_PIN 26     // PREV
#define BUTTON_3_PIN 4      // NEXT

#define SLEEP_TIMER 60 * 60

// Timer NRs for different gpioSetTimer calls
enum {
    TIMER_NR_BACKLIGHT,
    TIMER_NR_CHECK_SONG,
    TIMER_NR_SLEEP
};

#define BACKLIGHT_OFF_TIMEOUT 60 * 1000

// Globals
int timer = 0;  // seconds 
int micros;     // dummy, unused but we need it to pass to gpioTime()
bool is_sleeping = false;
int current_song = 0;
bool running = true;
bool select_mode = false;
Browser *browser;

// The MPD music dir
#define MUSICDIR "/srv/audio"

// Prototypes
void update_lcd();
/* static void start_daemon(const char*, int); */
static void display_network_info();
/* static void read_directories(const char *path, int depth); */



static void backlight_off() {

    // disabled for the time being
    return;

    // Only switch off between 18:00 and 09:00
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if (timeinfo->tm_hour > 18 || timeinfo->tm_hour < 9) {
        lcd_set_backlight(false);
        // Turn off timer
        gpioSetTimerFunc(TIMER_NR_BACKLIGHT, BACKLIGHT_OFF_TIMEOUT, NULL);
    }
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
    bool do_update_lcd = true;

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
                    if (select_mode) {
                        select_mode = false;
                        const char *uri = browser_get_selected_directory(browser);
                        if (uri != NULL) {
                            syslog(LOG_NOTICE, "Playing URI: %s\n", uri);
                            player_play_uri(uri);
                            update_lcd();
                        }
                        else {
                            syslog(LOG_WARNING, "Failed to get selected path fromr file browser");
                        }
                    }
                    else {
						// If mpd is stopped, f.e. after reboot, toggle has no
						// effect, so we need to explicitly send PLAY
						if (!player_is_playing()) {
							syslog(LOG_NOTICE, "Calling player_play()");
							player_play();
						}
						else {
							syslog(LOG_NOTICE, "|| TOGGLE\n");
							player_toggle();
						}
                    }
                    break;

                case BUTTON_2_PIN:
                    if (select_mode) {
                        browser_previous(browser);
                    }
                    else {
                        syslog(LOG_NOTICE, "<< PREV\n");
                        player_previous();
                    }
                    break;

                case BUTTON_3_PIN:
                    if (select_mode) {
                        browser_next(browser);
                    }
                    else {
                        syslog(LOG_NOTICE, ">> NEXT\n");
                        player_next();
                    }
                    break;
            }
        }
        /*
         * Long button press (700 - 5000 ms)
         */
        else if (duration < 5000) {
            switch (pin) {
                case BUTTON_1_PIN:
                    // Re-play current playlist from start
                    player_replay_playlist();
                    break;

                case BUTTON_2_PIN:
                    // Re-init LCD
                    lcd_deinit();
                    lcd_init();
                    break;

                case BUTTON_3_PIN:
                    display_network_info();
                    do_update_lcd = false;
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
                    syslog(LOG_NOTICE, ".. SHUTDOWN\n");
                    lcd_clear();
                    lcd_puts(LCD_LINE_1, "Tschüß..!");
                    sync();
                    reboot(LINUX_REBOOT_CMD_POWER_OFF);
                    break;

                case BUTTON_2_PIN:
                    // restart the software 
                    // (we just exit, the application will
                    // be respawned by systemd)
                    syslog(LOG_NOTICE, "() RESTART\n");
                    running = false;
                    break;

                case BUTTON_3_PIN:
                    syslog(LOG_NOTICE, "Entering SELECT MODE\n");
                    select_mode = true;
                    browser_start_browsing(browser);
                    break;
            }
        }
 
        // Reset sleep timer
        reset_timer();

        // "wake up"
        //if (is_sleeping) {
            is_sleeping = false;
        //}

        if (do_update_lcd) {
            update_lcd();
        }
        lcd_set_backlight(true);
        gpioSetTimerFunc(TIMER_NR_BACKLIGHT, BACKLIGHT_OFF_TIMEOUT, &backlight_off);
    }
}



/**
 * Update the LCD display
 */
void update_lcd() {
    if (select_mode) {
        lcd_clear();

		const char *musicdir = MUSICDIR;
        const char *uri = browser_get_selected_directory(browser);
		if (uri != NULL) {
			syslog(LOG_NOTICE, "Selected: %s", uri);
			int n;
			for (n = 0; n < strlen(uri); n++) {
				if (uri[n] != musicdir[n]) {
					break;
				}
			}
            lcd_puts(LCD_LINE_1, (const char*)&uri[n]);
			lcd_puts(LCD_LINE_2, "SEL     UP    DN");
        }
    }
    else {
        struct mpd_connection *mpd;
        char str[17], states[] = { '?', '.', LCD_CHAR_PLAY, LCD_CHAR_PAUSE };
        int n, m;

        mpd = mpd_connection_new("localhost", 6600, 0);
        if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
            syslog(LOG_ERR, "update_lcd(): Failed to connect to mpd\n");
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
        char *title, *album;
        if (song) {
            title = (char*)mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			album = (char*)mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);

            n = mpd_status_get_song_pos(status) + 1;
            m = mpd_status_get_queue_length(status);

            mpd_status_free(status);

            snprintf(str, sizeof(str), "%c %02u/%02u         ", states[state], n, m);
            lcd_puts(LCD_LINE_1, str);

			/* TODO: Use album (or card name) if title is null */
            snprintf(str, sizeof(str), "%-16s", title != NULL ? title : album);
            lcd_puts(LCD_LINE_2, str);
        }

		mpd_connection_free(mpd);
    }
	return;

    /* wifi_info_t wifi_info; */
    /* get_wifi_info(&wifi_info); */
    /*  */
    /* int level = wifi_info.link_quality / 25; */
    /* if (level > 3) { */
    /*     level = 3; */
    /* } */
    /* lcd_loc(0x8f); */
    /* lcd_byte(level, LCD_CHR); */
}




static void display_network_info() {
    syslog(LOG_NOTICE, "Displaying network info");
    lcd_clear();
    wifi_info_t wifi_info;
    get_wifi_info(&wifi_info);
    char buf[17];
    snprintf(buf, sizeof(buf), "%03u,%03u,%03u", wifi_info.link_quality, wifi_info.signal_level, wifi_info.noise_level);
    syslog(LOG_NOTICE, "%s", buf);
    lcd_puts(LCD_LINE_1, buf);

    char *ip_addr = get_ip_address("wlan0");
    syslog(LOG_NOTICE, "%s", ip_addr);
    snprintf(buf, sizeof(buf), "%-16s", ip_addr);
    syslog(LOG_NOTICE, "%s", buf);
    lcd_puts(LCD_LINE_2, buf);
    free(ip_addr);
}



static void on_card_detected(int card_id) {

	char *message = NULL;

    Card *card = card_read(card_id);
    if (card != NULL) {
        syslog(LOG_NOTICE, "Card #%u has been detected: %s!\n", card_id, card->name);
        if (card->uri[strlen(card->uri) - 1] == '/') {
            card->uri[strlen(card->uri)] = '\0';
        }

		syslog(LOG_NOTICE, "Calling player_play_uri(%s)\n", card->uri);
        player_play_uri(card->uri);

        lcd_set_backlight(true);
		lcd_clear();
		if (asprintf(&message, "Spiele Karte #%u", card_id) >= 0) {
			lcd_puts(LCD_LINE_1, message);
			lcd_puts(LCD_LINE_2, card->name);
			free(message);
		}
    }
    else {
		lcd_clear();
		lcd_puts(LCD_LINE_1, "Keine Karte ge-");
		if (asprintf(&message, "funden mit #%u", card_id) > 0) {
			lcd_puts(LCD_LINE_2, message);
			free(message);
		}

		syslog(LOG_ERR, "No card found with id #%u\n", card_id);
    }
}





void clean_up() {
    syslog(LOG_INFO, "Cleaning up before exit");
    lcd_clear();
    lcd_puts(LCD_LINE_1, "***  BYE!  ***");
    lcd_set_backlight(false);
    player_pause();
    gpioTerminate();
    free(browser);
}


// Called on signal SIGTERM.
void on_sigterm(int signum) {
    syslog(LOG_INFO, "Caught SIGTERM, terminating main loop to exit");
    running = false;
}


int main(int argc, char **argv) {


    // Setup pigpio lib
    if (gpioInitialise() < 0) {
        syslog(LOG_ERR, "Failed to initialize GPIO\n");
        exit(-1);
    }


    // Catch SIGTERM 
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = on_sigterm;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGKILL, &action, NULL);
    sigaction(SIGHUP, &action, NULL);
    sigaction(SIGINT, &action, NULL);


    // Register clean-up function
    /* atexit(clean_up); */

    // Write own PID to /var/run
    pid_t pid = getpid();
    FILE *runfile = fopen("/var/run/kiddyblaster", "w");
    if (runfile == NULL) {
        syslog(LOG_ERR, "Failed to write /var/run/kiddyblaster");
    }
    fprintf(runfile, "%u", pid);
    fclose(runfile);


	// Handled in mpd.conf with `restore_paused "yes"`
    // player_pause();

    // Init LCD display
    lcd_init();
    /* update_lcd(); */
    lcd_clear();
    update_lcd();
    syslog(LOG_INFO, "*** KIDDYBLASTER STARTING UP ***");

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

    // Set glitch filter to software-debounce buttons
    gpioGlitchFilter(BUTTON_1_PIN, 100000);
    gpioGlitchFilter(BUTTON_2_PIN, 100000);
    gpioGlitchFilter(BUTTON_3_PIN, 100000);

    // Register callbacks on button press
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
	syslog(LOG_NOTICE, "Launching Card Reader Thread");
    card_reader = gpioStartThread(read_cards, &on_card_detected);

    browser = browser_new(MUSICDIR);
	if (browser == NULL) {
		syslog(LOG_WARNING, "Failed to create Browser object");
	}

	syslog(LOG_NOTICE, "player_is_playing(): %s", player_is_playing() ? "yes" : "no");

    // Start an endless loop
    while (running) {
        int now;
        /* gpioDelay(5000000); */
        gpioSleep(PI_TIME_RELATIVE, 5, 0);


        if (is_sleeping) {
            continue;
        }

        // Get current time
        if (gpioTime(PI_TIME_RELATIVE, &now, &micros) < 0) {
            syslog(LOG_ERR, "Failed to get time\n");
            continue;
        }

        update_lcd();

        /* int seconds_left = SLEEP_TIMER - (now - timer); */
        /* syslog(LOG_NOTICE, "%02u:%02u until sleep\n", seconds_left / 60, seconds_left % 60); */
        if (now - timer >= SLEEP_TIMER) {
            goto_sleep();
            timer = now;
        }
    }

    // Clean-up and terminate
    syslog(LOG_NOTICE, "Terminating\n");
    gpioStopThread(card_reader);
    clean_up();
    closelog();
    return 0;
}

