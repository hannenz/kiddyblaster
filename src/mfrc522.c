/*
 * MFRC522.cpp - Library to use ARDUINO RFID MODULE KIT 13.56 MHZ WITH TAGS SPI W AND R BY COOQROBOT.
 * NOTE: Please also check the comments in MFRC522.h - they provide useful hints and background information.
 * Released into the public domain.
 *
 *
 * Johannes Braun 2021-03-12:
 * Added conditional compilation to use
 * [pigpio library](http://abyz.me.uk/rpi/pigpio/index.html) instead of bcm2835
 * Define LIB_PIGPIO to compile with pigpio library instead of bcm2835.
 * Define either here with `#define LIB_PIGPIO` or in Makefile with
 * `CPPFLAGS := ... -DLIB_PIGPIO`
 */
#include "mfrc522.h"
#include <linux/types.h>
#include <stdio.h>
#include <stdbool.h>
#include <syslog.h>

#ifndef LIB_PIGPIO

#include "bcm2835.h"
#define RSTPIN RPI_V2_GPIO_P1_22

#else

#include "pigpio.h"
#define OK 0
#define RSTPIN 22
#define LOW 0
#define HIGH 1
#define delay(n) gpioDelay(n * 1000)
int spiHandle;

#endif


// Firmware data for self-test
// Reference values based on firmware version; taken from 16.1.1 in spec.
// Version 1.0

const byte MFRC522_firmware_referenceV1_0[]  = {
	0x00, 0xC6, 0x37, 0xD5, 0x32, 0xB7, 0x57, 0x5C,
	0xC2, 0xD8, 0x7C, 0x4D, 0xD9, 0x70, 0xC7, 0x73,
	0x10, 0xE6, 0xD2, 0xAA, 0x5E, 0xA1, 0x3E, 0x5A,
	0x14, 0xAF, 0x30, 0x61, 0xC9, 0x70, 0xDB, 0x2E,
	0x64, 0x22, 0x72, 0xB5, 0xBD, 0x65, 0xF4, 0xEC,
	0x22, 0xBC, 0xD3, 0x72, 0x35, 0xCD, 0xAA, 0x41,
	0x1F, 0xA7, 0xF3, 0x53, 0x14, 0xDE, 0x7E, 0x02,
	0xD9, 0x0F, 0xB5, 0x5E, 0x25, 0x1D, 0x29, 0x79
};

// Version 2.0
const byte MFRC522_firmware_referenceV2_0[] = {
	0x00, 0xEB, 0x66, 0xBA, 0x57, 0xBF, 0x23, 0x95,
	0xD0, 0xE3, 0x0D, 0x3D, 0x27, 0x89, 0x5C, 0xDE,
	0x9D, 0x3B, 0xA7, 0x00, 0x21, 0x5B, 0x89, 0x82,
	0x51, 0x3A, 0xEB, 0x02, 0x0C, 0xA5, 0x00, 0x49,
	0x7C, 0x84, 0x4D, 0xB3, 0xCC, 0xD2, 0x1B, 0x81,
	0x5D, 0x48, 0x76, 0xD5, 0x71, 0x61, 0x21, 0xA9,
	0x86, 0x96, 0x83, 0x38, 0xCF, 0x9D, 0x5B, 0x6D,
	0xDC, 0x15, 0xBA, 0x3E, 0x7D, 0x95, 0x3B, 0x2F
};


// Member variables
Uid uid;								// Used by PICC_ReadCardSerial().

/**
 * Constructor.
 * Prepares the output pins.
 */
void mfrc522_init() {


#ifndef LIB_PIGPIO
	if (!bcm2835_init()) {
		printf("Failed to initialize. This tool needs root access, use sudo.\n");
	}
	bcm2835_gpio_fsel(RSTPIN, BCM2835_GPIO_FSEL_OUTP);
	bcm2835_gpio_write(RSTPIN, LOW);
#else
	// Should we (re)initialize pigpio or assume it is already initialized?
	gpioSetMode(RSTPIN, PI_OUTPUT);
	gpioWrite(RSTPIN, false);
#endif
	// Set SPI bus to work with MFRC522 chip.
	mfrc522_set_spi_config();
} // End constructor

/**
 * Set SPI bus to work with MFRC522 chip.
 * Please call this function if you have changed the SPI config since the MFRC522 constructor was run.
 */
void mfrc522_set_spi_config() {

#ifndef LIB_PIGPIO
	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);    // ~ 4 MHz
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default
#else
	int channel = 0;
	int baud = 3906250; // 250000000 / 64, 250MHz is Core Speed, 64 is "divider" from bcm2835 code
	int flags = 0;
	spiHandle = spiOpen(channel, baud, flags);
	if (spiHandle < 0) {
		perror("spiOpen(): FAILED!\n");
		syslog(LOG_ERR, "spiOpen(): FAILED!");
	}
	else {
		syslog(LOG_INFO, "spiOpen(): OK!");
	}

#endif

} // End setSPIConfig()

/////////////////////////////////////////////////////////////////////////////////////
// Basic interface functions for communicating with the MFRC522
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Writes a byte to the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void mfrc522_pcd_write_register(byte reg, byte value) {

	char data[2];
	data[0] = reg & 0x7E;
	data[1] = value;
#ifndef LIB_PIGPIO
	bcm2835_spi_transfern(data, 2);
#else
	int n;
	if ((n = spiWrite(spiHandle, data, 2)) < 0) {
		perror("spiWrite failed in mfrc522_pcd_write_register()");
	}
#endif

} // End PCD_WriteRegister()

/**
 * Writes a number of bytes to the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void mfrc522_pcd_write_register_multi(byte reg, byte count, byte *values) {
	byte index;
	for (index = 0; index < count; index++) {
		mfrc522_pcd_write_register(reg, values[index]);
	}

} // End PCD_WriteRegister()

/**
 * Reads a byte from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
byte mfrc522_pcd_read_register(byte reg) {

	char data[2];
	data[0] = 0x80 | ((reg) & 0x7E);
#ifndef LIB_PIGPIO
	bcm2835_spi_transfern(data,2);
#else
	int n;
	if ((n = spiXfer(spiHandle, (char*)data, (char*)data, 2)) < 0) {
		perror("spiWrite failed in mfrc522_pcd_read_register_multi()");
	}
#endif
	return (byte)data[1];
} // End PCD_ReadRegister()

/**
 * Reads a number of bytes from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void mfrc522_pcd_read_register_multi(byte reg,		///< The register to read from. One of the PCD_Register enums.
		byte count,		///< The number of bytes to read
		byte *values,	///< Byte array to store the values in.
		byte rxAlign	///< Only bit positions rxAlign..7 in values[0] are updated.
		) {
	if (count == 0) {
		return;
	}
	//Serial.print(F("Reading ")); 	Serial.print(count); Serial.println(F(" bytes from register."));
	byte address = 0x80 | (reg & 0x7E);		// MSB == 1 is for reading. LSB is not used in address. Datasheet section 8.1.2.3.
	byte index = 0;							// Index in values array.
	count--;								// One read is performed outside of the loop
#ifndef LIB_PIGPIO
	bcm2835_spi_transfer(address);
#else
	if (spiWrite(spiHandle, (char*)&address, 1) < 0) {
		perror("spiWrite failed in mfrc522_pcd_read_register()");
	}
#endif
	while (index < count) {
		if (index == 0 && rxAlign) {		// Only update bit positions rxAlign..7 in values[0]
			// Create bit mask for bit positions rxAlign..7
			byte i, mask = 0;
			for (i = rxAlign; i <= 7; i++) {
				mask |= (1 << i);
			}
			// Read value and tell that we want to read the same address again.

			byte value;
#ifndef LIB_PIGPIO
			value = bcm2835_spi_transfer(address);
#else
			if (spiXfer(spiHandle, (char*)&address, (char*)&value, 1) < 0) {
				perror("spiXfer failed");
			}
#endif

			// Apply mask to both current value of values[0] and the new data in value.
			values[0] = (values[index] & ~mask) | (value & mask);
		}
		else { // Normal case
#ifndef LIB_PIGPIO
			values[index] = bcm2835_spi_transfer(address);
#else
			if (spiXfer(spiHandle, (char*)&address, (char*)&values[index], 1) < 0) {
				perror("spiXfer failed");
			}
#endif
		}
		index++;
	}
#ifndef LIB_PIGPIO
	values[index] = bcm2835_spi_transfer(0);			// Read the final byte. Send 0 to stop reading.
#else
	byte tmp0 = 0;
	if (spiXfer(spiHandle, (char*)&tmp0, (char*)&values[index], 1) < 0) {
		perror("spiXfer failed");
	}
#endif
} // End PCD_ReadRegister()

/**
 * Sets the bits given in mask in register reg.
 */
void mfrc522_pcd_set_register_bit_mask(	byte reg,	///< The register to update. One of the PCD_Register enums.
		byte mask	///< The bits to set.
		) { 
	byte tmp;
	tmp = mfrc522_pcd_read_register(reg);
	mfrc522_pcd_write_register(reg, tmp | mask);			// set bit mask
} // End PCD_SetRegisterBitMask()

/**
 * Clears the bits given in mask from register reg.
 */
void mfrc522_pcd_clear_register_bit_mask(	byte reg,	///< The register to update. One of the PCD_Register enums.
		byte mask	///< The bits to clear.
		) {
	byte tmp;
	tmp = mfrc522_pcd_read_register(reg);
	mfrc522_pcd_write_register(reg, tmp & (~mask));		// clear bit mask
} // End PCD_ClearRegisterBitMask()


