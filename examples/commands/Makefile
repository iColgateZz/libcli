CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2
BUILD_DIR = build

.PHONY: all clean

all: $(BUILD_DIR)/commands

$(BUILD_DIR)/commands: main.c ../../cli.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) main.c -o $@

clean:
	rm -rf $(BUILD_DIR)
