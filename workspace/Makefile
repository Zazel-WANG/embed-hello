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

# --- AI 交叉编译目标 (sysroot = /usr/aarch64-linux-gnu/) ---
# 动态链接 —— GStreamer 插件系统和 RKNN 驱动无法静态链接
AI_SRC_DIR := src/ai
AI_INC_DIR := include/ai
AI_SYSROOT := /usr/aarch64-linux-gnu
GST_INC := $(AI_SYSROOT)/include/gstreamer-1.0
GLIB_INC := $(AI_SYSROOT)/include/glib-2.0

AI_CFLAGS := -Wall -O2 \
	-I$(AI_INC_DIR) \
	-I$(GST_INC) \
	-I$(GLIB_INC)

# 运行时库 (板上 Debian 11 自带)
AI_LIBS := -lrknnrt -ljpeg -lgstreamer-1.0 -lglib-2.0 -lgobject-2.0 -lgmodule-2.0 -lX11 -lm

$(BUILD_DIR)/yolov5_post.o: $(AI_SRC_DIR)/yolov5_post.c $(AI_SRC_DIR)/yolov5_post.h
	$(CC) $(AI_CFLAGS) -c $< -o $@

# ---- 独立模块 ----
ai-query-cross: $(AI_SRC_DIR)/rknn_query_demo.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(AI_CFLAGS) $< -o $(BUILD_DIR)/$@ $(AI_LIBS)
	@file $(BUILD_DIR)/$@

ai-infer-cross: $(AI_SRC_DIR)/rknn_infer_demo.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(AI_CFLAGS) $< -o $(BUILD_DIR)/$@ $(AI_LIBS)
	@file $(BUILD_DIR)/$@

ai-gst-cross: $(AI_SRC_DIR)/gst_camera_test.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(AI_CFLAGS) $< -o $(BUILD_DIR)/$@ $(AI_LIBS)
	@file $(BUILD_DIR)/$@

ai-x11-cross: $(AI_SRC_DIR)/x11_display_test.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(AI_CFLAGS) $< -o $(BUILD_DIR)/$@ $(AI_LIBS)
	@file $(BUILD_DIR)/$@

# ---- 全链路集成 ----
ai-full-cross: $(AI_SRC_DIR)/ai_demo.c $(BUILD_DIR)/yolov5_post.o
	mkdir -p $(BUILD_DIR)
	$(CC) $(AI_CFLAGS) $^ -o $(BUILD_DIR)/$@ $(AI_LIBS)
	@echo "=== AI full demo cross-compiled ==="
	@file $(BUILD_DIR)/$@

# ---- 全部 AI 目标 ----
ai-all-cross: ai-query-cross ai-infer-cross ai-gst-cross ai-x11-cross ai-full-cross

ai-cross-clean:
	rm -f $(BUILD_DIR)/ai-*-cross $(BUILD_DIR)/yolov5_post.o
