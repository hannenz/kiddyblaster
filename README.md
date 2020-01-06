# KiddyBlaster

RFID driven musicbox for children

## Parts

- RFID-Reader: MFRC522 and some RFID cards
- LCD (optional): I2C 2x16 LCD 
- USB Audio / Soundcard
- Amplifier, e.g. [DEBO Sound Amp2, 3.7 W Class D Amplifier](https://www.reichelt.de/entwicklerboards-audioverstaerker-stereo-3-7-w-klasse-d-max-debo-sound-amp2-p235507.html?)
- Speakers, e.g. [VIS 4629 77mm 5W](https://www.reichelt.de/lautsprecher-breitband-77-mm-5-w-vis-4629-p248312.html?)
- 3 Arcade buttons, e.g. [](https://www.reichelt.de/drucktaster-4a-250vac-1x-ein-21-16mm-sw-mar-5000-0104-p108204.html?)
- 1 Stereo Potentiometer for volume control (50 KOhm logarithmic), e.g. [](https://www.reichelt.de/drehpotentiometer-stereo-50-kohm-logarithmisch-6-mm-rk14k12b-log50k-p73862.html?)
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
sudo apt install libmpdclient-dev pigpio libsqlite3-dev libglib2.0-dev
```

and follow the instructions on `http://www.airspayce.com/mikem/bcm2835/` to install the bcm2835 library

Then you will need mpd and optionally mpc:

```
sudo apt install mpd mpc
```

## Installation

### Raspberry Pi

Install Raspbian on a SD Card. I used 8.0 (Jessie) but newer releases should work, too)
Update 2019-12-01: Tested on Raspbian Buster Lite

Launch `raspi-config` and do:

- Expand filesystem

- Setup hostname, change password (optionally, but recommended)

- Boot options: Auto-login, don't wait for network

- Enable SSH

- Enable SPI

- Enable I2C (optionally, when using a 16x2 Display over I2C)


Clone this repository and run `make`, then `sudo make install`

Reboot


Update 2019-12-02: Installation step-by-step

- Raspberry II

- Downloaded Raspbian Buster Lite (10.0)

- `sudo dd bs=4M if=2019-09-26-raspbian-buster.img of=/dev/mmcblk0 conv=fsync`

- boot

- raspi-config, steps see above + memory split: 16M for GPU

```
sudo apt update && sudo appt upgrade
sudo apt-get --no-install-recommends install git mpc mpd pigpio libmpdclient-dev libsqlite3-dev libglib2.0-dev
cd
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.60.tar.gz`
tar zxvf bcm2835-1.60.tar.gz
cd bcm2835-1.60
./configure
make
sudo make check
sudo make install
cd
git clone https://github.com/hannenz/kiddyblaster
cd ~/kiddyblaster
make
sudo make install
sudo reboot
```



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
It might be necessary to restart the kiddyblaster daemon after writing a card:

```
sudo systemctl restart kiddyblaster.service
```


### Programming cards with the WebUI

The webui is still under development. To try out you can install the webui:

```
sudo apt install nodejs npm
cd webui
npm install -g gulp-cli
npm install
```
This will take a time!

Launch the web app:
```
nodejs main.js
```

Then open a browser and open `ip-address-of-raspi:4444` 


## Todos

- Headphones
- Shorter press delay (700 is too much)
- LCD scroll long lines
- Finish  WebUI
- Button labels

	
