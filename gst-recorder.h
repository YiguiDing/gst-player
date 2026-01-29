
#include <gst/gst.h>
#include "gst-player.h"

typedef struct GstRecorder
{
    GstBus *bus;
    GstElement *bin;
    GstPad *v_pad, *a_pad;

    GstElement *v_queue, *v_convert, *v_encoder;
    GstElement *a_queue, *a_convert, *a_encoder;

    GstElement *mp4mux, *filesink;

} GstRecorder;

void recorder_init(GstRecorder *self);
void recorder_destroy(GstRecorder *self);
void recorder_link(GstRecorder *self, GstPlayer *player);
void recorder_start(GstRecorder *self, const char *filename);
void recorder_stop(GstRecorder *self);