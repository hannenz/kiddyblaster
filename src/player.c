#include <stdio.h>
#include <mpd/client.h>
#include <mpd/connection.h>
#include <syslog.h>
#include "player.h"


bool player_is_playing() {
    struct mpd_connection *mpd;
    struct mpd_status *status;
    int state;
	

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_is_playing(): Failed to connect to mpd\n");
        return -1;
    }

    status = mpd_run_status(mpd);
    if (status == NULL) {
        syslog(LOG_ERR, "Failed to get mpd status\n");
        return -1;
    }

    state = mpd_status_get_state(status);

    mpd_status_free(status);
    mpd_connection_free(mpd);

    return state == MPD_STATE_PLAY;
}


int player_get_current_song_nr() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_get_current_song_nr(): Failed to connect to mpd\n");
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
        syslog(LOG_ERR, "player_toggle(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_toggle_pause(mpd);

    mpd_connection_free(mpd);
}

void player_pause() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_pause(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_pause(mpd, true);

    mpd_connection_free(mpd);
}

void player_play() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_pause(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_play(mpd);

    mpd_connection_free(mpd);
}


void player_next() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_next(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_next(mpd);

    mpd_connection_free(mpd);

}

void player_previous() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_previous(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_previous(mpd);

    mpd_connection_free(mpd);
}


void player_play_uri(const char *uri) {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_play_uri(): Failed to connect to mpd\n");
        return;
    }

    syslog(LOG_NOTICE, "Stopping MPD\n");
    mpd_run_stop(mpd);
    syslog(LOG_NOTICE, "Clearing playlist\n");
    mpd_run_clear(mpd);
    syslog(LOG_NOTICE, "Adding URI to playlist: '%s'\n", uri);
    mpd_run_add(mpd, uri);
    syslog(LOG_NOTICE, "Playing playlist\n");
    mpd_run_play(mpd);

    mpd_connection_free(mpd);
}

void player_replay_playlist() {
    struct mpd_connection *mpd;

    mpd = mpd_connection_new("localhost", 6600, 0);
    if (mpd == NULL || mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
        syslog(LOG_ERR, "player_replay_playlist(): Failed to connect to mpd\n");
        return;
    }

    mpd_run_stop(mpd);
    mpd_run_play_pos(mpd, 0);

    mpd_connection_free(mpd);

}
