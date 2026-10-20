CC = cc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L
BUILD_DIR = build

.PHONY: all clean format

all:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) main.c timeout.c -o $(BUILD_DIR)/timeout

format:
	clang-format -i main.c timeout.c timeout.h list.h

clean:
	rm -rf $(BUILD_DIR)