/**
 * Use the CRC coprocessor in the MFRC522 to calculate a CRC_A.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_pcd_calculate_crc(	byte *data,		///< In: Pointer to the data to transfer to the FIFO for CRC calculation.
		byte length,	///< In: The number of bytes to transfer.
		byte *result	///< Out: Pointer to result buffer. Result is written to result[0..1], low byte first.
		) {
	mfrc522_pcd_write_register(CommandReg, PCD_Idle);		// Stop any active command.
	mfrc522_pcd_write_register(DivIrqReg, 0x04);				// Clear the CRCIRq interrupt request bit
	mfrc522_pcd_set_register_bit_mask(FIFOLevelReg, 0x80);		// FlushBuffer = 1, FIFO initialization
	mfrc522_pcd_write_register_multi(FIFODataReg, length, data);	// Write data to the FIFO
	mfrc522_pcd_write_register(CommandReg, PCD_CalcCRC);		// Start the calculation

	// Wait for the CRC calculation to complete. Each iteration of the while-loop takes 17.73�s.
	word i = 5000;
	byte n;
	while (1) {
		n = mfrc522_pcd_read_register(DivIrqReg);	// DivIrqReg[7..0] bits are: Set2 reserved reserved MfinActIRq reserved CRCIRq reserved reserved
		if (n & 0x04) {						// CRCIRq bit set - calculation done
			break;
		}
		if (--i == 0) {						// The emergency break. We will eventually terminate on this one after 89ms. Communication with the MFRC522 might be down.
			return STATUS_TIMEOUT;
		}
	}
	mfrc522_pcd_write_register(CommandReg, PCD_Idle);		// Stop calculating CRC for new content in the FIFO.

	// Transfer the result from the registers to the result buffer
	result[0] = mfrc522_pcd_read_register(CRCResultRegL);
	result[1] = mfrc522_pcd_read_register(CRCResultRegH);
	return STATUS_OK;
} // End PCD_CalculateCRC()


/////////////////////////////////////////////////////////////////////////////////////
// Functions for manipulating the MFRC522
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the MFRC522 chip.
 */
void mfrc522_pcd_init() {
#ifndef LIB_PIGPIO
	if (bcm2835_gpio_lev(RSTPIN) == LOW) {	//The MFRC522 chip is in power down mode.
		bcm2835_gpio_write(RSTPIN, HIGH);		// Exit power down mode. This triggers a hard reset.
		// Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74�s. Let us be generous: 50ms.
		delay(50);
	}
	else { // Perform a soft reset
		mfrc522_pcd_reset();
	}
#else
	gpioSetMode(RSTPIN, PI_INPUT);
	if (gpioRead(RSTPIN) == LOW) {
		gpioSetMode(RSTPIN, PI_OUTPUT);
		gpioWrite(RSTPIN, HIGH);
		gpioDelay(50 * 1000);
	}
	else { // Perform a soft reset
		mfrc522_pcd_reset();
	}
#endif

	// When communicating with a PICC we need a timeout if something goes wrong.
	// f_timer = 13.56 MHz / (2*TPreScaler+1) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo].
	// TPrescaler_Hi are the four low bits in TModeReg. TPrescaler_Lo is TPrescalerReg.
	mfrc522_pcd_write_register(TModeReg, 0x80);			// TAuto=1; timer starts automatically at the end of the transmission in all communication modes at all speeds
	mfrc522_pcd_write_register(TPrescalerReg, 0xA9);		// TPreScaler = TModeReg[3..0]:TPrescalerReg, ie 0x0A9 = 169 => f_timer=40kHz, ie a timer period of 25�s.
	mfrc522_pcd_write_register(TReloadRegH, 0x03);		// Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
	mfrc522_pcd_write_register(TReloadRegL, 0xE8);

	mfrc522_pcd_write_register(TxASKReg, 0x40);		// Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
	mfrc522_pcd_write_register(ModeReg, 0x3D);		// Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)
	mfrc522_pcd_antenna_on();						// Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset)
} // End PCD_Init()

/**
 * Performs a soft reset on the MFRC522 chip and waits for it to be ready again.
 */
void mfrc522_pcd_reset() {
	mfrc522_pcd_write_register(CommandReg, PCD_SoftReset);	// Issue the SoftReset command.
	// The datasheet does not mention how long the SoftRest command takes to complete.
	// But the MFRC522 might have been in soft power-down mode (triggered by bit 4 of CommandReg) 
	// Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74�s. Let us be generous: 50ms.
	delay(50);
	// Wait for the PowerDown bit in CommandReg to be cleared
	while (mfrc522_pcd_read_register(CommandReg) & (1<<4)) {
		// PCD still restarting - unlikely after waiting 50ms, but better safe than sorry.
	}
} // End PCD_Reset()

/**
 * Turns the antenna on by enabling pins TX1 and TX2.
 * After a reset these pins are disabled.
 */
void mfrc522_pcd_antenna_on() {
	byte value = mfrc522_pcd_read_register(TxControlReg);
	if ((value & 0x03) != 0x03) {
		mfrc522_pcd_write_register(TxControlReg, value | 0x03);
	}
} // End PCD_AntennaOn()

/**
 * Turns the antenna off by disabling pins TX1 and TX2.
 */
void mfrc522_pcd_antenna_off() {
	mfrc522_pcd_clear_register_bit_mask(TxControlReg, 0x03);
} // End PCD_AntennaOff()

/**
 * Get the current MFRC522 Receiver Gain (RxGain[2:0]) value.
 * See 9.3.3.6 / table 98 in http://www.nxp.com/documents/data_sheet/MFRC522.pdf
 * NOTE: Return value scrubbed with (0x07<<4)=01110000b as RCFfgReg may use reserved bits.
 * 
 * @return Value of the RxGain, scrubbed to the 3 bits used.
 */
byte mfrc522_pcd_get_antenna_gain() {
	return mfrc522_pcd_read_register(RFCfgReg) & (0x07<<4);
} // End PCD_GetAntennaGain()

/**
 * Set the MFRC522 Receiver Gain (RxGain) to value specified by given mask.
 * See 9.3.3.6 / table 98 in http://www.nxp.com/documents/data_sheet/MFRC522.pdf
 * NOTE: Given mask is scrubbed with (0x07<<4)=01110000b as RCFfgReg may use reserved bits.
 */
void mfrc522_pcd_set_antenna_gain(byte mask) {
	if (mfrc522_pcd_get_antenna_gain() != mask) {						// only bother if there is a change
		mfrc522_pcd_clear_register_bit_mask(RFCfgReg, (0x07<<4));		// clear needed to allow 000 pattern
		mfrc522_pcd_set_register_bit_mask(RFCfgReg, mask & (0x07<<4));	// only set RxGain[2:0] bits
	}
} // End PCD_SetAntennaGain()

/**
 * Performs a self-test of the MFRC522
 * See 16.1.1 in http://www.nxp.com/documents/data_sheet/MFRC522.pdf
 * 
 * @return Whether or not the test passed.
 */
bool mfrc522_pcd_perform_self_test() {
	// This follows directly the steps outlined in 16.1.1
	// 1. Perform a soft reset.
	mfrc522_pcd_reset();

	// 2. Clear the internal buffer by writing 25 bytes of 00h
	byte ZEROES[25] = {0x00};
	mfrc522_pcd_set_register_bit_mask(FIFOLevelReg, 0x80); // flush the FIFO buffer
	mfrc522_pcd_write_register_multi(FIFODataReg, 25, ZEROES); // write 25 bytes of 00h to FIFO
	mfrc522_pcd_write_register(CommandReg, PCD_Mem); // transfer to internal buffer

	// 3. Enable self-test
	mfrc522_pcd_write_register(AutoTestReg, 0x09);

	// 4. Write 00h to FIFO buffer
	mfrc522_pcd_write_register(FIFODataReg, 0x00);

	// 5. Start self-test by issuing the CalcCRC command
	mfrc522_pcd_write_register(CommandReg, PCD_CalcCRC);

	// 6. Wait for self-test to complete
	word i;
	byte n;
	for (i = 0; i < 0xFF; i++) {
		n = mfrc522_pcd_read_register(DivIrqReg);	// DivIrqReg[7..0] bits are: Set2 reserved reserved MfinActIRq reserved CRCIRq reserved reserved
		if (n & 0x04) {						// CRCIRq bit set - calculation done
			break;
		}
	}
	mfrc522_pcd_write_register(CommandReg, PCD_Idle);		// Stop calculating CRC for new content in the FIFO.

	// 7. Read out resulting 64 bytes from the FIFO buffer.
	byte result[64];
	mfrc522_pcd_read_register_multi(FIFODataReg, 64, result, 0);

	// Auto self-test done
	// Reset AutoTestReg register to be 0 again. Required for normal operation.
	mfrc522_pcd_write_register(AutoTestReg, 0x00);

	// Determine firmware version (see section 9.3.4.8 in spec)
	byte version = mfrc522_pcd_read_register(VersionReg);

	// Pick the appropriate reference values
	const byte *reference;
	switch (version) {
		case 0x91:	// Version 1.0
			reference = MFRC522_firmware_referenceV1_0;
			break;
		case 0x92:	// Version 2.0
			reference = MFRC522_firmware_referenceV2_0;
			break;
		default:	// Unknown version
			return false;
	}

	// Verify that the results match up to our expectations
	for (i = 0; i < 64; i++) {
		if (result[i] != reference[i]) {
			return false;
		}
	}
	// Test passed; all is good.
	return true;
} // End PCD_PerformSelfTest()

/////////////////////////////////////////////////////////////////////////////////////
// Functions for communicating with PICCs
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Executes the Transceive command.
 * CRC validation can only be done if backData and backLen are specified.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_pcd_transceive_data(	byte *sendData,		///< Pointer to the data to transfer to the FIFO.
		byte sendLen,		///< Number of bytes to transfer to the FIFO.
		byte *backData,		///< NULL or pointer to buffer if data should be read back after executing the command.
		byte *backLen,		///< In: Max number of bytes to write to *backData. Out: The number of bytes returned.
		byte *validBits,	///< In/Out: The number of valid bits in the last byte. 0 for 8 valid bits. Default NULL.
		byte rxAlign,		///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
		bool checkCRC		///< In: true => The last two bytes of the response is assumed to be a CRC_A that must be validated.
		) {
	byte waitIRq = 0x30;		// RxIRq and IdleIRq
	return mfrc522_pcd_communicate_with_picc(PCD_Transceive, waitIRq, sendData, sendLen, backData, backLen, validBits, rxAlign, checkCRC);
} // End PCD_TransceiveData()

