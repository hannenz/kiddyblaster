#ifndef __PLAYER_H__
#define __PLAYER_H__

int player_get_current_song_nr();
void player_toggle();
void player_pause();
void player_next();
void player_previous();
void player_play_uri(const char *uri);

#endif
