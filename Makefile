CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = build

ALL_SRCS = $(wildcard $(SRC_DIR)/*.c)
CORE_SRCS = $(filter-out $(SRC_DIR)/main.c, $(ALL_SRCS))
CORE_OBJS = $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

MAIN_OBJ = $(BUILD_DIR)/main.o

TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/$(TEST_DIR)/%.o)

TARGET = $(BUILD_DIR)/main
TEST_TARGET = $(BUILD_DIR)/$(TEST_DIR)/test

all: $(TARGET)

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_TARGET)
	./$<

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(TEST_DIR)/%.o: $(TEST_DIR)/%.c
	@mkdir -p $(BUILD_DIR)/$(TEST_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean run test