/**
 * Transfers data to the MFRC522 FIFO, executes a command, waits for completion and transfers data back from the FIFO.
 * CRC validation can only be done if backData and backLen are specified.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_pcd_communicate_with_picc(	byte command,		///< The command to execute. One of the PCD_Command enums.
		byte waitIRq,		///< The bits in the ComIrqReg register that signals successful completion of the command.
		byte *sendData,		///< Pointer to the data to transfer to the FIFO.
		byte sendLen,		///< Number of bytes to transfer to the FIFO.
		byte *backData,		///< NULL or pointer to buffer if data should be read back after executing the command.
		byte *backLen,		///< In: Max number of bytes to write to *backData. Out: The number of bytes returned.
		byte *validBits,	///< In/Out: The number of valid bits in the last byte. 0 for 8 valid bits.
		byte rxAlign,		///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
		bool checkCRC		///< In: true => The last two bytes of the response is assumed to be a CRC_A that must be validated.
		) {
	byte n, _validBits = 0;
	unsigned int i;

	// Prepare values for BitFramingReg
	byte txLastBits = validBits ? *validBits : 0;
	byte bitFraming = (rxAlign << 4) + txLastBits;		// RxAlign = BitFramingReg[6..4]. TxLastBits = BitFramingReg[2..0]

	mfrc522_pcd_write_register(CommandReg, PCD_Idle);			// Stop any active command.
	mfrc522_pcd_write_register(ComIrqReg, 0x7F);					// Clear all seven interrupt request bits
	mfrc522_pcd_set_register_bit_mask(FIFOLevelReg, 0x80);			// FlushBuffer = 1, FIFO initialization
	mfrc522_pcd_write_register_multi(FIFODataReg, sendLen, sendData);	// Write sendData to the FIFO
	mfrc522_pcd_write_register(BitFramingReg, bitFraming);		// Bit adjustments
	mfrc522_pcd_write_register(CommandReg, command);				// Execute the command
	if (command == PCD_Transceive) {
		mfrc522_pcd_set_register_bit_mask(BitFramingReg, 0x80);	// StartSend=1, transmission of data starts
	}

	// Wait for the command to complete.
	// In PCD_Init() we set the TAuto flag in TModeReg. This means the timer automatically starts when the PCD stops transmitting.
	// Each iteration of the do-while-loop takes 17.86�s.
	i = 2000;
	while (1) {
		n = mfrc522_pcd_read_register(ComIrqReg);	// ComIrqReg[7..0] bits are: Set1 TxIRq RxIRq IdleIRq HiAlertIRq LoAlertIRq ErrIRq TimerIRq
		if (n & waitIRq) {					// One of the interrupts that signal success has been set.
			break;
		}
		if (n & 0x01) {						// Timer interrupt - nothing received in 25ms
			return STATUS_TIMEOUT;
		}
		if (--i == 0) {						// The emergency break. If all other condions fail we will eventually terminate on this one after 35.7ms. Communication with the MFRC522 might be down.
			return STATUS_TIMEOUT;
		}
	}

	// Stop now if any errors except collisions were detected.
	byte errorRegValue = mfrc522_pcd_read_register(ErrorReg); // ErrorReg[7..0] bits are: WrErr TempErr reserved BufferOvfl CollErr CRCErr ParityErr ProtocolErr
	if (errorRegValue & 0x13) {	 // BufferOvfl ParityErr ProtocolErr
		return STATUS_ERROR;
	}	

	// If the caller wants data back, get it from the MFRC522.
	if (backData && backLen) {
		n = mfrc522_pcd_read_register(FIFOLevelReg);			// Number of bytes in the FIFO
		if (n > *backLen) {
			return STATUS_NO_ROOM;
		}
		*backLen = n;											// Number of bytes returned
		mfrc522_pcd_read_register_multi(FIFODataReg, n, backData, rxAlign);	// Get received data from FIFO
		_validBits = mfrc522_pcd_read_register(ControlReg) & 0x07;		// RxLastBits[2:0] indicates the number of valid bits in the last received byte. If this value is 000b, the whole byte is valid.
		if (validBits) {
			*validBits = _validBits;
		}
	}

	// Tell about collisions
	if (errorRegValue & 0x08) {		// CollErr
		return STATUS_COLLISION;
	}

	// Perform CRC_A validation if requested.
	if (backData && backLen && checkCRC) {
		// In this case a MIFARE Classic NAK is not OK.
		if (*backLen == 1 && _validBits == 4) {
			return STATUS_MIFARE_NACK;
		}
		// We need at least the CRC_A value and all 8 bits of the last byte must be received.
		if (*backLen < 2 || _validBits != 0) {
			return STATUS_CRC_WRONG;
		}
		// Verify CRC_A - do our own calculation and store the control in controlBuffer.
		byte controlBuffer[2];
		n = mfrc522_pcd_calculate_crc(&backData[0], *backLen - 2, &controlBuffer[0]);
		if (n != STATUS_OK) {
			return n;
		}
		if ((backData[*backLen - 2] != controlBuffer[0]) || (backData[*backLen - 1] != controlBuffer[1])) {
			return STATUS_CRC_WRONG;
		}
	}

	return STATUS_OK;
} // End PCD_CommunicateWithPICC()

/**
 * Transmits a REQuest command, Type A. Invites PICCs in state IDLE to go to READY and prepare for anticollision or selection. 7 bit frame.
 * Beware: When two PICCs are in the field at the same time I often get STATUS_TIMEOUT - probably due do bad antenna design.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_picc_request_a(byte *bufferATQA,	///< The buffer to store the ATQA (Answer to request) in
		byte *bufferSize	///< Buffer size, at least two bytes. Also number of bytes returned if STATUS_OK.
		) {
	return mfrc522_picc_reqa_or_wupa(PICC_CMD_REQA, bufferATQA, bufferSize);
} // End PICC_RequestA()

/**
 * Transmits a Wake-UP command, Type A. Invites PICCs in state IDLE and HALT to go to READY(*) and prepare for anticollision or selection. 7 bit frame.
 * Beware: When two PICCs are in the field at the same time I often get STATUS_TIMEOUT - probably due do bad antenna design.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_picc_wakeup_a(	byte *bufferATQA,	///< The buffer to store the ATQA (Answer to request) in
		byte *bufferSize	///< Buffer size, at least two bytes. Also number of bytes returned if STATUS_OK.
		) {
	return mfrc522_picc_reqa_or_wupa(PICC_CMD_WUPA, bufferATQA, bufferSize);
} // End PICC_WakeupA()

/**
 * Transmits REQA or WUPA commands.
 * Beware: When two PICCs are in the field at the same time I often get STATUS_TIMEOUT - probably due do bad antenna design.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */ 
byte mfrc522_picc_reqa_or_wupa(	byte command, 		///< The command to send - PICC_CMD_REQA or PICC_CMD_WUPA
		byte *bufferATQA,	///< The buffer to store the ATQA (Answer to request) in
		byte *bufferSize	///< Buffer size, at least two bytes. Also number of bytes returned if STATUS_OK.
		) {
	byte validBits;
	byte status;

	if (bufferATQA == NULL || *bufferSize < 2) {	// The ATQA response is 2 bytes long.
		return STATUS_NO_ROOM;
	}
	mfrc522_pcd_clear_register_bit_mask(CollReg, 0x80);		// ValuesAfterColl=1 => Bits received after collision are cleared.
	validBits = 7;									// For REQA and WUPA we need the short frame format - transmit only 7 bits of the last (and only) byte. TxLastBits = BitFramingReg[2..0]
	status = mfrc522_pcd_transceive_data(&command, 1, bufferATQA, bufferSize, &validBits, 0, false);
	if (status != STATUS_OK) {
		return status;
	}
	if (*bufferSize != 2 || validBits != 0) {		// ATQA must be exactly 16 bits.
		return STATUS_ERROR;
	}
	return STATUS_OK;
} // End PICC_REQA_or_WUPA()

