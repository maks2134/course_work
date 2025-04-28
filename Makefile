CC := gcc
CFLAGS := -Wall -Wextra -pedantic -std=c11 -g -I. -Isrc
LDFLAGS := -lm

SRC_DIR := src
BUILD_DIR := build

TARGET := client
TARGET_EXE := $(BUILD_DIR)/$(TARGET)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(TARGET_EXE)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_EXE): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)