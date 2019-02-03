# KiddyBlaster

RFID driven musicbox for children

## Parts

- RFID-Reader: MFRC522
- LCD (optional): I2C 2x16 LCD 
- USB Audio / Soundcard
- Amplifier
- Speakers
- 3 Arcade buttons
- Potentiometer for volume control
- RJ52 Jack and short patch cable
- Case: I've done it with PVC, could also be done with wood or use/upcycle an old plastic case or whatever...


## Some words on ...

... sound quality: It's not as decent as I wished it would be, don't expect HiFi. The speakers are really not so well. But it's ok for listening to audiobooks and some music

... WiFi?  I decided to not spoil my children's room with WiFi and to keep my box without WiFi, however adding WiFi is easy (search for it in the internets) Thus I decidede to add a RJ52 plug for maintenance.


## Dependencies

- libmpdclient-dev
- pigpio
- bcm2835 (Todo: Get rid of one of the two...)
- libsqlite3-dev
- libglib2.0-dev

On raspbian with apt:

```
sudo apt install libmpdclient-dev libsqlite3-dev libglib2.0-dev
```

and follow the instructions on `http://www.airspayce.com/mikem/bcm2835/` to install the bcm2835 library

## Installation

Run `make`, then `sudo make install`

## Usage