/**
 * Transmits SELECT/ANTICOLLISION commands to select a single PICC.
 * Before calling this function the PICCs must be placed in the READY(*) state by calling PICC_RequestA() or PICC_WakeupA().
 * On success:
 * 		- The chosen PICC is in state ACTIVE(*) and all other PICCs have returned to state IDLE/HALT. (Figure 7 of the ISO/IEC 14443-3 draft.)
 * 		- The UID size and value of the chosen PICC is returned in *uid along with the SAK.
 * 
 * A PICC UID consists of 4, 7 or 10 bytes.
 * Only 4 bytes can be specified in a SELECT command, so for the longer UIDs two or three iterations are used:
 * 		UID size	Number of UID bytes		Cascade levels		Example of PICC
 * 		========	===================		==============		===============
 * 		single				 4						1				MIFARE Classic
 * 		double				 7						2				MIFARE Ultralight
 * 		triple				10						3				Not currently in use?
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_picc_select(	Uid *uid,			///< Pointer to Uid struct. Normally output, but can also be used to supply a known UID.
		byte validBits		///< The number of known UID bits supplied in *uid. Normally 0. If set you must also supply uid->size.
		) {
	bool uidComplete;
	bool selectDone;
	bool useCascadeTag;
	byte cascadeLevel = 1;
	byte result;
	byte count;
	byte index;
	byte uidIndex;					// The first index in uid->uidByte[] that is used in the current Cascade Level.
	signed char currentLevelKnownBits;		// The number of known UID bits in the current Cascade Level.
	byte buffer[9];					// The SELECT/ANTICOLLISION commands uses a 7 byte standard frame + 2 bytes CRC_A
	byte bufferUsed;				// The number of bytes used in the buffer, ie the number of bytes to transfer to the FIFO.
	byte rxAlign;					// Used in BitFramingReg. Defines the bit position for the first bit received.
	byte txLastBits;				// Used in BitFramingReg. The number of valid bits in the last transmitted byte. 
	byte *responseBuffer;
	byte responseLength;

	// Description of buffer structure:
	//		Byte 0: SEL 				Indicates the Cascade Level: PICC_CMD_SEL_CL1, PICC_CMD_SEL_CL2 or PICC_CMD_SEL_CL3
	//		Byte 1: NVB					Number of Valid Bits (in complete command, not just the UID): High nibble: complete bytes, Low nibble: Extra bits. 
	//		Byte 2: UID-data or CT		See explanation below. CT means Cascade Tag.
	//		Byte 3: UID-data
	//		Byte 4: UID-data
	//		Byte 5: UID-data
	//		Byte 6: BCC					Block Check Character - XOR of bytes 2-5
	//		Byte 7: CRC_A
	//		Byte 8: CRC_A
	// The BCC and CRC_A is only transmitted if we know all the UID bits of the current Cascade Level.
	//
	// Description of bytes 2-5: (Section 6.5.4 of the ISO/IEC 14443-3 draft: UID contents and cascade levels)
	//		UID size	Cascade level	Byte2	Byte3	Byte4	Byte5
	//		========	=============	=====	=====	=====	=====
	//		 4 bytes		1			uid0	uid1	uid2	uid3
	//		 7 bytes		1			CT		uid0	uid1	uid2
	//						2			uid3	uid4	uid5	uid6
	//		10 bytes		1			CT		uid0	uid1	uid2
	//						2			CT		uid3	uid4	uid5
	//						3			uid6	uid7	uid8	uid9

	// Sanity checks
	if (validBits > 80) {
		return STATUS_INVALID;
	}

	// Prepare MFRC522
	mfrc522_pcd_clear_register_bit_mask(CollReg, 0x80);		// ValuesAfterColl=1 => Bits received after collision are cleared.

	// Repeat Cascade Level loop until we have a complete UID.
	uidComplete = false;
	while (!uidComplete) {
		// Set the Cascade Level in the SEL byte, find out if we need to use the Cascade Tag in byte 2.
		switch (cascadeLevel) {
			case 1:
				buffer[0] = PICC_CMD_SEL_CL1;
				uidIndex = 0;
				useCascadeTag = validBits && uid->size > 4;	// When we know that the UID has more than 4 bytes
				break;

			case 2:
				buffer[0] = PICC_CMD_SEL_CL2;
				uidIndex = 3;
				useCascadeTag = validBits && uid->size > 7;	// When we know that the UID has more than 7 bytes
				break;

			case 3:
				buffer[0] = PICC_CMD_SEL_CL3;
				uidIndex = 6;
				useCascadeTag = false;						// Never used in CL3.
				break;

			default:
				return STATUS_INTERNAL_ERROR;
				break;
		}

		// How many UID bits are known in this Cascade Level?
		currentLevelKnownBits = validBits - (8 * uidIndex);
		if (currentLevelKnownBits < 0) {
			currentLevelKnownBits = 0;
		}
		// Copy the known bits from uid->uidByte[] to buffer[]
		index = 2; // destination index in buffer[]
		if (useCascadeTag) {
			buffer[index++] = PICC_CMD_CT;
		}
		byte bytesToCopy = currentLevelKnownBits / 8 + (currentLevelKnownBits % 8 ? 1 : 0); // The number of bytes needed to represent the known bits for this level.
		if (bytesToCopy) {
			byte maxBytes = useCascadeTag ? 3 : 4; // Max 4 bytes in each Cascade Level. Only 3 left if we use the Cascade Tag
			if (bytesToCopy > maxBytes) {
				bytesToCopy = maxBytes;
			}
			for (count = 0; count < bytesToCopy; count++) {
				buffer[index++] = uid->uidByte[uidIndex + count];
			}
		}
		// Now that the data has been copied we need to include the 8 bits in CT in currentLevelKnownBits
		if (useCascadeTag) {
			currentLevelKnownBits += 8;
		}

		// Repeat anti collision loop until we can transmit all UID bits + BCC and receive a SAK - max 32 iterations.
		selectDone = false;
		while (!selectDone) {
			// Find out how many bits and bytes to send and receive.
			if (currentLevelKnownBits >= 32) { // All UID bits in this Cascade Level are known. This is a SELECT.
				//Serial.print(F("SELECT: currentLevelKnownBits=")); Serial.println(currentLevelKnownBits, DEC);
				buffer[1] = 0x70; // NVB - Number of Valid Bits: Seven whole bytes
				// Calculate BCC - Block Check Character
				buffer[6] = buffer[2] ^ buffer[3] ^ buffer[4] ^ buffer[5];
				// Calculate CRC_A
				result = mfrc522_pcd_calculate_crc(buffer, 7, &buffer[7]);
				if (result != STATUS_OK) {
					return result;
				}
				txLastBits		= 0; // 0 => All 8 bits are valid.
				bufferUsed		= 9;
				// Store response in the last 3 bytes of buffer (BCC and CRC_A - not needed after tx)
				responseBuffer	= &buffer[6];
				responseLength	= 3;
			}
			else { // This is an ANTICOLLISION.
				//Serial.print(F("ANTICOLLISION: currentLevelKnownBits=")); Serial.println(currentLevelKnownBits, DEC);
				txLastBits		= currentLevelKnownBits % 8;
				count			= currentLevelKnownBits / 8;	// Number of whole bytes in the UID part.
				index			= 2 + count;					// Number of whole bytes: SEL + NVB + UIDs
				buffer[1]		= (index << 4) + txLastBits;	// NVB - Number of Valid Bits
				bufferUsed		= index + (txLastBits ? 1 : 0);
				// Store response in the unused part of buffer
				responseBuffer	= &buffer[index];
				responseLength	= sizeof(buffer) - index;
			}

			// Set bit adjustments
			rxAlign = txLastBits;											// Having a seperate variable is overkill. But it makes the next line easier to read.
			mfrc522_pcd_write_register(BitFramingReg, (rxAlign << 4) + txLastBits);	// RxAlign = BitFramingReg[6..4]. TxLastBits = BitFramingReg[2..0]

			// Transmit the buffer and receive the response.
			result = mfrc522_pcd_transceive_data(buffer, bufferUsed, responseBuffer, &responseLength, &txLastBits, rxAlign, false);
			if (result == STATUS_COLLISION) { // More than one PICC in the field => collision.
				result = mfrc522_pcd_read_register(CollReg); // CollReg[7..0] bits are: ValuesAfterColl reserved CollPosNotValid CollPos[4:0]
				if (result & 0x20) { // CollPosNotValid
					return STATUS_COLLISION; // Without a valid collision position we cannot continue
				}
				byte collisionPos = result & 0x1F; // Values 0-31, 0 means bit 32.
				if (collisionPos == 0) {
					collisionPos = 32;
				}
				if (collisionPos <= currentLevelKnownBits) { // No progress - should not happen 
					return STATUS_INTERNAL_ERROR;
				}
				// Choose the PICC with the bit set.
				currentLevelKnownBits = collisionPos;
				count			= (currentLevelKnownBits - 1) % 8; // The bit to modify
				index			= 1 + (currentLevelKnownBits / 8) + (count ? 1 : 0); // First byte is index 0.
				buffer[index]	|= (1 << count);
			}
			else if (result != STATUS_OK) {
				return result;
			}
			else { // STATUS_OK
				if (currentLevelKnownBits >= 32) { // This was a SELECT.
					selectDone = true; // No more anticollision 
					// We continue below outside the while.
				}
				else { // This was an ANTICOLLISION.
					// We now have all 32 bits of the UID in this Cascade Level
					currentLevelKnownBits = 32;
					// Run loop again to do the SELECT.
				}
			}
		} // End of while (!selectDone)

		// We do not check the CBB - it was constructed by us above.

		// Copy the found UID bytes from buffer[] to uid->uidByte[]
		index			= (buffer[2] == PICC_CMD_CT) ? 3 : 2; // source index in buffer[]
		bytesToCopy		= (buffer[2] == PICC_CMD_CT) ? 3 : 4;
		for (count = 0; count < bytesToCopy; count++) {
			uid->uidByte[uidIndex + count] = buffer[index++];
		}

		// Check response SAK (Select Acknowledge)
		if (responseLength != 3 || txLastBits != 0) { // SAK must be exactly 24 bits (1 byte + CRC_A).
			return STATUS_ERROR;
		}
		// Verify CRC_A - do our own calculation and store the control in buffer[2..3] - those bytes are not needed anymore.
		result = mfrc522_pcd_calculate_crc(responseBuffer, 1, &buffer[2]);
		if (result != STATUS_OK) {
			return result;
		}
		if ((buffer[2] != responseBuffer[1]) || (buffer[3] != responseBuffer[2])) {
			return STATUS_CRC_WRONG;
		}
		if (responseBuffer[0] & 0x04) { // Cascade bit set - UID not complete yes
			cascadeLevel++;
		}
		else {
			uidComplete = true;
			uid->sak = responseBuffer[0];
		}
	} // End of while (!uidComplete)

	// Set correct uid->size
	uid->size = 3 * cascadeLevel + 1;

	return STATUS_OK;
} // End PICC_Select()

/**
 * Instructs a PICC in state ACTIVE(*) to go to state HALT.
 *
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */ 
byte mfrc522_picc_halt_a() {
	byte result;
	byte buffer[4];

	// Build command buffer
	buffer[0] = PICC_CMD_HLTA;
	buffer[1] = 0;
	// Calculate CRC_A
	result = mfrc522_pcd_calculate_crc(buffer, 2, &buffer[2]);
	if (result != STATUS_OK) {
		return result;
	}

	// Send the command.
	// The standard says:
	//		If the PICC responds with any modulation during a period of 1 ms after the end of the frame containing the
	//		HLTA command, this response shall be interpreted as 'not acknowledge'.
	// We interpret that this way: Only STATUS_TIMEOUT is an success.
	result = mfrc522_pcd_transceive_data(buffer, sizeof(buffer), NULL, 0, NULL, 0, false);
	if (result == STATUS_TIMEOUT) {
		return STATUS_OK;
	}
	if (result == STATUS_OK) { // That is ironically NOT ok in this case ;-)
		return STATUS_ERROR;
	}
	return result;
} // End PICC_HaltA()


