#!/bin/bash

# ESP32项目bin文件复制脚本
# 用于将构建后的bin文件复制到output目录

# 设置颜色输出
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT=$(pwd)
BUILD_DIR="${PROJECT_ROOT}/build"
OUTPUT_DIR="${PROJECT_ROOT}/output"

echo "================================"
echo "ESP32 Bin文件复制工具"
echo "================================"

# 检查build目录是否存在
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}错误: build目录不存在，请先编译项目${NC}"
    exit 1
fi

# 创建output目录
if [ ! -d "$OUTPUT_DIR" ]; then
    echo "创建output目录..."
    mkdir -p "$OUTPUT_DIR"
fi

# 清空output目录（可选，根据需要取消注释）
# rm -rf "$OUTPUT_DIR"/*

# 复制bin文件
echo ""
echo "开始复制bin文件..."

# 复制主程序bin
if [ -f "$BUILD_DIR/N3Lite-1.0.bin" ]; then
    cp "$BUILD_DIR/N3Lite-1.0.bin" "$OUTPUT_DIR/"
    echo -e "${GREEN}✓${NC} N3Lite-1.0.bin"
else
    echo -e "${RED}✗${NC} N3Lite-1.0.bin (文件不存在)"
fi

# 复制bootloader
if [ -f "$BUILD_DIR/bootloader/bootloader.bin" ]; then
    cp "$BUILD_DIR/bootloader/bootloader.bin" "$OUTPUT_DIR/"
    echo -e "${GREEN}✓${NC} bootloader.bin"
else
    echo -e "${RED}✗${NC} bootloader.bin (文件不存在)"
fi

# 复制分区表
if [ -f "$BUILD_DIR/partition_table/partition-table.bin" ]; then
    cp "$BUILD_DIR/partition_table/partition-table.bin" "$OUTPUT_DIR/"
    echo -e "${GREEN}✓${NC} partition-table.bin"
else
    echo -e "${RED}✗${NC} partition-table.bin (文件不存在)"
fi

# 复制OTA数据初始化文件
if [ -f "$BUILD_DIR/ota_data_initial.bin" ]; then
    cp "$BUILD_DIR/ota_data_initial.bin" "$OUTPUT_DIR/"
    echo -e "${GREEN}✓${NC} ota_data_initial.bin"
else
    echo -e "${RED}✗${NC} ota_data_initial.bin (文件不存在)"
fi

echo ""
echo "================================"
echo "复制完成！"
echo "输出目录: $OUTPUT_DIR"
echo "================================"

# 显示output目录内容
echo ""
echo "output目录内容:"
ls -lh "$OUTPUT_DIR"
