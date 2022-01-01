# Makefile from https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
CC=arm-linux-gnueabi-gcc
CXX=arm-linux-gnueabi-g++
TARGET_EXEC ?= kiddyblaster
BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
DATA_DIR ?= ./data

SRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))  -I/home/hannenz/pidev/libs/include

# Flags for C Compiler
CFLAGS:=-Wall -g -O3 

# Preprocessor flags
CPPFLAGS ?= $(INC_FLAGS) -DLIB_PIGPIO

# Linker flags
LDFLAGS:=-L/home/hannenz/pidev/libs/lib -lrt -pthread -lpigpio -lmpdclient -lsqlite3


$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS) 
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# assembly
$(BUILD_DIR)/%.s.o: %.s
	$(MKDIR_P) $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# c source
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# c++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


.PHONY: clean install

writecard: src/writecard/writecard.c src/card.c src/card.h src/mfrc522.c src/mfrc522.h
	$(CC) $(CFLAGS) $(CXXFLAGS) -o $(BUILD_DIR)/writecard src/writecard/writecard.c src/card.c src/mfrc522.c $(INC_FLAGS) $(LDFLAGS) -lsqlite3 -lbcm2835 -lpigpio

install: 
	install -m 755 $(BUILD_DIR)/$(TARGET_EXEC) /usr/local/bin/
	install -m 755 $(BUILD_DIR)/writecard /usr/local/bin/

	# Read-only is o.k.
	[ -e /usr/local/share/kiddyblaster ] || mkdir -m 755 /usr/local/share/kiddyblaster
	install -m 644 $(DATA_DIR)/success.wav /usr/local/share/kiddyblaster/

	# Needs to be writable
	[ -e /var/lib/kiddyblaster ] || mkdir -m 755 /var/lib/kiddyblaster
	# Only install if not exists yet, we don't want to overwrite an existing database!
	[ -e /var/lib/kiddyblaster/cards.sql ] || install -m 644 $(DATA_DIR)/cards.sql /var/lib/kiddyblaster/

	# Install systemd service 
	install -m 644 $(DATA_DIR)/kiddyblaster.service /etc/systemd/system/
	/bin/systemctl enable kiddyblaster.service

	# Install mpd.conf
	install -m 644 $(DATA_DIR)/mpd.conf /etc/

REMOTE_HOST=pi@kiddyblaster
remoteinstall:
	scp -r build $(REMOTE_HOST)-simon:/tmp/
	ssh $(REMOTE_HOST)-simon "sudo install -m 755 /tmp/build/kiddyblaster /usr/local/bin/"
	ssh $(REMOTE_HOST)-simon "sudo install -m 755 /tmp/build/writecard /usr/local/bin/"


devinstall:
	/bin/systemctl stop kiddyblaster.service
	install -m 755 $(BUILD_DIR)/$(TARGET_EXEC) /usr/local/bin/
	install -m 755 $(BUILD_DIR)/writecard /usr/local/bin/
	/bin/systemctl start kiddyblaster.service


uninstall:
	/bin/systemctl disable kiddyblaster.service
	rm -f /usr/local/bin/kiddyblaster
	rm -f /usr/local/bin/writecard
	rm -rf /usr/local/share/kiddyblaster
	rm -rf /var/lib/kiddyblaster

webui:
	export NODE_ENV=production
	cd ./webui
	npm install

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