/////////////////////////////////////////////////////////////////////////////////////
// Functions for communicating with MIFARE PICCs
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Executes the MFRC522 MFAuthent command.
 * This command manages MIFARE authentication to enable a secure communication to any MIFARE Mini, MIFARE 1K and MIFARE 4K card.
 * The authentication is described in the MFRC522 datasheet section 10.3.1.9 and http://www.nxp.com/documents/data_sheet/MF1S503x.pdf section 10.1.
 * For use with MIFARE Classic PICCs.
 * The PICC must be selected - ie in state ACTIVE(*) - before calling this function.
 * Remember to call PCD_StopCrypto1() after communicating with the authenticated PICC - otherwise no new communications can start.
 * 
 * All keys are set to FFFFFFFFFFFFh at chip delivery.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise. Probably STATUS_TIMEOUT if you supply the wrong key.
 */
byte mfrc522_pcd_authenticate(byte command,		///< PICC_CMD_MF_AUTH_KEY_A or PICC_CMD_MF_AUTH_KEY_B
		byte blockAddr, 	///< The block number. See numbering in the comments in the .h file.
		MIFARE_Key *key,	///< Pointer to the Crypteo1 key to use (6 bytes)
		Uid *uid			///< Pointer to Uid struct. The first 4 bytes of the UID is used.
		) {
	byte waitIRq = 0x10;		// IdleIRq

	// Build command buffer
	byte sendData[12];
	sendData[0] = command;
	sendData[1] = blockAddr;
	byte i;
	for (i = 0; i < MF_KEY_SIZE; i++) {	// 6 key bytes
		sendData[2+i] = key->keyByte[i];
	}
	for (i = 0; i < 4; i++) {				// The first 4 bytes of the UID
		sendData[8+i] = uid->uidByte[i];
	}

	// Start the authentication.
	return mfrc522_pcd_communicate_with_picc(PCD_MFAuthent, waitIRq, &sendData[0], sizeof(sendData), NULL, NULL, NULL, 0, true);
} // End PCD_Authenticate()

/**
 * Used to exit the PCD from its authenticated state.
 * Remember to call this function after communicating with an authenticated PICC - otherwise no new communications can start.
 */
void mfrc522_pcd_stop_crypto_1() {
	// Clear MFCrypto1On bit
	mfrc522_pcd_clear_register_bit_mask(Status2Reg, 0x08); // Status2Reg[7..0] bits are: TempSensClear I2CForceHS reserved reserved MFCrypto1On ModemState[2:0]
} // End PCD_StopCrypto1()

/**
 * Reads 16 bytes (+ 2 bytes CRC_A) from the active PICC.
 * 
 * For MIFARE Classic the sector containing the block must be authenticated before calling this function.
 * 
 * For MIFARE Ultralight only addresses 00h to 0Fh are decoded.
 * The MF0ICU1 returns a NAK for higher addresses.
 * The MF0ICU1 responds to the READ command by sending 16 bytes starting from the page address defined by the command argument.
 * For example; if blockAddr is 03h then pages 03h, 04h, 05h, 06h are returned.
 * A roll-back is implemented: If blockAddr is 0Eh, then the contents of pages 0Eh, 0Fh, 00h and 01h are returned.
 * 
 * The buffer must be at least 18 bytes because a CRC_A is also returned.
 * Checks the CRC_A before returning STATUS_OK.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_read(	byte blockAddr, 	///< MIFARE Classic: The block (0-0xff) number. MIFARE Ultralight: The first page to return data from.
		byte *buffer,		///< The buffer to store the data in
		byte *bufferSize	///< Buffer size, at least 18 bytes. Also number of bytes returned if STATUS_OK.
		) {
	byte result;

	// Sanity check
	if (buffer == NULL || *bufferSize < 18) {
		return STATUS_NO_ROOM;
	}

	// Build command buffer
	buffer[0] = PICC_CMD_MF_READ;
	buffer[1] = blockAddr;
	// Calculate CRC_A
	result = mfrc522_pcd_calculate_crc(buffer, 2, &buffer[2]);
	if (result != STATUS_OK) {
		return result;
	}

	// Transmit the buffer and receive the response, validate CRC_A.
	return mfrc522_pcd_transceive_data(buffer, 4, buffer, bufferSize, NULL, 0, false);
} // End MIFARE_Read()

/**
 * Writes 16 bytes to the active PICC.
 * 
 * For MIFARE Classic the sector containing the block must be authenticated before calling this function.
 * 
 * For MIFARE Ultralight the operation is called "COMPATIBILITY WRITE".
 * Even though 16 bytes are transferred to the Ultralight PICC, only the least significant 4 bytes (bytes 0 to 3)
 * are written to the specified address. It is recommended to set the remaining bytes 04h to 0Fh to all logic 0.
 * * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_write(	byte blockAddr, ///< MIFARE Classic: The block (0-0xff) number. MIFARE Ultralight: The page (2-15) to write to.
		byte *buffer,	///< The 16 bytes to write to the PICC
		byte bufferSize	///< Buffer size, must be at least 16 bytes. Exactly 16 bytes are written.
		) {
	byte result;

	// Sanity check
	if (buffer == NULL || bufferSize < 16) {
		return STATUS_INVALID;
	}

	// Mifare Classic protocol requires two communications to perform a write.
	// Step 1: Tell the PICC we want to write to block blockAddr.
	byte cmdBuffer[2];
	cmdBuffer[0] = PICC_CMD_MF_WRITE;
	cmdBuffer[1] = blockAddr;
	result = mfrc522_pcd_mifare_transceive(cmdBuffer, 2, false); // Adds CRC_A and checks that the response is MF_ACK.
	if (result != STATUS_OK) {
		return result;
	}

	// Step 2: Transfer the data
	result = mfrc522_pcd_mifare_transceive(buffer, bufferSize, false); // Adds CRC_A and checks that the response is MF_ACK.
	if (result != STATUS_OK) {
		return result;
	}

	return STATUS_OK;
} // End MIFARE_Write()

/**
 * Writes a 4 byte page to the active MIFARE Ultralight PICC.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_ultralight_write(	byte page, 		///< The page (2-15) to write to.
		byte *buffer,	///< The 4 bytes to write to the PICC
		byte bufferSize	///< Buffer size, must be at least 4 bytes. Exactly 4 bytes are written.
		) {
	byte result;

	// Sanity check
	if (buffer == NULL || bufferSize < 4) {
		return STATUS_INVALID;
	}

	// Build commmand buffer
	byte cmdBuffer[6];
	cmdBuffer[0] = PICC_CMD_UL_WRITE;
	cmdBuffer[1] = page;
	memcpy(&cmdBuffer[2], buffer, 4);

	// Perform the write
	result = mfrc522_pcd_mifare_transceive(cmdBuffer, 6, false); // Adds CRC_A and checks that the response is MF_ACK.
	if (result != STATUS_OK) {
		return result;
	}
	return STATUS_OK;
} // End MIFARE_Ultralight_Write()

/**
 * MIFARE Decrement subtracts the delta from the value of the addressed block, and stores the result in a volatile memory.
 * For MIFARE Classic only. The sector containing the block must be authenticated before calling this function.
 * Only for blocks in "value block" mode, ie with access bits [C1 C2 C3] = [110] or [001].
 * Use MIFARE_Transfer() to store the result in a block.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_decrement(	byte blockAddr, ///< The block (0-0xff) number.
		long delta		///< This number is subtracted from the value of block blockAddr.
		) {
	return mfrc522_mifare_two_step_helper(PICC_CMD_MF_DECREMENT, blockAddr, delta);
} // End MIFARE_Decrement()

/**
 * MIFARE Increment adds the delta to the value of the addressed block, and stores the result in a volatile memory.
 * For MIFARE Classic only. The sector containing the block must be authenticated before calling this function.
 * Only for blocks in "value block" mode, ie with access bits [C1 C2 C3] = [110] or [001].
 * Use MIFARE_Transfer() to store the result in a block.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_increment(	byte blockAddr, ///< The block (0-0xff) number.
		long delta		///< This number is added to the value of block blockAddr.
		) {
	return mfrc522_mifare_two_step_helper(PICC_CMD_MF_INCREMENT, blockAddr, delta);
} // End MIFARE_Increment()

/**
 * MIFARE Restore copies the value of the addressed block into a volatile memory.
 * For MIFARE Classic only. The sector containing the block must be authenticated before calling this function.
 * Only for blocks in "value block" mode, ie with access bits [C1 C2 C3] = [110] or [001].
 * Use MIFARE_Transfer() to store the result in a block.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_restore(	byte blockAddr ///< The block (0-0xff) number.
		) {
	// The datasheet describes Restore as a two step operation, but does not explain what data to transfer in step 2.
	// Doing only a single step does not work, so I chose to transfer 0L in step two.
	return mfrc522_mifare_two_step_helper(PICC_CMD_MF_RESTORE, blockAddr, 0l);
} // End MIFARE_Restore()

/**
 * Helper function for the two-step MIFARE Classic protocol operations Decrement, Increment and Restore.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_two_step_helper(	byte command,	///< The command to use
		byte blockAddr,	///< The block (0-0xff) number.
		long data		///< The data to transfer in step 2
		) {
	byte result;
	byte cmdBuffer[2]; // We only need room for 2 bytes.

	// Step 1: Tell the PICC the command and block address
	cmdBuffer[0] = command;
	cmdBuffer[1] = blockAddr;
	result = mfrc522_pcd_mifare_transceive(	cmdBuffer, 2, false); // Adds CRC_A and checks that the response is MF_ACK.
	if (result != STATUS_OK) {
		return result;
	}

	// Step 2: Transfer the data
	result = mfrc522_pcd_mifare_transceive(	(byte *)&data, 4, true); // Adds CRC_A and accept timeout as success.
	if (result != STATUS_OK) {
		return result;
	}

	return STATUS_OK;
} // End MIFARE_TwoStepHelper()

/**
 * MIFARE Transfer writes the value stored in the volatile memory into one MIFARE Classic block.
 * For MIFARE Classic only. The sector containing the block must be authenticated before calling this function.
 * Only for blocks in "value block" mode, ie with access bits [C1 C2 C3] = [110] or [001].
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_transfer(	byte blockAddr ///< The block (0-0xff) number.
		) {
	byte result;
	byte cmdBuffer[2]; // We only need room for 2 bytes.

	// Tell the PICC we want to transfer the result into block blockAddr.
	cmdBuffer[0] = PICC_CMD_MF_TRANSFER;
	cmdBuffer[1] = blockAddr;
	result = mfrc522_pcd_mifare_transceive(	cmdBuffer, 2, false); // Adds CRC_A and checks that the response is MF_ACK.
	if (result != STATUS_OK) {
		return result;
	}
	return STATUS_OK;
} // End MIFARE_Transfer()

/**
 * Helper routine to read the current value from a Value Block.
 * 
 * Only for MIFARE Classic and only for blocks in "value block" mode, that
 * is: with access bits [C1 C2 C3] = [110] or [001]. The sector containing
 * the block must be authenticated before calling this function. 
 * 
 * @param[in]   blockAddr   The block (0x00-0xff) number.
 * @param[out]  value       Current value of the Value Block.
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_get_value(byte blockAddr, long *value) {
	byte status;
	byte buffer[18];
	byte size = sizeof(buffer);

	// Read the block
	status = mfrc522_mifare_read(blockAddr, buffer, &size);
	if (status == STATUS_OK) {
		// Extract the value
		*value = ((long)(buffer[3])<<24) | ((long)(buffer[2])<<16) | ((long)(buffer[1])<<8) | (long)(buffer[0]);
	}
	return status;
} // End MIFARE_GetValue()

/**
 * Helper routine to write a specific value into a Value Block.
 * 
 * Only for MIFARE Classic and only for blocks in "value block" mode, that
 * is: with access bits [C1 C2 C3] = [110] or [001]. The sector containing
 * the block must be authenticated before calling this function. 
 * 
 * @param[in]   blockAddr   The block (0x00-0xff) number.
 * @param[in]   value       New value of the Value Block.
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_mifare_set_value(byte blockAddr, long value) {
	byte buffer[18];

	// Translate the long into 4 bytes; repeated 2x in value block
	buffer[0] = buffer[ 8] = (value & 0xFF);
	buffer[1] = buffer[ 9] = (value & 0xFF00) >> 8;
	buffer[2] = buffer[10] = (value & 0xFF0000) >> 16;
	buffer[3] = buffer[11] = (value & 0xFF000000) >> 24;
	// Inverse 4 bytes also found in value block
	buffer[4] = ~buffer[0];
	buffer[5] = ~buffer[1];
	buffer[6] = ~buffer[2];
	buffer[7] = ~buffer[3];
	// Address 2x with inverse address 2x
	buffer[12] = buffer[14] = blockAddr;
	buffer[13] = buffer[15] = ~blockAddr;

	// Write the whole data block
	return mfrc522_mifare_write(blockAddr, buffer, 16);
} // End MIFARE_SetValue()

/////////////////////////////////////////////////////////////////////////////////////
// Support functions
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Wrapper for MIFARE protocol communication.
 * Adds CRC_A, executes the Transceive command and checks that the response is MF_ACK or a timeout.
 * 
 * @return STATUS_OK on success, STATUS_??? otherwise.
 */
