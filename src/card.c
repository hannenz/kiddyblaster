#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sqlite3.h>

#include "card.h"

#define DB_FILE "/home/pi/cards.sql"

static int callback(void *udata, int argc, char **argv, char **az_col_name) {
    Card *card = (Card*)udata;
    int i;
    for (i = 0; i < argc; i++) {
        syslog(LOG_NOTICE, "%s = %s\n", az_col_name[i], argv[i] ? argv[i] : "NULL");
        if (strcmp(az_col_name[i], "id") == 0) {
            card->id = atoi(argv[i]);
        }
        if (strcmp(az_col_name[i], "name") == 0) {
            strncpy(card->name, argv[i], 100);
        }
        if (strcmp(az_col_name[i], "uri") == 0) {
            strncpy(card->uri, argv[i], 256);
        }
    }
    return 0;
}

Card* card_read(int card_id) {
    char str[512], *error_message = 0;
    sqlite3 *db;
    int rc;
    Card *card = malloc(sizeof(Card));
    card->id = 0;
    card->name[0] = '\0';
    card->uri[0] = '\0';

    rc = sqlite3_open(DB_FILE, &db);
    if (rc) {
        syslog(LOG_ERR, "Failed to open sqlite database: %s\n", DB_FILE);
        sqlite3_close(db);
        return NULL;
    }

    snprintf(str, sizeof(str), "SELECT * FROM cards WHERE id=%u LIMIT 1", card_id);
    rc = sqlite3_exec(db, str, callback, card, &error_message);
    if (rc != SQLITE_OK) {
        syslog(LOG_ERR, "SQL error: %s\n", error_message);
        sqlite3_free(error_message);
    }

    sqlite3_close(db);

    if (card->id == -1 || card->id != card_id) {
        return NULL;
    }

    return card;
}
