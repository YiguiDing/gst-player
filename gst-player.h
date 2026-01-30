#ifndef __GST_PLAYER_H__
#define __GST_PLAYER_H__

#include <gst/gst.h>
#include "gst-media.h"

typedef enum
{
    PLAYER_STATE_STOPPED,
    PLAYER_STATE_PLAYING,
    PLAYER_STATE_PAUSED
} PlayerState;

typedef struct GstPlayer
{
    GstBus *bus;
    GstBin *bin;

    GstElement *v_queue, *v_convert, *v_sink;
    GstElement *a_queue, *a_convert, *a_resample, *a_sink;

    PlayerState state;
    gchar *current_uri;

} GstPlayer;

gboolean player_init(GstPlayer *self);
void player_destroy(GstPlayer *self);
gboolean player_set_uri(GstPlayer *self, const char *url);
gboolean player_play(GstPlayer *self);
gboolean player_pause(GstPlayer *self);
gboolean player_stop(GstPlayer *self);
void player_seek(GstPlayer *self, gint64 position);
gboolean player_link(GstPlayer *self, GstMedia *media);

#endif