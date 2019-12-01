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

... sound quality: It's not as decent as I wished it would be, don't expect
HiFi. The speakers are really not so well. But it's ok for listening to
audiobooks and some music

... WiFi?  I decided to not spoil my children's room with WiFi and to keep my
box without WiFi, however adding WiFi is easy (search for it in the internets)
Thus I decidede to add a RJ52 plug for maintenance.


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

Then you will need mpd and optionally mpc:

```
sudo apt install mpd mpc
```

## Installation

Run `make`, then `sudo make install`

## Usage

When booting the Raspberry Pi it will automatically launch the kiddyplayer daemon and MPD (Music Player Daemon).
MPD will look for the music library in `/home/pi/Music`, so put your audio files there (subdirectories are fine).
To program the cards there is a cli utility `writecard` and a webui.

### Programming cards with the cli utility `writecard`

SSH onto the Kiddyblaster box and issue:

```
sudo writecard "{name of the card}" "path/to/directory"
```

where the name of the card is any arbitrary name (not used at the moment) and
the path to a directory containing the desired audio files, relative to the
music library directory (`/home/pi/Music`) and without any leading or trailing
slashes, e.g.

```
sudo writecard "Das Dschungelbuch" "Audiobooks/Das Dschungelbuch"
```

Now place a card near the RFID chip and it will be programmed to play the audio files in this directory from the next time on.

