# 编译器设置
CC = gcc
CFLAGS = -Wall -g -std=c99 -O2

# 使用 pkg-config 获取 gstreamer-1.0 和 gstreamer-rtsp-server-1.0 的编译和链接标志
CFLAGS += $(shell pkg-config --cflags gstreamer-1.0 gstreamer-rtsp-server-1.0)
LDLIBS += $(shell pkg-config --libs gstreamer-1.0 gstreamer-rtsp-server-1.0 glib-2.0)

# 目标
TARGET = main.out
SOURCES = main.c gst-media.c gst-player.c gst-recorder.c gst-rtsp-server.c
OBJECTS = $(SOURCES:.c=.o)

# 默认目标
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDLIBS)

# 编译 .c 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJECTS) $(TARGET)

# 重新构建
rebuild: clean all

.PHONY: all clean rebuild