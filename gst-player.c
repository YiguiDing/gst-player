#include "gst-player.h"
#include <string.h>

void player_on_src_pad_added(GstElement *src, GstPad *new_pad, GstPlayer *self);
gboolean player_on_bus_message(GstBus *bus, GstMessage *msg, GstPlayer *self);

gboolean player_init(GstPlayer *self)
{
    if (!self)
    {
        g_printerr("Player instance is NULL\n");
        return FALSE;
    }

    memset(self, 0, sizeof(GstPlayer));

    /* 创建元素 */
    self->bin = GST_BIN(gst_bin_new("player_bin"));
    self->v_queue = gst_element_factory_make("queue", "videoqueue");
    self->v_convert = gst_element_factory_make("videoconvert", "videoconvert");
    self->v_sink = gst_element_factory_make("autovideosink", "videosink");
    self->a_queue = gst_element_factory_make("queue", "audioqueue");
    self->a_convert = gst_element_factory_make("audioconvert", "audioconvert");
    self->a_resample = gst_element_factory_make("audioresample", "resample");
    self->a_sink = gst_element_factory_make("autoaudiosink", "audiosink");

    if (
        !self->bin ||                                                            // pipeline
        !self->v_queue || !self->v_convert || !self->v_sink ||                   // video
        !self->a_queue || !self->a_convert || !self->a_resample || !self->a_sink // audio
    )
    {
        g_printerr("Not all elements could be created.\n");
        player_destroy(self);
        return FALSE;
    }

    // 构建管道
    //
    gst_bin_add_many(
        GST_BIN(self->bin),
        self->v_queue, self->v_convert, self->v_sink,
        self->a_queue, self->a_convert, self->a_resample, self->a_sink,
        NULL);

    // 连接元素
    if (
        !gst_element_link_many(self->v_queue, self->v_convert, self->v_sink, NULL) ||                // video
        !gst_element_link_many(self->a_queue, self->a_convert, self->a_resample, self->a_sink, NULL) // audio
    )
    {
        g_printerr("Elements could not be linked.\n");
        player_destroy(self);
        return FALSE;
    }

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

    // 监听pipeline的总线
    self->bus = gst_element_get_bus(GST_ELEMENT(self->bin));
    gst_bus_add_watch(self->bus, (GstBusFunc)player_on_bus_message, self);

    self->state = PLAYER_STATE_STOPPED;
    self->current_uri = NULL;

    return TRUE;
}

void player_destroy(GstPlayer *self)
{
    if (!self)
        return;

    if (GST_ELEMENT(self->bin))
    {
        gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_NULL);
    }

    if (self->bus)
    {
        gst_object_unref(self->bus);
        self->bus = NULL;
    }

    if (self->bin)
    {
        gst_object_unref(GST_ELEMENT(self->bin));
        self->bin = NULL;
    }

    if (self->current_uri)
    {
        g_free(self->current_uri);
        self->current_uri = NULL;
    }
}

gboolean player_link(GstPlayer *self, GstMedia *media)
{
    if (!self || !media || !media->pipeline)
    {
        g_printerr("Invalid arguments to player_link\n");
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

gboolean player_play(GstPlayer *self)
{
    if (!self || !GST_ELEMENT(self->bin))
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    if (gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        return FALSE;
    }

    self->state = PLAYER_STATE_PLAYING;
    return TRUE;
}

gboolean player_pause(GstPlayer *self)
{
    if (!self || !GST_ELEMENT(self->bin))
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    if (gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the paused state.\n");
        return FALSE;
    }

    self->state = PLAYER_STATE_PAUSED;
    return TRUE;
}

gboolean player_stop(GstPlayer *self)
{
    if (!self || !GST_ELEMENT(self->bin))
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    if (gst_element_set_state(GST_ELEMENT(self->bin), GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the stopped state.\n");
        return FALSE;
    }

    self->state = PLAYER_STATE_STOPPED;
    return TRUE;
}

void player_seek(GstPlayer *self, gint64 position)
{
    if (!self || !GST_ELEMENT(self->bin))
    {
        g_printerr("Player not initialized\n");
        return;
    }

    gst_element_seek(GST_ELEMENT(self->bin), 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                     GST_SEEK_TYPE_SET, position,
                     GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
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
        self->state = PLAYER_STATE_STOPPED;
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(GST_ELEMENT(self->bin)))
        {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            g_print("Pipeline state changed from %s to %s:\n",
                    gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
        }
        break;
    default:
        break;
    }
    return TRUE; // to keep receiving messages
}