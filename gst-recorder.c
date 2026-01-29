#include "gst-recorder.h"
#include "gst-player.h"

void recorder_init(GstRecorder *self)
{
    // 创建元素
    self->bin = gst_bin_new("recorder_bin");

    self->v_queue = gst_element_factory_make("queue", "v_queue");
    self->v_convert = gst_element_factory_make("videoconvert", "v_convert");
    self->v_encoder = gst_element_factory_make("x264enc", "v_encoder");

    self->a_queue = gst_element_factory_make("queue", "a_queue");
    self->a_convert = gst_element_factory_make("audioconvert", "a_convert");
    self->a_encoder = gst_element_factory_make("avenc_aac", "a_encoder");

    self->mp4mux = gst_element_factory_make("mp4mux", "mp4mux");
    self->filesink = gst_element_factory_make("filesink", "filesink");

    if (
        !self->bin                                                //
        || !self->v_queue || !self->v_convert || !self->v_encoder //
        || !self->a_queue || !self->a_convert || !self->a_encoder //
        || !self->mp4mux || !self->filesink                       //
    )
    {
        g_printerr("Could not create recording elements.\n");
        return;
    }
    // 添加到bin
    gst_bin_add_many(GST_BIN(self->bin),
                     self->v_queue, self->v_convert, self->v_encoder,
                     self->a_queue, self->a_convert, self->a_encoder,
                     self->mp4mux, self->filesink,
                     NULL);

    // 连接元素
    // videoqueue => videoconvert => videoencoder =>  mp4mux.sink_0
    // audioqueue => audioconvert => audioencoder =>  mp4mux.sink_1
    // mp4mux => filesink
    if (
        !gst_element_link_many(self->v_queue, self->v_convert, self->v_encoder, self->mp4mux, NULL)    //
        || !gst_element_link_many(self->a_queue, self->a_convert, self->a_encoder, self->mp4mux, NULL) //
        || !gst_element_link_many(self->mp4mux, self->filesink, NULL)                                  //
    )
    {
        g_printerr("Elements could not be linked.\n");
        return;
    }
    g_object_set(self->v_encoder, "speed-preset", 1, "tune", 0x00000004, "bitrate", 500, NULL);
    g_object_set(self->a_encoder, "bitrate", 128000, NULL);
    g_object_set(self->mp4mux, "faststart", TRUE, NULL);

    // 映射
    // bin.sink_v => v_queue.sink
    // bin.sink_a => a_queue.sink
    GstPad *v_pad = gst_element_get_static_pad(self->v_queue, "sink");
    GstPad *a_pad = gst_element_get_static_pad(self->a_queue, "sink");
    GstPad *v_ghost_pad = gst_ghost_pad_new("sink_v", v_pad);
    GstPad *a_ghost_pad = gst_ghost_pad_new("sink_a", a_pad);
    gst_element_add_pad(self->bin, v_ghost_pad);
    gst_element_add_pad(self->bin, a_ghost_pad);
    gst_pad_set_active(v_ghost_pad, TRUE);
    gst_pad_set_active(a_ghost_pad, TRUE);
    gst_object_unref(v_pad);
    gst_object_unref(a_pad);
}
void recorder_destroy(GstRecorder *self)
{
    gst_object_unref(self->bin);
}
void recorder_link(GstRecorder *self, GstPlayer *player)
{
    gst_bin_add_many(GST_BIN(player->pipeline), self->bin, NULL);
    // 链接
    // player.v_tee_src_1 => recorder.v_queue_sink
    // player.a_tee_src_1 => recorder.a_queue_sink
    GstPad *v_tee_src = gst_element_request_pad_simple(player->v_tee, "src_%u");
    GstPad *a_tee_src = gst_element_request_pad_simple(player->a_tee, "src_%u");
    GstPad *v_queue_sink = gst_element_get_static_pad(self->bin, "sink_v");
    GstPad *a_queue_sink = gst_element_get_static_pad(self->bin, "sink_a");

    if (!v_tee_src || !a_tee_src || !v_queue_sink || !a_queue_sink)
    {
        g_printerr("Failed to get pads: v_tee_src=%p, a_tee_src=%p, v_queue_sink=%p, a_queue_sink=%p\n",
                   v_tee_src, a_tee_src, v_queue_sink, a_queue_sink);
        return;
    }

    g_print("link pad %s to %s video.\n", gst_pad_get_name(v_tee_src), gst_pad_get_name(v_queue_sink));
    g_print("link pad %s to %s audio.\n", gst_pad_get_name(a_tee_src), gst_pad_get_name(a_queue_sink));

    if (gst_pad_link(v_tee_src, v_queue_sink) != GST_PAD_LINK_OK ||
        gst_pad_link(a_tee_src, a_queue_sink) != GST_PAD_LINK_OK)
    {
        g_printerr("Tee could not be linked.\n");
    }
    if (v_queue_sink)
        gst_object_unref(v_queue_sink);
    if (a_queue_sink)
        gst_object_unref(a_queue_sink);
}

void recorder_start(GstRecorder *self, const char *filename)
{
    g_print("Starting recording to %s...\n", filename);
    g_object_set(self->filesink, "location", filename, NULL);
    gst_element_set_state(self->bin, GST_STATE_PLAYING);
}
void recorder_stop(GstRecorder *self)
{
    g_print("Stopping recording...\n");
    gst_element_set_state(self->bin, GST_STATE_NULL);
}