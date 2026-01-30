#include "gst-recorder.h"
#include "gst-media.h"
#include <string.h>

gboolean recorder_init(GstRecorder *self)
{
    if (!self)
    {
        g_printerr("Recorder instance is NULL\n");
        return FALSE;
    }

    memset(self, 0, sizeof(GstRecorder));

    // 创建元素
    self->bin = GST_BIN(gst_bin_new("recorder_bin"));

    self->v_queue = gst_element_factory_make("queue", "rec_v_queue");
    self->v_convert = gst_element_factory_make("videoconvert", "rec_v_convert");
    self->v_encoder = gst_element_factory_make("x264enc", "rec_v_encoder");

    self->a_queue = gst_element_factory_make("queue", "rec_a_queue");
    self->a_convert = gst_element_factory_make("audioconvert", "rec_a_convert");
    self->a_encoder = gst_element_factory_make("avenc_aac", "rec_a_encoder");

    self->mp4mux = gst_element_factory_make("mp4mux", "rec_mp4mux");
    self->filesink = gst_element_factory_make("filesink", "rec_filesink");

    if (
        !self->bin ||
        !self->v_queue || !self->v_convert || !self->v_encoder ||
        !self->a_queue || !self->a_convert || !self->a_encoder ||
        !self->mp4mux || !self->filesink)
    {
        g_printerr("Could not create recording elements.\n");
        recorder_destroy(self);
        return FALSE;
    }

    // 添加到bin
    gst_bin_add_many(GST_BIN(self->bin),
                     self->v_queue, self->v_convert, self->v_encoder,
                     self->a_queue, self->a_convert, self->a_encoder,
                     self->mp4mux, self->filesink,
                     NULL);

    // 配置mp4mux为流式传输模式
    g_object_set(self->mp4mux, "streamable", TRUE, "fragment-duration", 1000, NULL);

    // 连接元素
    if (
        !gst_element_link_many(self->v_queue, self->v_convert, self->v_encoder, self->mp4mux, NULL) ||
        !gst_element_link_many(self->a_queue, self->a_convert, self->a_encoder, self->mp4mux, NULL) ||
        !gst_element_link_many(self->mp4mux, self->filesink, NULL))
    {
        g_printerr("Elements could not be linked.\n");
        recorder_destroy(self);
        return FALSE;
    }

    g_object_set(self->v_encoder, "speed-preset", 1, "tune", 0x00000004, "bitrate", 500, NULL);
    g_object_set(self->a_encoder, "bitrate", 128000, NULL);
    g_object_set(self->mp4mux, "faststart", TRUE, NULL);

    // 创建ghost pads
    GstPad *v_pad = gst_element_get_static_pad(self->v_queue, "sink");
    GstPad *a_pad = gst_element_get_static_pad(self->a_queue, "sink");
    GstPad *v_ghost_pad = gst_ghost_pad_new("v_sink", v_pad);  // 统一使用v_sink和a_sink
    GstPad *a_ghost_pad = gst_ghost_pad_new("a_sink", a_pad);  // 统一使用v_sink和a_sink
    gst_element_add_pad(GST_ELEMENT(self->bin), v_ghost_pad);
    gst_element_add_pad(GST_ELEMENT(self->bin), a_ghost_pad);
    gst_pad_set_active(v_ghost_pad, TRUE);
    gst_pad_set_active(a_ghost_pad, TRUE);
    gst_object_unref(v_pad);
    gst_object_unref(a_pad);

    self->state = RECORDER_STATE_STOPPED;
    self->filename = NULL;

    return TRUE;
}

void recorder_destroy(GstRecorder *self)
{
    if (!self)
        return;

    if (self->state == RECORDER_STATE_RECORDING)
    {
        recorder_stop(self);
    }

    if (self->bin)
    {
        gst_object_unref(GST_ELEMENT(self->bin));
        self->bin = NULL;
    }

    if (self->filename)
    {
        g_free(self->filename);
        self->filename = NULL;
    }
}

gboolean recorder_link(GstRecorder *self, GstMedia *media)
{
    if (!self || !media || !media->pipeline)
    {
        g_printerr("Invalid arguments to recorder_link\n");
        return FALSE;
    }

    gst_bin_add(GST_BIN(media->pipeline), GST_ELEMENT(self->bin));

    // 获取pad并连接
    GstPad *v_tee_src = gst_element_request_pad_simple(media->v_tee, "src_%u");
    GstPad *a_tee_src = gst_element_request_pad_simple(media->a_tee, "src_%u");
    GstPad *v_queue_sink = gst_element_get_static_pad(GST_ELEMENT(self->bin), "v_sink");
    GstPad *a_queue_sink = gst_element_get_static_pad(GST_ELEMENT(self->bin), "a_sink");

    if (!v_tee_src || !a_tee_src || !v_queue_sink || !a_queue_sink)
    {
        g_printerr("Failed to get pads: v_tee_src=%p, a_tee_src=%p, v_queue_sink=%p, a_queue_sink=%p\n",
                   v_tee_src, a_tee_src, v_queue_sink, a_queue_sink);

        if (v_queue_sink)
            gst_object_unref(v_queue_sink);
        if (a_queue_sink)
            gst_object_unref(a_queue_sink);
        return FALSE;
    }

    g_print("link pad %s to %s for video.\n", gst_pad_get_name(v_tee_src), gst_pad_get_name(v_queue_sink));
    g_print("link pad %s to %s for audio.\n", gst_pad_get_name(a_tee_src), gst_pad_get_name(a_queue_sink));

    gboolean result = TRUE;
    if (gst_pad_link(v_tee_src, v_queue_sink) != GST_PAD_LINK_OK ||
        gst_pad_link(a_tee_src, a_queue_sink) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked.\n");
        result = FALSE;
    }

    gst_object_unref(v_queue_sink);
    gst_object_unref(a_queue_sink);

    return result;
}

gboolean recorder_start(GstRecorder *self, const char *filename)
{
    if (!self || !filename)
    {
        g_printerr("Invalid arguments to recorder_start\n");
        return FALSE;
    }

    if (self->state == RECORDER_STATE_RECORDING)
    {
        g_printerr("Recorder is already recording\n");
        return FALSE;
    }

    g_print("Starting recording to %s...\n", filename);

    if (self->filename)
    {
        g_free(self->filename);
    }
    self->filename = g_strdup(filename);

    g_object_set(self->filesink, "location", filename, NULL);

    GstStateChangeReturn ret = gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Could not set recorder to playing state\n");
        return FALSE;
    }

    self->state = RECORDER_STATE_RECORDING;
    return TRUE;
}

gboolean recorder_stop(GstRecorder *self)
{
    if (!self)
    {
        g_printerr("Recorder instance is NULL\n");
        return FALSE;
    }

    if (self->state != RECORDER_STATE_RECORDING)
    {
        g_print("Recorder is not currently recording\n");
        return TRUE;
    }

    g_print("Stopping recording...\n");

    GstStateChangeReturn ret = gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Could not stop recorder\n");
        return FALSE;
    }

    self->state = RECORDER_STATE_STOPPED;
    return TRUE;
}