#include <gst/gst.h>

typedef struct GstPlayer
{
    GstBus *bus;
    GstElement *pipeline;
    GstElement *source;

    GstElement *v_tee, *v_queue, *v_convert, *v_sink;
    GstElement *a_tee, *a_queue, *a_convert, *a_resample, *a_sink;

} GstPlayer;

void player_init(GstPlayer *self);
void player_destroy(GstPlayer *self);
void player_set_uri(GstPlayer *self, const char *url);
void player_play(GstPlayer *self);