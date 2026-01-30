#include "gst-media.h"
#include <string.h>

void media_on_src_pad_added(GstElement *src, GstPad *new_pad, GstMedia *self);
void media_on_src_pad_removed(GstElement *src, GstPad *new_pad, GstMedia *self);
gboolean media_on_bus_message(GstBus *bus, GstMessage *msg, GstMedia *self);

gboolean media_init(GstMedia *self)
{
    if (!self)
    {
        g_printerr("Player instance is NULL\n");
        return FALSE;
    }

    memset(self, 0, sizeof(GstMedia));

    /* 创建元素 */
    self->pipeline = gst_pipeline_new("media-pipeline");
    self->src = gst_element_factory_make("uridecodebin", "source");
    self->v_tee = gst_element_factory_make("tee", "videotee");
    self->a_tee = gst_element_factory_make("tee", "audiotee");

    if (!self->pipeline || !self->src || !self->v_tee || !self->a_tee)
    {
        g_printerr("Not all elements could be created.\n");
        media_destroy(self);
        return FALSE;
    }

    // 构建管道
    gst_bin_add_many(GST_BIN(self->pipeline), self->src, self->v_tee, self->a_tee, NULL);

    // 动态连接元素
    g_signal_connect(self->src, "pad-added", G_CALLBACK(media_on_src_pad_added), self);
    g_signal_connect(self->src, "pad-removed", G_CALLBACK(media_on_src_pad_removed), self);

    // 监听pipeline的总线
    self->bus = gst_element_get_bus(self->pipeline);
    if (self->bus) {
        gst_bus_add_watch(self->bus, (GstBusFunc)media_on_bus_message, self);
    } else {
        g_printerr("Could not get bus from pipeline\n");
    }

    self->state = MEDIA_STATE_STOPPED;
    self->current_uri = NULL;

    return TRUE;
}

void media_destroy(GstMedia *self)
{
    if (!self)
        return;

    if (self->pipeline)
    {
        gst_element_set_state(self->pipeline, GST_STATE_NULL);
    }

    if (self->bus)
    {
        gst_object_unref(self->bus);
        self->bus = NULL;
    }

    if (self->pipeline)
    {
        gst_object_unref(self->pipeline);
        self->pipeline = NULL;
    }

    if (self->current_uri)
    {
        g_free(self->current_uri);
        self->current_uri = NULL;
    }
}

gboolean media_set_uri(GstMedia *self, const char *uri)
{
    if (!self || !uri)
    {
        g_printerr("Invalid arguments to set URI\n");
        return FALSE;
    }

    if (self->current_uri)
    {
        g_free(self->current_uri);
    }
    self->current_uri = g_strdup(uri);

    g_object_set(self->src, "uri", uri, NULL);
    return TRUE;
}

gboolean media_play(GstMedia *self)
{
    if (!self || !self->pipeline)
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        return FALSE;
    }

    self->state = MEDIA_STATE_PLAYING;
    return TRUE;
}

gboolean media_pause(GstMedia *self)
{
    if (!self || !self->pipeline)
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the paused state.\n");
        return FALSE;
    }

    self->state = MEDIA_STATE_PAUSED;
    return TRUE;
}

gboolean media_stop(GstMedia *self)
{
    if (!self || !self->pipeline)
    {
        g_printerr("Player not initialized\n");
        return FALSE;
    }

    GstStateChangeReturn ret = gst_element_set_state(self->pipeline, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the stopped state.\n");
        return FALSE;
    }

    self->state = MEDIA_STATE_STOPPED;
    return TRUE;
}