byte mfrc522_pcd_mifare_transceive(	byte *sendData,		///< Pointer to the data to transfer to the FIFO. Do NOT include the CRC_A.
		byte sendLen,		///< Number of bytes in sendData.
		bool acceptTimeout	///< true => A timeout is also success
		) {
	byte result;
	byte cmdBuffer[18]; // We need room for 16 bytes data and 2 bytes CRC_A.

	// Sanity check
	if (sendData == NULL || sendLen > 16) {
		return STATUS_INVALID;
	}

	// Copy sendData[] to cmdBuffer[] and add CRC_A
	memcpy(cmdBuffer, sendData, sendLen);
	result = mfrc522_pcd_calculate_crc(cmdBuffer, sendLen, &cmdBuffer[sendLen]);
	if (result != STATUS_OK) { 
		return result;
	}
	sendLen += 2;

	// Transceive the data, store the reply in cmdBuffer[]
	byte waitIRq = 0x30;		// RxIRq and IdleIRq
	byte cmdBufferSize = sizeof(cmdBuffer);
	byte validBits = 0;
	result = mfrc522_pcd_communicate_with_picc(PCD_Transceive, waitIRq, cmdBuffer, sendLen, cmdBuffer, &cmdBufferSize, &validBits, 0, false);
	if (acceptTimeout && result == STATUS_TIMEOUT) {
		return STATUS_OK;
	}
	if (result != STATUS_OK) {
		return result;
	}
	// The PICC must reply with a 4 bit ACK
	if (cmdBufferSize != 1 || validBits != 4) {
		return STATUS_ERROR;
	}
	if (cmdBuffer[0] != MF_ACK) {
		return STATUS_MIFARE_NACK;
	}
	return STATUS_OK;
} // End PCD_MIFARE_Transceive()

/**
 * Returns a __FlashStringHelper pointer to a status code name.
 * 
 */
const char* mfrc522_get_status_code_name(byte code	///< One of the StatusCode enums.
		) {
	switch (code) {
		case STATUS_OK:				return ("Success.");										break;
		case STATUS_ERROR:			return ("Error in communication.");						break;
		case STATUS_COLLISION:		return ("Collission detected.");							break;
		case STATUS_TIMEOUT:		return ("Timeout in communication.");						break;
		case STATUS_NO_ROOM:		return ("A buffer is not big enough.");					break;
		case STATUS_INTERNAL_ERROR:	return ("Internal error in the code. Should not happen.");	break;
		case STATUS_INVALID:		return ("Invalid argument.");								break;
		case STATUS_CRC_WRONG:		return ("The CRC_A does not match.");						break;
		case STATUS_MIFARE_NACK:	return ("A MIFARE PICC responded with NAK.");				break;
		default:					return ("Unknown error");									break;
	}
} // End GetStatusCodeName()

/**
 * Translates the SAK (Select Acknowledge) to a PICC type.
 * 
 * @return PICC_Type
 */
byte mfrc522_picc_get_type(byte sak		///< The SAK byte returned from PICC_Select().
		) {
	if (sak & 0x04) { // UID not complete
		return PICC_TYPE_NOT_COMPLETE;
	}

	switch (sak) {
		case 0x09:	return PICC_TYPE_MIFARE_MINI;	break;
		case 0x08:	return PICC_TYPE_MIFARE_1K;		break;
		case 0x18:	return PICC_TYPE_MIFARE_4K;		break;
		case 0x00:	return PICC_TYPE_MIFARE_UL;		break;
		case 0x10:
		case 0x11:	return PICC_TYPE_MIFARE_PLUS;	break;
		case 0x01:	return PICC_TYPE_TNP3XXX;		break;
		default:	break;
	}

	if (sak & 0x20) {
		return PICC_TYPE_ISO_14443_4;
	}

	if (sak & 0x40) {
		return PICC_TYPE_ISO_18092;
	}

	return PICC_TYPE_UNKNOWN;
} // End PICC_GetType()

/**
 * Returns a String pointer to the PICC type name.
 * 
 */
const char* mfrc522_picc_get_type_name(byte piccType	///< One of the PICC_Type enums.
		) {
	switch (piccType) {
		case PICC_TYPE_ISO_14443_4:		return ("PICC compliant with ISO/IEC 14443-4");	break;
		case PICC_TYPE_ISO_18092:		return ("PICC compliant with ISO/IEC 18092 (NFC)");break;
		case PICC_TYPE_MIFARE_MINI:		return ("MIFARE Mini, 320 bytes");					break;
		case PICC_TYPE_MIFARE_1K:		return ("MIFARE 1KB");								break;
		case PICC_TYPE_MIFARE_4K:		return ("MIFARE 4KB");								break;
		case PICC_TYPE_MIFARE_UL:		return ("MIFARE Ultralight or Ultralight C");		break;
		case PICC_TYPE_MIFARE_PLUS:		return ("MIFARE Plus");							break;
		case PICC_TYPE_TNP3XXX:			return ("MIFARE TNP3XXX");							break;
		case PICC_TYPE_NOT_COMPLETE:	return ("SAK indicates UID is not complete.");		break;
		case PICC_TYPE_UNKNOWN:
		default:						return ("Unknown type");							break;
	}
} // End PICC_GetTypeName()

/**
 * Dumps debug info about the selected PICC to Serial.
 * On success the PICC is halted after dumping the data.
 * For MIFARE Classic the factory default key of 0xFFFFFFFFFFFF is tried. 
 */
