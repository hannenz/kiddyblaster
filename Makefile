# Makefile from https://spin.atomicobject.com/2016/08/26/makefile-c-projects/
TARGET_EXEC ?= kiddyblaster

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -name *.cpp -or -name *.c -or -name *.s)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS ?= $(INC_FLAGS) -MMD -MP -Wall
CFLAGS:=`pkg-config --cflags glib-2.0`
LDFLAGS:=-pthread -lpigpio -lmpdclient -lbcm2835 -lrt -lsqlite3 `pkg-config --libs glib-2.0`


$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS) writecard
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
	$(CC) -o $(BUILD_DIR)/writecard src/writecard/writecard.c src/card.c src/mfrc522.c -lsqlite3 -lbcm2835 -lpigpio

install: $(BUILD_DIR)/$(TARGET_EXEC) $(BUILD_DIR)/writecard
	install -m 755 $(BUILD_DIR)/$(TARGET_EXEC) /usr/local/bin/
	install -m 755 $(BUILD_DIR)/writecard /usr/local/bin/
	# todo: Install systemd service 

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
