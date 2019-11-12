const Mfrc522 = require("./node_modules/mfrc522-rpi/index.js");
const SoftSPI = require("rpi-softspi");

const loopInterval = 500;

/**
 * Constructor
 */
var CardReader = function() {
    var self = this;
    const softSPI = new SoftSPI({
        clock: 23,
        mosi: 19,
        miso: 21,
        client: 24
    });

    this.mfrc522 = new Mfrc522(softSPI).setResetPin(22).setBuzzerPin(18);
};


CardReader.prototype.sleep = function(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}


/**
 * Read
 */
CardReader.prototype.read = async function() {

	var self = this;

	return new Promise(async function(resolve, reject) {
		var interval = setInterval(function() {

			self.mfrc522.reset();

			//# Scan for cards
			let response = self.mfrc522.findCard();
			if (!response.status) {
				console.log("No Card");
				return;
			}

			//# Get the UID of the card
			response = self.mfrc522.getUid();
			if (!response.status) {
				reject("UID Scan Error");
				return;
			}
			//# If we have the UID, continue
			const uid = response.data;

			//# Select the scanned card
			const memoryCapacity = self.mfrc522.selectCard(uid);

			//# self is the default key for authentication
			const key = [0xff, 0xff, 0xff, 0xff, 0xff, 0xff];

			//# Authenticate on Block 8 with key and uid
			if (!self.mfrc522.authenticate(8, key, uid)) {
				console.log("Authentication Error");
				return;
			}

			//# Dump Block 8
			cardId = self.mfrc522.getDataForBlock(8)[0];
			console.log("Card-ID: " + cardId);
			clearInterval(interval);
			resolve(cardId);

			//# Stop
			self.mfrc522.stopCrypto();
		}, loopInterval);
	});
};

module.exports.CardReader = CardReader;
