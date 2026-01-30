#ifndef __GST_MEDIA_H__
#define __GST_MEDIA_H__

#include <gst/gst.h>

typedef enum
{
    MEDIA_STATE_STOPPED,
    MEDIA_STATE_PLAYING,
    MEDIA_STATE_PAUSED
} MediaState;

typedef struct GstMedia
{
    GstBus *bus;
    GstElement *pipeline;
    GstElement *src;

    GstElement *v_tee, *v_queue, *v_convert, *v_sink;
    GstElement *a_tee, *a_queue, *a_convert, *a_resample, *a_sink;

    MediaState state;
    gchar *current_uri;

} GstMedia;

gboolean media_init(GstMedia *self);
void media_destroy(GstMedia *self);
gboolean media_set_uri(GstMedia *self, const char *url);
gboolean media_play(GstMedia *self);
gboolean media_pause(GstMedia *self);
gboolean media_stop(GstMedia *self);
void media_seek(GstMedia *self, gint64 position);

// 添加视频/音频分支的辅助函数
gboolean media_add_video_branch(GstMedia *media, GstElement *branch);
gboolean media_add_audio_branch(GstMedia *media, GstElement *branch);
gboolean media_remove_video_branch(GstMedia *media, GstElement *branch);
gboolean media_remove_audio_branch(GstMedia *media, GstElement *branch);

#endif