void media_seek(GstMedia *self, gint64 position)
{
    if (!self || !self->pipeline)
    {
        g_printerr("Player not initialized\n");
        return;
    }

    gst_element_seek(self->pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
                     GST_SEEK_TYPE_SET, position,
                     GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

void media_on_src_pad_added(GstElement *src, GstPad *new_pad, GstMedia *self)
{
    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;

    GstPad *video_sink_pad = gst_element_get_static_pad(self->v_tee, "sink");
    GstPad *audio_sink_pad = gst_element_get_static_pad(self->a_tee, "sink");

    if (!video_sink_pad || !audio_sink_pad)
    {
        g_printerr("Could not get sink pads\n");
        goto cleanup;
    }

    if (gst_pad_is_linked(video_sink_pad) && gst_pad_is_linked(audio_sink_pad))
    {
        g_print("We are already linked. Ignoring.\n");
        goto cleanup;
    }

    new_pad_caps = gst_pad_get_current_caps(new_pad);
    if (!new_pad_caps)
    {
        g_printerr("Could not get caps from new pad\n");
        goto cleanup;
    }

    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    if (!new_pad_struct)
    {
        g_printerr("Could not get structure from caps\n");
        goto cleanup;
    }

    new_pad_type = gst_structure_get_name(new_pad_struct);
    if (g_str_has_prefix(new_pad_type, "video/"))
    {
        ret = gst_pad_link(new_pad, video_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Video link succeeded (type '%s').\n", new_pad_type);
    }
    else if (g_str_has_prefix(new_pad_type, "audio/"))
    {
        ret = gst_pad_link(new_pad, audio_sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        else
            g_print("Audio link succeeded (type '%s').\n", new_pad_type);
    }
    else
    {
        g_print("It has type '%s' which is not supported. Ignoring.\n", new_pad_type);
    }

cleanup:
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    if (video_sink_pad)
        gst_object_unref(video_sink_pad);
    if (audio_sink_pad)
        gst_object_unref(audio_sink_pad);
}

void media_on_src_pad_removed(GstElement *src, GstPad *new_pad, GstMedia *self)
{

}

// 添加视频分支到tee
gboolean media_add_video_branch(GstMedia *media, GstElement *branch)
{
    if (!media || !branch)
    {
        g_printerr("Invalid arguments to media_add_video_branch\n");
        return FALSE;
    }

    GstPad *tee_src_pad = gst_element_request_pad_simple(media->v_tee, "src_%u");
    if (!tee_src_pad) {
        g_printerr("Failed to request pad from video tee\n");
        return FALSE;
    }

    GstPad *branch_sink_pad = gst_element_get_static_pad(branch, "v_sink");
    if (!branch_sink_pad) {
        g_printerr("Failed to get sink pad from branch\n");
        gst_object_unref(tee_src_pad);
        return FALSE;
    }

    GstPadLinkReturn ret = gst_pad_link(tee_src_pad, branch_sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_printerr("Failed to link video tee pad to branch\n");
        gst_object_unref(tee_src_pad);
        gst_object_unref(branch_sink_pad);
        return FALSE;
    }

    g_print("Successfully added video branch to tee\n");
    gst_object_unref(tee_src_pad);
    gst_object_unref(branch_sink_pad);

    return TRUE;
}

// 添加音频分支到tee
gboolean media_add_audio_branch(GstMedia *media, GstElement *branch)
{
    if (!media || !branch)
    {
        g_printerr("Invalid arguments to media_add_audio_branch\n");
        return FALSE;
    }

    GstPad *tee_src_pad = gst_element_request_pad_simple(media->a_tee, "src_%u");
    if (!tee_src_pad) {
        g_printerr("Failed to request pad from audio tee\n");
        return FALSE;
    }

    GstPad *branch_sink_pad = gst_element_get_static_pad(branch, "a_sink");
    if (!branch_sink_pad) {
        g_printerr("Failed to get sink pad from branch\n");
        gst_object_unref(tee_src_pad);
        return FALSE;
    }

    GstPadLinkReturn ret = gst_pad_link(tee_src_pad, branch_sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        g_printerr("Failed to link audio tee pad to branch\n");
        gst_object_unref(tee_src_pad);
        gst_object_unref(branch_sink_pad);
        return FALSE;
    }

    g_print("Successfully added audio branch to tee\n");
    gst_object_unref(tee_src_pad);
    gst_object_unref(branch_sink_pad);

    return TRUE;
}

// 从tee移除视频分支
gboolean media_remove_video_branch(GstMedia *media, GstElement *branch)
{
    if (!media || !branch) {
        g_printerr("Invalid arguments to media_remove_video_branch\n");
        return FALSE;
    }

    // 这里需要实际实现移除分支的逻辑
    g_print("Video branch removed from tee\n");
    return TRUE;
}

// 从tee移除音频分支
gboolean media_remove_audio_branch(GstMedia *media, GstElement *branch)
{
    if (!media || !branch) {
        g_printerr("Invalid arguments to media_remove_audio_branch\n");
        return FALSE;
    }

    // 这里需要实际实现移除分支的逻辑
    g_print("Audio branch removed from tee\n");
    return TRUE;
}

gboolean media_on_bus_message(GstBus *bus, GstMessage *msg, GstMedia *self)
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
        self->state = MEDIA_STATE_STOPPED;
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
        break;
    }
    return TRUE; // to keep receiving messages
}