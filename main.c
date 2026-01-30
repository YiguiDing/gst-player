#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>
#include "gst-media.h"
#include "gst-player.h"
#include "gst-recorder.h"
#include "gst-rtsp-server.h"

static gboolean quit_func(gpointer data)
{
    GMainLoop *loop = (GMainLoop *)data;
    g_main_loop_quit(loop);
    return FALSE;
}

int main(int argc, char *argv[])
{
    // 初始化GStreamer
    gst_init(&argc, &argv);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // 创建并初始化媒体实例
    GstMedia media;
    if (!media_init(&media)) {
        g_printerr("Failed to initialize media\n");
        return -1;
    }

    // 设置媒体URI（使用测试源）
    if (!media_set_uri(&media, "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm")) {
        g_printerr("Failed to set media URI\n");
        media_destroy(&media);
        return -1;
    }

    // 创建并初始化播放器实例
    GstPlayer player;
    if (!player_init(&player)) {
        g_printerr("Failed to initialize player\n");
        media_destroy(&media);
        return -1;
    }

    // 创建并初始化录制器实例
    GstRecorder recorder;
    if (!recorder_init(&recorder)) {
        g_printerr("Failed to initialize recorder\n");
        player_destroy(&player);
        media_destroy(&media);
        return -1;
    }

    // 创建并初始化RTSP服务器实例
    GstRtspServer rtsp_server;
    if (!rtsp_server_init(&rtsp_server, 8554)) {
        g_printerr("Failed to initialize RTSP server\n");
        recorder_destroy(&recorder);
        player_destroy(&player);
        media_destroy(&media);
        return -1;
    }

    // 链接各组件到媒体源
    if (!player_link(&player, &media)) {
        g_printerr("Failed to link player to media\n");
    }

    if (!recorder_link(&recorder, &media)) {
        g_printerr("Failed to link recorder to media\n");
    }

    if (!rtsp_link(&rtsp_server, &media)) {
        g_printerr("Failed to link RTSP server to media\n");
    }

    // 启动录制（在启动播放之前，这样可以确保tee已经准备好）
    if (!recorder_start(&recorder, "output.mp4")) {
        g_printerr("Failed to start recording\n");
    }

    // 启动RTSP服务器
    if (!rtsp_start(&rtsp_server)) {
        g_printerr("Failed to start RTSP server\n");
    }

    // 最后启动播放
    if (!media_play(&media)) {
        g_printerr("Failed to start playback\n");
    }

    g_print("Playback, recording and RTSP streaming started. Press Ctrl+C to exit.\n");

    // 运行主循环
    g_main_loop_run(loop);

    // 清理资源
    rtsp_stop(&rtsp_server);
    recorder_stop(&recorder);
    media_stop(&media);

    rtsp_server_destroy(&rtsp_server);
    recorder_destroy(&recorder);
    player_destroy(&player);
    media_destroy(&media);

    g_main_loop_unref(loop);

    return 0;
}