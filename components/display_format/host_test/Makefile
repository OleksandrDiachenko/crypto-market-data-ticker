CC ?= gcc
SRC_DIR = ../src
BUILD_DIR = build
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I$(SRC_DIR) -I../include
LDLIBS = -lm

TEST_NAMES = \
	test_display_format

TESTS = $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))

.PHONY: test clean

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_display_format: test_display_format.c $(SRC_DIR)/display_format.c test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_display_format.c $(SRC_DIR)/display_format.c $(LDLIBS)

clean:
	rm -rf $(BUILD_DIR)
