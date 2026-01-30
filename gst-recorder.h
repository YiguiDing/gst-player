#ifndef __GST_RECORDER_H__
#define __GST_RECORDER_H__

#include <gst/gst.h>
#include "gst-media.h"

typedef enum {
    RECORDER_STATE_STOPPED,
    RECORDER_STATE_RECORDING
} RecorderState;

typedef struct GstRecorder
{
    GstBus *bus;
    GstBin *bin;
    GstPad *v_pad, *a_pad;

    GstElement *v_queue, *v_convert, *v_encoder;
    GstElement *a_queue, *a_convert, *a_encoder;

    GstElement *mp4mux, *filesink;
    
    RecorderState state;
    gchar *filename;

} GstRecorder;

gboolean recorder_init(GstRecorder *self);
void recorder_destroy(GstRecorder *self);
gboolean recorder_link(GstRecorder *self, GstMedia *media);
gboolean recorder_start(GstRecorder *self, const char *filename);
gboolean recorder_stop(GstRecorder *self);

#endif