#!/bin/bash
# 从鲁班猫提取 GStreamer/X11/glib 头文件和库，构建 ARM64 交叉编译 sysroot
# 用法：./sysroot-populate.sh
# 前置：能免密 SSH 到 cat@192.168.137.100

set -e

BOARD="cat@192.168.137.100"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SYSROOT="$SCRIPT_DIR/../sysroot"
INCLUDE_DIR="$SYSROOT/usr/aarch64-linux-gnu/include"
LIB_DIR="$SYSROOT/usr/aarch64-linux-gnu/lib"
RKNNSDK="$SCRIPT_DIR/../../references/lubancat_ai_manual_code/dev_env/rknpu2/runtime/RK3588/Linux/librknn_api"

echo "=== sysroot-populate: 从 $BOARD 提取 ARM64 sysroot ==="

# 清理旧 sysroot
rm -rf "$SYSROOT"
mkdir -p "$INCLUDE_DIR" "$LIB_DIR"

# ---- 1: 从鲁班猫复制头文件 ----
echo "[1/4] 复制头文件..."
scp -r -q "$BOARD:/usr/include/gstreamer-1.0" "$INCLUDE_DIR/"
scp -r -q "$BOARD:/usr/include/glib-2.0"       "$INCLUDE_DIR/"
scp -r -q "$BOARD:/usr/include/X11"            "$INCLUDE_DIR/"

# glibconfig.h 不在 /usr/include，需要单独处理
mkdir -p "$INCLUDE_DIR/glib-2.0"
scp -q "$BOARD:/usr/lib/aarch64-linux-gnu/glib-2.0/include/glibconfig.h" "$INCLUDE_DIR/glib-2.0/"

echo "  头文件复制完成"

# ---- 2: 从鲁班猫复制共享库 ----
echo "[2/4] 复制共享库..."

# 用 tar+ssh 复制库文件，保持符号链接
ssh "$BOARD" "cd /usr/lib/aarch64-linux-gnu && tar chf - \
    libgstreamer-1.0.so* \
    libgstapp-1.0.so* \
    libgstbase-1.0.so* \
    libgstvideo-1.0.so* \
    libgstaudio-1.0.so* \
    libgstpbutils-1.0.so* \
    libgsttag-1.0.so* \
    libgstallocators-1.0.so* \
    libgstgl-1.0.so* \
    libglib-2.0.so* \
    libgobject-2.0.so* \
    libgmodule-2.0.so* \
    libgio-2.0.so* \
    libgthread-2.0.so* \
    libX11.so* \
    libX11-xcb.so* \
    libxcb.so* \
    libxcb-shm.so* \
    libxcb-render.so* \
    libxcb-util.so* \
    libxcb-keysyms.so* \
    libxcb-shape.so* \
    libxcb-xfixes.so* \
    libXau.so* \
    libXdmcp.so* \
    libXext.so* \
    libffi.so* \
    libpcre.so* \
    libz.so* \
    libmount.so* \
    libselinux.so* \
    libpcre2-8.so* \
    libEGL.so* \
    libGL.so* \
    libdrm.so* \
    libfreetype.so* \
    libpng16.so* \
    libbrotlidec.so* \
    libbrotlicommon.so* \
    2>/dev/null" | tar xf - -C "$LIB_DIR"

echo "  共享库复制完成"

# ---- 3: 从本地 rknpu2 SDK 复制 RKNN 文件 ----
echo "[3/4] 复制 RKNN 头文件和库..."

if [ -f "$RKNNSDK/include/rknn_api.h" ]; then
    cp "$RKNNSDK/include/rknn_api.h" "$INCLUDE_DIR/"
    cp "$RKNNSDK/include/rknn_matmul_api.h" "$INCLUDE_DIR/" 2>/dev/null || true
    cp "$RKNNSDK/aarch64/librknnrt.so" "$LIB_DIR/"
    cp "$RKNNSDK/aarch64/librknn_api.so" "$LIB_DIR/" 2>/dev/null || true
    echo "  RKNN 文件复制完成 (from SDK)"
else
    echo "  [WARN] rknpu2 SDK 未找到, 尝试从板子复制..."
    scp -q "$BOARD:/usr/lib/librknnrt.so" "$LIB_DIR/"
    scp -q "$BOARD:/usr/local/include/rknn_api.h" "$INCLUDE_DIR/" 2>/dev/null || true
fi

# ---- 4: 验证 sysroot 完整性 ----
echo "[4/4] 验证 sysroot..."

check_header() { [ -f "$1" ] && echo "  OK  $1" || echo "  FAIL $1"; }
check_lib()   { [ -f "$1" -o -L "$1" ] && echo "  OK  $1" || echo "  FAIL $1"; }

echo "-- 关键头文件 --"
check_header "$INCLUDE_DIR/gstreamer-1.0/gst/gst.h"
check_header "$INCLUDE_DIR/glib-2.0/glib.h"
check_header "$INCLUDE_DIR/glib-2.0/glibconfig.h"
check_header "$INCLUDE_DIR/X11/Xlib.h"
check_header "$INCLUDE_DIR/X11/Xutil.h"
check_header "$INCLUDE_DIR/X11/keysym.h"
check_header "$INCLUDE_DIR/rknn_api.h"

echo "-- 关键共享库 --"
check_lib "$LIB_DIR/libgstreamer-1.0.so"
check_lib "$LIB_DIR/libglib-2.0.so"
check_lib "$LIB_DIR/libgobject-2.0.so"
check_lib "$LIB_DIR/libX11.so"
check_lib "$LIB_DIR/libxcb.so"
check_lib "$LIB_DIR/librknnrt.so"

echo ""
echo "=== sysroot 就绪: $SYSROOT ==="
du -sh "$SYSROOT"
