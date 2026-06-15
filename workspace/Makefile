# 交叉编译工具链
CROSS_COMPILE ?= aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc

# 构建配置
ARCH ?= arm64
VERSION ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo "dev")
CFLAGS := -static -Wall -Wextra -O2
TARGET := hello-$(VERSION)

# 目录
SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build
TEST_DIR := tests

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test verify

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)

# 单元测试（qemu-aarch64 模拟运行）
test: $(BUILD_DIR)/test_main
	@echo "=== 单元测试 ===" && \
	qemu-aarch64 $(BUILD_DIR)/test_main

$(BUILD_DIR)/test_main: $(TEST_DIR)/test_main.c $(filter-out $(BUILD_DIR)/main.o, $(OBJS)) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -o $@ $^

# 二进制验证
verify: $(BUILD_DIR)/$(TARGET)
	@echo "=== Binary Info ===" && \
	file $(BUILD_DIR)/$(TARGET) && \
	echo "=== Expected: ELF 64-bit LSB executable, ARM aarch64 ==="
