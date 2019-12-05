const Mfrc522 = require("./node_modules/mfrc522-rpi/index.js");
const SoftSPI = require("rpi-softspi");

const loopInterval = 500;

/**
 * Constructor
 *
 * @param int 		Timeout in sec., default: 60
 */
var CardReaderWriter = function(timeout = 60) {

    var self = this;
	var timeout = timeout;

    const softSPI = new SoftSPI({
        clock: 23,
        mosi: 19,
        miso: 21,
        client: 24
    });

    this.mfrc522 = new Mfrc522(softSPI).setResetPin(22).setBuzzerPin(18);
};


CardReaderWriter.prototype.sleep = function(ms) {
	return new Promise(resolve => setTimeout(resolve, ms));
}


/**
 * Read
 * 
 * @return Promise 		Returns a Promise which resolves to the read card's id
 */
CardReaderWriter.prototype.read = async function() {

	var self = this;

	return new Promise(async function(resolve, reject) {

		var timeSpent = 0;

		(function loop() {

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
				reject("Authentication Error");
				return;
			}

			//# Dump Block 8
			cardId = self.mfrc522.getDataForBlock(8)[0];
			console.log("Card-ID: " + cardId);
			clearTimeout(t);
			resolve(cardId);

			//# Stop
			self.mfrc522.stopCrypto();

			timeSpent += (loopInterval / 1000);
			if (timeSpent < self.timeout) {
				setTimeout(loop, loopInterval);
			}
			else {
				reject(`Timeout after ${timeSpent} seconds`);
			}
		})();
	});
};



/**
 * Write
 *
 * @param int 		The id to write to the card
 * @return Promise
 */
CardReaderWriter.prototype.write = async function(id) {
	return new Promise(async function(resolve, reject) {

		var timeSpent = 0;

		(function loop() {

			mfrc522.reset();

			let response = mfrc522.findCard();
			if (response.status) {
				console.log("Card detected, CardType: " + response.bitSize);

				response = mfrc522.getUid();
				if (!response.status) {
					reject("UID Scan Error");
					return;
				}

				const uid = response.data;

				const memoryCapacity = self.mfrc522.selectCard(uid);
				const key = [0xff, 0xff, 0xff, 0xff, 0xff, 0xff];

				if (!mfrc522.authenticate(8, key, uid)) {
					reject("Authentication Error");
					return;
				}

				let data = [
					id % 256,
					id / 256,
					0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff,
					0xff, 0xff, 0xff, 0xff
				];

				mfrc522.writeDataToBlock(8, data);
				mfrc522.stopCrypto();
				resolve();
			}
			timeSpent += (loopInterval / 1000);
			if (timeSpent < self.timeout) {
				setTimeout(loop, loopInterval);
			}
			else {
				reject(`Timeout after ${timeSpent} seconds`);
			}
		})();

	});
};

module.exports.CardReaderWriter = CardReaderWriter;
