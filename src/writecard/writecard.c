/**
 * Standalone CLI utility to
 * write RFID cards
 *
 * Johannes Braun <johannes.braun@hannenz.de>
 * Sun Feb  3 15:46:40 UTC 2019
 *
 * Compile with
 * ```
 * gcc -o writecard src/writecard.c src/card.c src/mfrc522.c -lsqlite3 -lbcm2835 -lpigpio
 * ```
 */
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <pigpio.h>
#include "../mfrc522.h"
#include "../card.h"

extern Uid uid;

// This is the default key for authentication
static MIFARE_Key auth_key = {{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }};

int write_card(const char *name, const char *uri) {
    int card_id, i;

    printf("About to write:\nname = %s\nuri=%s\n\n", name, uri);
    printf("Waitng for card ... hold a card near the reader or press CTRL+c to abort\n");

    for(i = 0; i < 1000 ; i++) {

        gpioDelay(500000);

        printf(".");
        fflush(stdout);

        // Check for a new card
        if (!mfrc522_picc_is_new_card_present()) {
            continue;
        }

        if (!mfrc522_picc_read_card_serial()) {
            continue;
        }

        // Authenticate
        if (mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, 8, &auth_key, &uid) != STATUS_OK) {
            fprintf(stderr, "Failed to authenticate\n");
            continue;
        }

        printf("\n");

        // Get the card's id
        byte data[32];
        byte size = sizeof(data);
        mfrc522_mifare_read(8, data, &size);
        printf("%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);

        card_id = data[0] + 256 * data[1];

        // Check if we have this card in database
        Card *card;
        if ((card = card_read(card_id)) != NULL) {

            // If yes: Update database entry for this card with new uri; we are done,
            // no need to write anything to the card itself

            char answer;
            printf("This card already contains data:\nid=%u\nname=%s\nuri=%s\nProceed and overwrite this card? y/N?\n", card->id, card->name, card->uri);
            do {
                answer = getchar();
            } while (isspace(answer));
            if (answer != 'y' && answer != 'Y') {
                printf("Aborted.\n");
                return 0;
            }

            strncpy(card->name, name, sizeof(card->name));
            strncpy(card->uri, uri, sizeof(card->uri));
            card_write(card);
            printf("Card #%u has been updated\n", card->id);
            break;
        }
        else {
            // If no: Insert new database entry, get lastinsertid and write this to card
            card = malloc(sizeof(Card));
            card->id = 0;
            strncpy(card->name, name, 100);
            strncpy(card->uri, uri, 256);
            printf("Writing new card data to db\n");
            printf("New card|name: %s\n", card->name);
            printf("New card|uri : %s\n", card->uri);
            int new_id = card_write(card);
            if (new_id <= 0) {
                fprintf(stderr, "Something went wrong when trying to write new Card to database\n");
                break;
            }

            // Write new ID to card
            printf("Writing new ID #%u to card\n", new_id);
            byte data[] = {
                (new_id % 256), (new_id / 256), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };

            int r;
            if ((r = mfrc522_mifare_write(8, data, sizeof(data))) != STATUS_OK) {
                fprintf(stderr, "Write failed: %u! :-/\n", r);
            }
            mfrc522_pcd_stop_crypto_1();

            printf("Done.");
            break;
        }
    }

    return 0;
}

const char *path_to_uri(const char *path) {
    // do sth like str_replace('/home/pi/Music/', '', path)
    // Trim trailing slashes
    // Trim leading slashes

    const char *uri = malloc(512);

    return uri;
}



int main(int argc, char **argv) {
    const char *uri, *name;

    // Init pigpio
    if (gpioInitialise() < 0) {
        fprintf(stderr, "Failed to initialise pigpio\n");
        return -1;
    }

    // Init MFRC522 (RFID card reader)
    mfrc522_init();
    mfrc522_pcd_init();

    if (argc != 3) {
        fprintf(stderr, "Usage: %s name uri\n", argv[0]);
        return -1;
    }

    name = argv[1];
    uri = argv[2];

    write_card(name, uri);

    gpioTerminate();
    return 0;
}
