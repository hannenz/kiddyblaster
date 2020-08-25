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
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "../bcm2835.h" 
#include "../mfrc522.h"
#include "../card.h"

extern Uid uid;
const char *path_to_uri(const char *_path);

// This is the default key for authentication
static MIFARE_Key auth_key = {{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }};


/**
 * Checks whether the main kiddyblaster service is running by checking its
 * pidfile at /var/rurn/kiddyblaster and test for this PID is currently running
 *
 * @return int          0 if not running, 1 if running
 */
int kiddyblaster_is_running() {
    FILE *runfile;
    pid_t pid;
    int n, retval = 0;

    runfile = fopen("/var/run/kiddyblaster", "r");
    if (runfile == NULL) {
        if (errno == ENOENT) {
            // File does not exist, so we assume the service is simply not running
            // No need to close the file
            return 1;
        }
        else {
            // Else we  have some sort of "hard error", no need to close the file
            fprintf(stderr, "Failed to open file /var/run/kiddyblaster\n");
            exit(-1);
        }
    }

    // Read PID from file and try to send a signal to it
    if (fscanf(runfile, "%u", &pid) == 1) {
        if (kill(pid, 0) == 0) {
            // Succeessfully sent a signal > process is running
            retval = 1;
        }
    }

    fclose(runfile);
    return retval;
}


/*
 * Verify that a path exists and is a directory
 *
 * @param const char* 		uri  Ptah to check
 * @return int 				success
 */
int verify_path(const char *uri) {
	struct stat sb;

	return (stat(uri, &sb) == 0 && S_ISDIR(sb.st_mode));
}


/**
 * write a card and associate with given name and uri
 *
 * @param const char* name
 * @param const char *uri
 * @return int      Success / 0 if card has been written, 1 if not (aborted, or path at URI does not exist)
 */
int write_card(const char *name, const char *_uri) {
    int card_id, i;
    /* char *uri = (char*)path_to_uri(_uri); */
    const char *uri;

	if (!verify_path(_uri)) {
		printf("Path at URI '%s' does not exist or is not a directory\n", uri);
		return 1;
	}

    uri = path_to_uri(_uri);

    printf("About to write:\nname = %s\nuri  = %s\n\n", name, uri);
    printf("Waiting for card - hold a card near the reader or press CTRL+c to abort\n");

    for(i = 0; i < 1000 ; i++) {

        /* gpioDelay(500000); */
        bcm2835_delay(500);

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
                return 1;
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


// do sth like str_replace('/home/pi/Music/', '', path)
const char *path_to_uri(const char *path) {

    /** TODO! Read this from /etc/mpd.conf **/
    const char *musicdir = "/home/pi/Music/";
    char *uri = malloc(strlen(path));
    int i;

    strncpy(uri, path, strlen(musicdir));
    if (strncmp(uri, musicdir, strlen(musicdir)) == 0) {
        printf("Match!\n");
        strncpy(uri, path + strlen(musicdir), strlen(path) - strlen(musicdir));
    }
    else {
        printf("No match!\n");
        uri = (char*)path;
    }

    // trim trailing slashes from uri (which is important for MPD to recognize
    // the path correctly)
    for (i = strlen(uri) - 1; i > 0 && uri[i] == '/'; i--) {
        uri[i] = '\0';
    }

    printf("URI derived from path: %s\n", uri);

    return uri;
}



static void usage() {
    puts("\nUsage: sudo writecard name uri\n");
	puts("name          Name of the card");
    puts("uri           Path to the directory relative");
    puts("              to `/home/pi/Music`, __without__ leading or");
    puts("              trailing slashes, e.g. `Audiobooks/Das Dschungelbuch`\n");
	puts("NOTE: writecard must be run with root privileges\n");
}



int main(int argc, char **argv) {
    const char *uri, *name;

    if (argc != 3) {
        usage();
        return -1;
    }


    if (kiddyblaster_is_running()) {
        fprintf(stderr, "Kiddyblaster is still running. Stop it with `systemctl stop kiddyblaster.service` and try again\n");
        return -1;
    }


	if (getuid() != 0) {
		fprintf(stderr, "%s must be run as root!\n", argv[0]);
		return -1;
	}


    /* Init pigpio */
    /* if (gpioInitialise() < 0) { */
    /*     fprintf(stderr, "Failed to initialise pigpio\n"); */
    /*     return -1; */
    /* } */


    // Init MFRC522 (RFID card reader)
    // (which also initializes bcm2835 lib)
    mfrc522_init();
    mfrc522_pcd_init();

    name = argv[1];
    uri = argv[2];

    int ret = write_card(name, uri);
    if (ret == 0) {
        printf("Card has been written successfully. You should restart kiddyblaster now with `systemctl start kiddyblaster.service`\n");
    }


    /* gpioTerminate(); */
    bcm2835_close();

    return 0;
}
