#include <stdio.h>
#include <mpd/client.h>
#include <mpd/connection.h>
#include <syslog.h>
#include "player.h"

int player_get_current_song_nr() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "Failed to connect to mpd\n");
        return -1;
    }

    struct mpd_status *status = mpd_run_status(mpd);
    if (status == NULL) {
        syslog(LOG_ERR, "Failed to get mpd status\n");
        return -1;
    }

    int pos = mpd_status_get_song_pos(status);
    
    mpd_status_free(status);
    mpd_connection_free(mpd);

    return pos;
}


void player_toggle() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "Failed to connect to mpd\n");
        return;
    }

    mpd_run_toggle_pause(mpd);

    mpd_connection_free(mpd);
}

void player_next() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "Failed to connect to mpd\n");
        return;
    }

    mpd_run_next(mpd);

    mpd_connection_free(mpd);

}

void player_previous() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "Failed to connect to mpd\n");
        return;
    }

    mpd_run_previous(mpd);

    mpd_connection_free(mpd);
}


