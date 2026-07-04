CC ?= gcc
SRC_DIR = ../src
BUILD_DIR = build
CFLAGS = -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I$(SRC_DIR) \
	-I../../market_data_client/include \
	-I../../market_data_ws_client/include \
	-I../../settings_store/include

RETRY_POLICY_SOURCES = $(SRC_DIR)/app_state_retry_policy.c
KLINE_MERGE_SOURCES = $(SRC_DIR)/app_state_kline_merge.c

TEST_NAMES = \
	test_app_state_retry_policy \
	test_app_state_kline_merge

TESTS = $(addprefix $(BUILD_DIR)/,$(TEST_NAMES))

.PHONY: test clean

test: $(TESTS)
	@set -e; for t in $(TESTS); do ./$$t; done

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_app_state_retry_policy: test_app_state_retry_policy.c $(RETRY_POLICY_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_app_state_retry_policy.c $(RETRY_POLICY_SOURCES)

$(BUILD_DIR)/test_app_state_kline_merge: test_app_state_kline_merge.c $(KLINE_MERGE_SOURCES) test_common.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ test_app_state_kline_merge.c $(KLINE_MERGE_SOURCES)

clean:
	rm -rf $(BUILD_DIR)