void mfrc522_picc_dump_to_serial(Uid *uid	///< Pointer to Uid struct returned from a successful PICC_Select().
		) {
	MIFARE_Key key;

	// UID
	printf("Card UID:");
	byte i;
	for (i = 0; i < uid->size; i++) {
		if(uid->uidByte[i] < 0x10)
			printf(" 0");
		else
			printf(" ");
		printf("%X", uid->uidByte[i]);
	} 
	printf("\n");

	// PICC type
	byte piccType = mfrc522_picc_get_type(uid->sak);
	printf("PICC type: ");
	//Serial.println(PICC_GetTypeName(piccType));
	printf("%s", mfrc522_picc_get_type_name(piccType));

	// Dump contents
	switch (piccType) {
		case PICC_TYPE_MIFARE_MINI:
		case PICC_TYPE_MIFARE_1K:
		case PICC_TYPE_MIFARE_4K:
			// All keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
			for (i = 0; i < 6; i++) {
				key.keyByte[i] = 0xFF;
			}
			mfrc522_picc_dump_mifare_classic_to_serial(uid, piccType, &key);
			break;

		case PICC_TYPE_MIFARE_UL:
			mfrc522_picc_dump_mifare_ultralight_to_serial();
			break;

		case PICC_TYPE_ISO_14443_4:
		case PICC_TYPE_ISO_18092:
		case PICC_TYPE_MIFARE_PLUS:
		case PICC_TYPE_TNP3XXX:
			printf("Dumping memory contents not implemented for that PICC type.");
			break;

		case PICC_TYPE_UNKNOWN:
		case PICC_TYPE_NOT_COMPLETE:
		default:
			break; // No memory dump here
	}

	printf("\n");
	mfrc522_picc_halt_a(); // Already done if it was a MIFARE Classic PICC.
} // End PICC_DumpToSerial()

/**
 * Dumps memory contents of a MIFARE Classic PICC.
 * On success the PICC is halted after dumping the data.
 */
void mfrc522_picc_dump_mifare_classic_to_serial(	Uid *uid,		///< Pointer to Uid struct returned from a successful PICC_Select().
		byte piccType,	///< One of the PICC_Type enums.
		MIFARE_Key *key	///< Key A used for all sectors.
		) {
	byte no_of_sectors = 0;
	switch (piccType) {
		case PICC_TYPE_MIFARE_MINI:
			// Has 5 sectors * 4 blocks/sector * 16 bytes/block = 320 bytes.
			no_of_sectors = 5;
			break;

		case PICC_TYPE_MIFARE_1K:
			// Has 16 sectors * 4 blocks/sector * 16 bytes/block = 1024 bytes.
			no_of_sectors = 16;
			break;

		case PICC_TYPE_MIFARE_4K:
			// Has (32 sectors * 4 blocks/sector + 8 sectors * 16 blocks/sector) * 16 bytes/block = 4096 bytes.
			no_of_sectors = 40;
			break;

		default: // Should not happen. Ignore.
			break;
	}

	char i;

	// Dump sectors, highest address first.
	if (no_of_sectors) {
		printf("Sector Block   0  1  2  3   4  5  6  7   8  9 10 11  12 13 14 15  AccessBits\n");
		for (i = no_of_sectors - 1; i >= 0; i--) {
			mfrc522_picc_dump_mifare_classic_sector_to_serial(uid, key, i);
		}
	}
	mfrc522_picc_halt_a(); // Halt the PICC before stopping the encrypted session.
	mfrc522_pcd_stop_crypto_1();
} // End PICC_DumpMifareClassicToSerial()

/**
 * Dumps memory contents of a sector of a MIFARE Classic PICC.
 * Uses PCD_Authenticate(), MIFARE_Read() and PCD_StopCrypto1.
 * Always uses PICC_CMD_MF_AUTH_KEY_A because only Key A can always read the sector trailer access bits.
 */
void mfrc522_picc_dump_mifare_classic_sector_to_serial(Uid *uid,			///< Pointer to Uid struct returned from a successful PICC_Select().
		MIFARE_Key *key,	///< Key A for the sector.
		byte sector			///< The sector to dump, 0..39.
		) {
	byte status;
	byte firstBlock;		// Address of lowest address to dump actually last block dumped)
	byte no_of_blocks;		// Number of blocks in sector
	bool isSectorTrailer;	// Set to true while handling the "last" (ie highest address) in the sector.

	// The access bits are stored in a peculiar fashion.
	// There are four groups:
	//		g[3]	Access bits for the sector trailer, block 3 (for sectors 0-31) or block 15 (for sectors 32-39)
	//		g[2]	Access bits for block 2 (for sectors 0-31) or blocks 10-14 (for sectors 32-39)
	//		g[1]	Access bits for block 1 (for sectors 0-31) or blocks 5-9 (for sectors 32-39)
	//		g[0]	Access bits for block 0 (for sectors 0-31) or blocks 0-4 (for sectors 32-39)
	// Each group has access bits [C1 C2 C3]. In this code C1 is MSB and C3 is LSB.
	// The four CX bits are stored together in a nible cx and an inverted nible cx_.
	byte c1, c2, c3;		// Nibbles
	byte c1_, c2_, c3_;		// Inverted nibbles
	bool invertedError;		// true if one of the inverted nibbles did not match
	byte g[4];				// Access bits for each of the four groups.
	byte group;				// 0-3 - active group for access bits
	bool firstInGroup;		// true for the first block dumped in the group

	// Determine position and size of sector.
	if (sector < 32) { // Sectors 0..31 has 4 blocks each
		no_of_blocks = 4;
		firstBlock = sector * no_of_blocks;
	}
	else if (sector < 40) { // Sectors 32-39 has 16 blocks each
		no_of_blocks = 16;
		firstBlock = 128 + (sector - 32) * no_of_blocks;
	}
	else { // Illegal input, no MIFARE Classic PICC has more than 40 sectors.
		return;
	}

	// Dump blocks, highest address first.
	byte byteCount;
	byte buffer[18];
	byte blockAddr;
	isSectorTrailer = true;
	char blockOffset;
	for (blockOffset = no_of_blocks - 1; blockOffset >= 0; blockOffset--) {
		blockAddr = firstBlock + blockOffset;
		// Sector number - only on first line
		if (isSectorTrailer) {
			if(sector < 10)
				printf("   "); // Pad with spaces
			else
				printf("  "); // Pad with spaces
			printf("%02X",sector);
			printf("   ");
		}
		else {
			printf("       ");
		}
		// Block number
		if(blockAddr < 10)
			printf("   "); // Pad with spaces
		else {
			if(blockAddr < 100)
				printf("  "); // Pad with spaces
			else
				printf(" "); // Pad with spaces
		}
		printf("%02X",blockAddr);
		printf("  ");
		// Establish encrypted communications before reading the first block
		if (isSectorTrailer) {
			status = mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, firstBlock, key, uid);
			if (status != STATUS_OK) {
				printf("PCD_Authenticate() failed: ");
				printf("%s\n",mfrc522_get_status_code_name(status));
				return;
			}
		}
		// Read block
		byteCount = sizeof(buffer);
		status = mfrc522_mifare_read(blockAddr, buffer, &byteCount);
		if (status != STATUS_OK) {
			printf("MIFARE_Read() failed: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
			continue;
		}
		// Dump data
		byte index;
		for (index = 0; index < 16; index++) {
			if(buffer[index] < 0x10)
				printf(" 0");
			else
				printf(" ");
			printf("9x%02X",buffer[index]);
			if ((index % 4) == 3) {
				printf(" ");
			}
		}
		// Parse sector trailer data
		if (isSectorTrailer) {
			c1  = buffer[7] >> 4;
			c2  = buffer[8] & 0xF;
			c3  = buffer[8] >> 4;
			c1_ = buffer[6] & 0xF;
			c2_ = buffer[6] >> 4;
			c3_ = buffer[7] & 0xF;
			invertedError = (c1 != (~c1_ & 0xF)) || (c2 != (~c2_ & 0xF)) || (c3 != (~c3_ & 0xF));
			g[0] = ((c1 & 1) << 2) | ((c2 & 1) << 1) | ((c3 & 1) << 0);
			g[1] = ((c1 & 2) << 1) | ((c2 & 2) << 0) | ((c3 & 2) >> 1);
			g[2] = ((c1 & 4) << 0) | ((c2 & 4) >> 1) | ((c3 & 4) >> 2);
			g[3] = ((c1 & 8) >> 1) | ((c2 & 8) >> 2) | ((c3 & 8) >> 3);
			isSectorTrailer = false;
		}

		// Which access group is this block in?
		if (no_of_blocks == 4) {
			group = blockOffset;
			firstInGroup = true;
		}
		else {
			group = blockOffset / 5;
			firstInGroup = (group == 3) || (group != (blockOffset + 1) / 5);
		}

		if (firstInGroup) {
			// Print access bits
			printf(" [ ");      
			printf("%02X" ,(g[group] >> 2) & 1); printf(" ");
			printf("%02X", (g[group] >> 1) & 1); printf(" ");
			printf("%02X", (g[group] >> 0) & 1);
			printf(" ] ");

			if (invertedError) {
				printf(" Inverted access bits did not match! ");
			}
		}

		if (group != 3 && (g[group] == 1 || g[group] == 6)) { // Not a sector trailer, a value block
			long value = ((long)(buffer[3])<<24) | ((long)(buffer[2])<<16) | ((long)(buffer[1])<<8) | (long)(buffer[0]);
			printf(" Value="); printf("0x%02X", (unsigned int)value);
			printf(" Adr="); printf("0x%02X", buffer[12]);
		}
		printf("\n");
	}

	return;
} // End PICC_DumpMifareClassicSectorToSerial()

/**
 * Dumps memory contents of a MIFARE Ultralight PICC.
 */
