#include "gst-player.h"

void player_on_src_pad_added(GstElement *src, GstPad *new_pad, GstPlayer *self);
gboolean player_on_bus_message(GstBus *bus, GstMessage *msg, GstPlayer *self);
void player_init(GstPlayer *self)
{
    /* 创建元素 */
    self->pipeline = gst_pipeline_new("player-pipeline");
    self->source = gst_element_factory_make("uridecodebin", "source");
    self->v_tee = gst_element_factory_make("tee", "videotee");
    self->v_queue = gst_element_factory_make("queue", "videoqueue");
    self->v_convert = gst_element_factory_make("videoconvert", "videoconvert");
    self->v_sink = gst_element_factory_make("autovideosink", "videosink");
    self->a_tee = gst_element_factory_make("tee", "audiotee");
    self->a_queue = gst_element_factory_make("queue", "audioqueue");
    self->a_convert = gst_element_factory_make("audioconvert", "audioconvert");
    self->a_resample = gst_element_factory_make("audioresample", "resample");
    self->a_sink = gst_element_factory_make("autoaudiosink", "audiosink");
    if (
        !self->pipeline                                                                             // pileline
        || !self->source                                                                            // source
        || !self->v_tee || !self->v_queue || !self->v_convert || !self->v_sink                      // video
        || !self->a_tee || !self->a_queue || !self->a_convert || !self->a_resample || !self->a_sink // audio
    )
    {
        g_printerr("Not all elements could be created.\n");
        return;
    }
    // 构建管道
    gst_bin_add_many(
        GST_BIN(self->pipeline),                                                     // pileline
        self->source,                                                                // source
        self->v_tee, self->v_queue, self->v_convert, self->v_sink,                   // video
        self->a_tee, self->a_queue, self->a_convert, self->a_resample, self->a_sink, // audio
        NULL);
    // 连接元素
    // v_tee.src_0 => videoqueue => videoconvert => videosink
    // a_tee.src_0 => audioqueue => audioconvert => audioresample => audiosink
    if (
        !gst_element_link_many(self->v_tee, self->v_queue, self->v_convert, self->v_sink, NULL)                      // video
        || !gst_element_link_many(self->a_tee, self->a_queue, self->a_convert, self->a_resample, self->a_sink, NULL) // audio
    )
    {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(self->pipeline);
        return;
    }
    // 动态连接元素
    // src.pad_0 => v_tee
    // src.pad_1 => a_tee
    g_signal_connect(self->source, "pad-added", G_CALLBACK(player_on_src_pad_added), self);

    // 监听pipeline的总线
    gst_bus_add_watch(self->bus = gst_element_get_bus(self->pipeline), (GstBusFunc)player_on_bus_message, self);
}

void player_destroy(GstPlayer *self)
{
    /* Free resources */
    gst_object_unref(self->bus);
    gst_element_set_state(self->pipeline, GST_STATE_NULL);
    gst_object_unref(self->pipeline);
}

void player_set_uri(GstPlayer *self, const char *uri)
{
    g_object_set(self->source, "uri", uri, NULL);
}

void player_play(GstPlayer *self)
{
    /* Start playing */
    if (gst_element_set_state(self->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
    }
}

void player_on_src_pad_added(GstElement *src, GstPad *new_pad, GstPlayer *self)
{
    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    GstPad *video_sink_pad = gst_element_get_static_pad(self->v_tee, "sink");
    GstPad *audio_sink_pad = gst_element_get_static_pad(self->a_tee, "sink");

    if (gst_pad_is_linked(video_sink_pad) && gst_pad_is_linked(audio_sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        goto ret;
    }
    new_pad_caps = gst_pad_get_current_caps(new_pad);
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (g_str_has_prefix(new_pad_type, "video/x-raw"))
    {
        ret = gst_pad_link(new_pad, video_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);
    }
    else if (g_str_has_prefix(new_pad_type, "audio/x-raw"))
    {
        ret = gst_pad_link(new_pad, audio_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Link succeeded (type '%s').\n", new_pad_type);
    }
    else
    {
        g_print("It has type '%s' which is not support. Ignoring.\n", new_pad_type);
    }
ret:
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    gst_object_unref(video_sink_pad);
    gst_object_unref(audio_sink_pad);
}

gboolean player_on_bus_message(GstBus *bus, GstMessage *msg, GstPlayer *self)
{
    GError *err;
    gchar *debug_info;
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &debug_info);
        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
        g_clear_error(&err);
        g_free(debug_info);
        break;
    case GST_MESSAGE_EOS:
        g_print("End-Of-Stream reached.\n");
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->pipeline))
        {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            g_print("Pipeline state changed from %s to %s:\n",
                    gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
    default:
        // g_printerr("Unexpected message received.\n");
        break;
    }
    return TRUE; // to keep receiving messages
}