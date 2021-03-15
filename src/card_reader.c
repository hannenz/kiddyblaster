#include <syslog.h>
#include <pigpio.h>
#include <stdio.h>

#include "mfrc522.h"
#include "card_reader.h"
#include "i2c_lcd.h"



extern Uid uid;
typedef void (*callback_t)();


// This is the default key for authentication
static MIFARE_Key auth_key = {{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }};



void card_reader_init() {
	mfrc522_init();
	mfrc522_pcd_init();
}



/**
 * Check for new cards.
 * This function is executed as a thread
 *
 */


void* read_cards(void *callback) {

    int card_id = 0;
	callback_t cb = callback;

    while(1) {
		gpioDelay(500 * 1000);

        // Check for a new rfid card
        if (!mfrc522_picc_is_new_card_present()) {
            continue;
        }

        if (!mfrc522_picc_read_card_serial()) {
            continue;
        }

        // Authenticate
        if(mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, 8, &auth_key, &uid) != STATUS_OK) {
            syslog(LOG_ERR, "Failed to authenticate\n");
            continue;
        }

        byte data[32];
        byte size = sizeof(data);
        // mfrc522_pcd_read_register_multi(8, sizeof(data), data, 1);
        mfrc522_mifare_read(8, data, &size);
        mfrc522_pcd_stop_crypto_1();
        syslog(LOG_NOTICE, "%02x %02x %02x %02x\n", data[0], data[1], data[2], data[3]);

        card_id = data[0] + 256 * data[1];

        cb(card_id);
    }
}

