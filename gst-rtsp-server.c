#include "gst-rtsp-server.h"
#include <string.h>

// 创建RTSP流的bin，用于连接到media的tee
GstElement* create_rtsp_stream_bin()
{
    GstElement *bin = gst_bin_new("rtsp_stream_bin");
    
    // 创建视频队列
    GstElement *v_queue = gst_element_factory_make("queue", "rtsp_v_queue");
    
    // 创建音频队列
    GstElement *a_queue = gst_element_factory_make("queue", "rtsp_a_queue");
    
    if (!bin || !v_queue || !a_queue)
    {
        g_printerr("Could not create RTSP stream basic elements\n");
        if (bin) gst_object_unref(bin);
        return NULL;
    }

    // 添加元素到bin
    gst_bin_add_many(GST_BIN(bin),
        v_queue, a_queue,
        NULL);

    // 创建ghost pads - 一个用于视频，一个用于音频
    GstPad *v_pad = gst_element_get_static_pad(v_queue, "sink");
    GstPad *a_pad = gst_element_get_static_pad(a_queue, "sink");
    GstPad *v_ghost_pad = gst_ghost_pad_new("v_sink", v_pad);  // 统一使用v_sink和a_sink
    GstPad *a_ghost_pad = gst_ghost_pad_new("a_sink", a_pad);  // 统一使用v_sink和a_sink
    gst_element_add_pad(bin, v_ghost_pad);
    gst_element_add_pad(bin, a_ghost_pad);
    gst_pad_set_active(v_ghost_pad, TRUE);
    gst_pad_set_active(a_ghost_pad, TRUE);
    gst_object_unref(v_pad);
    gst_object_unref(a_pad);

    return bin;
}

gboolean rtsp_server_init(GstRtspServer *self, guint port)
{
    if (!self)
    {
        g_printerr("RTSP Server instance is NULL\n");
        return FALSE;
    }

    memset(self, 0, sizeof(GstRtspServer));

    self->port = port;
    self->uri_path = g_strdup("/stream");  // 默认流路径
    self->is_streaming = FALSE;
    
    self->server = NULL;

    g_print("RTSP Server initialized on port %u\n", port);
    return TRUE;
}

void rtsp_server_destroy(GstRtspServer *self)
{
    if (!self)
        return;

    if (self->is_streaming) {
        rtsp_stop(self);
    }

    if (self->bin)
    {
        gst_element_set_state(self->bin, GST_STATE_NULL);
        gst_object_unref(self->bin);
        self->bin = NULL;
    }

    if (self->uri_path)
    {
        g_free(self->uri_path);
        self->uri_path = NULL;
    }
    
    g_print("RTSP Server destroyed\n");
}

gboolean rtsp_link(GstRtspServer *self, GstMedia *media)
{
    if (!self || !media || !media->pipeline)
    {
        g_printerr("Invalid arguments to rtsp_link\n");
        return FALSE;
    }

    // 创建RTSP流的bin
    self->bin = create_rtsp_stream_bin();
    if (!self->bin)
    {
        g_printerr("Could not create RTSP stream bin\n");
        return FALSE;
    }

    // 添加视频分支和音频分支
    gboolean video_success = media_add_video_branch(media, self->bin);
    gboolean audio_success = media_add_audio_branch(media, self->bin);

    if (!(video_success && audio_success)) {
        g_printerr("Failed to link RTSP stream bin to media\n");
        gst_element_set_state(self->bin, GST_STATE_NULL);
        gst_object_unref(self->bin);
        self->bin = NULL;
        return FALSE;
    }

    g_print("RTSP server successfully linked to media\n");
    return TRUE;
}

gboolean rtsp_unlink(GstRtspServer *self, GstMedia *media)
{
    if (!self || !media || !media->pipeline || !self->bin)
    {
        g_printerr("Invalid arguments to rtsp_unlink\n");
        return FALSE;
    }

    // 移除视频分支和音频分支
    gboolean video_success = media_remove_video_branch(media, self->bin);
    gboolean audio_success = media_remove_audio_branch(media, self->bin);

    gst_element_set_state(self->bin, GST_STATE_NULL);
    gst_object_unref(self->bin);
    self->bin = NULL;

    return video_success && audio_success;
}

gboolean rtsp_start(GstRtspServer *self)
{
    if (!self)
    {
        g_printerr("RTSP server instance is NULL\n");
        return FALSE;
    }

    self->is_streaming = TRUE;
    g_print("RTSP server started on port %u, path %s\n", 
            self->port, self->uri_path);
    g_print("Connect using: rtsp://127.0.0.1:%u%s\n", 
            self->port, self->uri_path);

    return TRUE;
}

gboolean rtsp_stop(GstRtspServer *self)
{
    if (!self)
    {
        g_printerr("RTSP server instance is NULL\n");
        return FALSE;
    }

    if (!self->is_streaming) {
        return TRUE;
    }

    self->is_streaming = FALSE;
    g_print("RTSP server stopped\n");

    return TRUE;
}