void mfrc522_picc_dump_mifare_ultralight_to_serial() {
	byte status;
	byte byteCount;
	byte buffer[18];
	byte i;

	printf("Page  0  1  2  3");
	// Try the mpages of the original Ultralight. Ultralight C has more pages.
	byte page;
	for (page = 0; page < 16; page +=4) { // Read returns data for 4 pages at a time.
		// Read pages
		byteCount = sizeof(buffer);
		status = mfrc522_mifare_read(page, buffer, &byteCount);
		if (status != STATUS_OK) {
			printf("MIFARE_Read() failed: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
			break;
		}
		// Dump data
		byte offset;
		for (offset = 0; offset < 4; offset++) {
			i = page + offset;
			if(i < 10)
				printf("  "); // Pad with spaces
			else
				printf(" "); // Pad with spaces
			printf("%02X",i);
			printf("  ");
			byte index;
			for (index = 0; index < 4; index++) {
				i = 4 * offset + index;
				if(buffer[i] < 0x10)
					printf(" 0");
				else
					printf(" ");
				printf("%02X",buffer[i]);
			}
			printf("\n");
		}
	}
} // End PICC_DumpMifareUltralightToSerial()

/**
 * Calculates the bit pattern needed for the specified access bits. In the [C1 C2 C3] tupples C1 is MSB (=4) and C3 is LSB (=1).
 */
void mfrc522_mifare_set_access_bits(	byte *accessBitBuffer,	///< Pointer to byte 6, 7 and 8 in the sector trailer. Bytes [0..2] will be set.
		byte g0,				///< Access bits [C1 C2 C3] for block 0 (for sectors 0-31) or blocks 0-4 (for sectors 32-39)
		byte g1,				///< Access bits C1 C2 C3] for block 1 (for sectors 0-31) or blocks 5-9 (for sectors 32-39)
		byte g2,				///< Access bits C1 C2 C3] for block 2 (for sectors 0-31) or blocks 10-14 (for sectors 32-39)
		byte g3					///< Access bits C1 C2 C3] for the sector trailer, block 3 (for sectors 0-31) or block 15 (for sectors 32-39)
		) {
	byte c1 = ((g3 & 4) << 1) | ((g2 & 4) << 0) | ((g1 & 4) >> 1) | ((g0 & 4) >> 2);
	byte c2 = ((g3 & 2) << 2) | ((g2 & 2) << 1) | ((g1 & 2) << 0) | ((g0 & 2) >> 1);
	byte c3 = ((g3 & 1) << 3) | ((g2 & 1) << 2) | ((g1 & 1) << 1) | ((g0 & 1) << 0);

	accessBitBuffer[0] = (~c2 & 0xF) << 4 | (~c1 & 0xF);
	accessBitBuffer[1] =          c1 << 4 | (~c3 & 0xF);
	accessBitBuffer[2] =          c3 << 4 | c2;
} // End MIFARE_SetAccessBits()


/**
 * Performs the "magic sequence" needed to get Chinese UID changeable
 * Mifare cards to allow writing to sector 0, where the card UID is stored.
 *
 * Note that you do not need to have selected the card through REQA or WUPA,
 * this sequence works immediately when the card is in the reader vicinity.
 * This means you can use this method even on "bricked" cards that your reader does
 * not recognise anymore (see MFRC522::MIFARE_UnbrickUidSector).
 * 
 * Of course with non-bricked devices, you're free to select them before calling this function.
 */
bool mfrc522_mifare_open_uid_backdoor(bool logErrors) {
	// Magic sequence:
	// > 50 00 57 CD (HALT + CRC)
	// > 40 (7 bits only)
	// < A (4 bits only)
	// > 43
	// < A (4 bits only)
	// Then you can write to sector 0 without authenticating

	mfrc522_picc_halt_a(); // 50 00 57 CD

	byte cmd = 0x40;
	byte validBits = 7; /* Our command is only 7 bits. After receiving card response,
						   this will contain amount of valid response bits. */
	byte response[32]; // Card's response is written here
	byte received;
	byte status = mfrc522_pcd_transceive_data(&cmd, (byte)1, response, &received, &validBits, (byte)0, false); // 40
	if(status != STATUS_OK) {
		if(logErrors) {
			printf("Card did not respond to 0x40 after HALT command. Are you sure it is a UID changeable one?");
			printf("Error name: ");
			printf("%s",mfrc522_get_status_code_name(status));
		}
		return false;
	}
	if (received != 1 || response[0] != 0x0A) {
		if(logErrors) {

			printf("Got bad response on backdoor 0x40 command: ");
			printf("0x%02X", response[0]);
			printf(" (");
			printf("%02X", validBits);
			printf(" valid bits)\r\n");
		}
		return false;
	}

	cmd = 0x43;
	validBits = 8;
	status = mfrc522_pcd_transceive_data(&cmd, (byte)1, response, &received, &validBits, (byte)0, false); // 43
	if(status != STATUS_OK) {
		if(logErrors) {
			printf("Error in communication at command 0x43, after successfully executing 0x40");
			printf("Error name: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
		}
		return false;
	}
	if (received != 1 || response[0] != 0x0A) {
		if (logErrors) {
			printf("Got bad response on backdoor 0x43 command: ");
			printf("%02X",response[0]);
			printf(" (");
			printf("%02X",validBits);
			printf(" valid bits)\r\n");
		}
		return false;
	}

	// You can now write to sector 0 without authenticating!
	return true;
} // End MIFARE_OpenUidBackdoor()

/**
 * Reads entire block 0, including all manufacturer data, and overwrites
 * that block with the new UID, a freshly calculated BCC, and the original
 * manufacturer data.
 *
 * It assumes a default KEY A of 0xFFFFFFFFFFFF.
 * Make sure to have selected the card before this function is called.
 */
bool mfrc522_mifare_set_uid(byte *newUid, byte uidSize, bool logErrors) {

	// UID + BCC byte can not be larger than 16 together
	if (!newUid || !uidSize || uidSize > 15) {
		if (logErrors) {
			printf("New UID buffer empty, size 0, or size > 15 given");
		}
		return false;
	}

	// Authenticate for reading
	MIFARE_Key key = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
	byte status = mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, (byte)1, &key, &uid);
	if (status != STATUS_OK) {

		if (status == STATUS_TIMEOUT) {
			// We get a read timeout if no card is selected yet, so let's select one

			// Wake the card up again if sleeping
			//			  byte atqa_answer[2];
			//			  byte atqa_size = 2;
			//			  PICC_WakeupA(atqa_answer, &atqa_size);

			if (!mfrc522_picc_is_new_card_present() || !mfrc522_picc_read_card_serial()) {
				printf("No card was previously selected, and none are available. Failed to set UID.");
				return false;
			}

			status = mfrc522_pcd_authenticate(PICC_CMD_MF_AUTH_KEY_A, (byte)1, &key, &uid);
			if (status != STATUS_OK) {
				// We tried, time to give up
				if (logErrors) {
					printf("Failed to authenticate to card for reading, could not set UID: ");
					printf("%s\n",mfrc522_get_status_code_name(status));
				}
				return false;
			}
		}
		else {
			if (logErrors) {
				printf("PCD_Authenticate() failed: ");
				printf("%s\n",mfrc522_get_status_code_name(status));
			}
			return false;
		}
	}

	// Read block 0
	byte block0_buffer[18];
	byte byteCount = sizeof(block0_buffer);
	status = mfrc522_mifare_read((byte)0, block0_buffer, &byteCount);
	if (status != STATUS_OK) {
		if (logErrors) {
			printf("MIFARE_Read() failed: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
			printf("Are you sure your KEY A for sector 0 is 0xFFFFFFFFFFFF?");
		}
		return false;
	}

	// Write new UID to the data we just read, and calculate BCC byte
	byte bcc = 0;
	int i;
	for (i = 0; i < uidSize; i++) {
		block0_buffer[i] = newUid[i];
		bcc ^= newUid[i];
	}

	// Write BCC byte to buffer
	block0_buffer[uidSize] = bcc;

	// Stop encrypted traffic so we can send raw bytes
	mfrc522_pcd_stop_crypto_1();

	// Activate UID backdoor
	if (!mfrc522_mifare_open_uid_backdoor(logErrors)) {
		if (logErrors) {
			printf("Activating the UID backdoor failed.");
		}
		return false;
	}

	// Write modified block 0 back to card
	status = mfrc522_mifare_write((byte)0, block0_buffer, (byte)16);
	if (status != STATUS_OK) {
		if (logErrors) {
			printf("MIFARE_Write() failed: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
		}
		return false;
	}

	// Wake the card up again
	byte atqa_answer[2];
	byte atqa_size = 2;
	mfrc522_picc_wakeup_a(atqa_answer, &atqa_size);

	return true;
}

/**
 * Resets entire sector 0 to zeroes, so the card can be read again by readers.
 */
bool mfrc522_mifare_unbrick_uid_sector(bool logErrors) {
	mfrc522_mifare_open_uid_backdoor(logErrors);

	byte block0_buffer[] = {0x01, 0x02, 0x03, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	// Write modified block 0 back to card
	byte status = mfrc522_mifare_write((byte)0, block0_buffer, (byte)16);
	if (status != STATUS_OK) {
		if (logErrors) {
			printf("MIFARE_Write() failed: ");
			printf("%s\n",mfrc522_get_status_code_name(status));
		}
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////
// Convenience functions - does not add extra functionality
/////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns true if a PICC responds to PICC_CMD_REQA.
 * Only "new" cards in state IDLE are invited. Sleeping cards in state HALT are ignored.
 * 
 * @return bool
 */
bool mfrc522_picc_is_new_card_present() {
	byte bufferATQA[2];
	byte bufferSize = sizeof(bufferATQA);
	byte result = mfrc522_picc_request_a(bufferATQA, &bufferSize);
	return (result == STATUS_OK || result == STATUS_COLLISION);
} // End PICC_IsNewCardPresent()

/**
 * Simple wrapper around PICC_Select.
 * Returns true if a UID could be read.
 * Remember to call PICC_IsNewCardPresent(), PICC_RequestA() or PICC_WakeupA() first.
 * The read UID is available in the class variable uid.
 * 
 * @return bool
 */
bool mfrc522_picc_read_card_serial() {
	byte result = mfrc522_picc_select(&uid, 0);
	return (result == STATUS_OK);
} // End PICC_ReadCardSerial()

