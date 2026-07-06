CC ?= gcc
SRC_DIR = ../src
MDC_SRC_DIR = ../../market_data_client/src
BUILD_DIR = build
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
	-I$(SRC_DIR) -I../include \
	-I../../market_data_client/include -I$(MDC_SRC_DIR) \
	-I../../settings_store/include

SCANNER_SOURCES = $(MDC_SRC_DIR)/market_data_json_scanner.c

TEST_NAMES = \
	test_market_data_ws_url \
	test_market_data_ws_stream_parser \
	test_market_data_ws_control

TESTS = $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))

.PHONY: test clean

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_market_data_ws_url: test_market_data_ws_url.c $(SRC_DIR)/market_data_ws_url.c test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_ws_url.c $(SRC_DIR)/market_data_ws_url.c

$(BUILD_DIR)/test_market_data_ws_stream_parser: test_market_data_ws_stream_parser.c $(SRC_DIR)/market_data_ws_stream_parser.c $(SCANNER_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_ws_stream_parser.c $(SRC_DIR)/market_data_ws_stream_parser.c $(SCANNER_SOURCES)

$(BUILD_DIR)/test_market_data_ws_control: test_market_data_ws_control.c $(SRC_DIR)/market_data_ws_control.c test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_market_data_ws_control.c $(SRC_DIR)/market_data_ws_control.c

clean:
	rm -rf $(BUILD_DIR)
