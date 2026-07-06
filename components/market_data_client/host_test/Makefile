CC ?= gcc
SRC_DIR = ../src
BUILD_DIR = build
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I$(SRC_DIR) -I../include

SCANNER_SOURCES = $(SRC_DIR)/market_data_json_scanner.c

TEST_NAMES = \
	test_market_data_url \
	test_market_data_json_scanner \
	test_market_data_symbol_parser \
	test_market_data_klines_parser \
	test_market_data_ticker_parser

TESTS = $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))

.PHONY: test clean

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_market_data_url: test_market_data_url.c $(SRC_DIR)/market_data_url.c test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_url.c $(SRC_DIR)/market_data_url.c

$(BUILD_DIR)/test_market_data_json_scanner: test_market_data_json_scanner.c $(SCANNER_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_json_scanner.c $(SCANNER_SOURCES)

$(BUILD_DIR)/test_market_data_symbol_parser: test_market_data_symbol_parser.c $(SRC_DIR)/market_data_symbol_parser.c $(SCANNER_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_symbol_parser.c $(SRC_DIR)/market_data_symbol_parser.c $(SCANNER_SOURCES)

$(BUILD_DIR)/test_market_data_klines_parser: test_market_data_klines_parser.c $(SRC_DIR)/market_data_klines_parser.c $(SCANNER_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_klines_parser.c $(SRC_DIR)/market_data_klines_parser.c $(SCANNER_SOURCES)

$(BUILD_DIR)/test_market_data_ticker_parser: test_market_data_ticker_parser.c $(SRC_DIR)/market_data_ticker_parser.c $(SCANNER_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_ticker_parser.c $(SRC_DIR)/market_data_ticker_parser.c $(SCANNER_SOURCES)

clean:
	rm -rf $(BUILD_DIR)
