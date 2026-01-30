#ifndef __GST_RTSP_SERVER_H__
#define __GST_RTSP_SERVER_H__

#include <gst/gst.h>
#include "gst-media.h"

// 预声明，避免循环依赖
typedef struct _GstRTSPServer GstRTSPServer;
typedef struct _GstRTSPMountPoints GstRTSPMountPoints;
typedef struct _GstRTSPMediaFactory GstRTSPMediaFactory;

typedef struct GstRtspServer
{
    GstRTSPServer *server;     // 实际的RTSP服务器实例
    GstRTSPMountPoints *mounts;// 挂载点
    GstRTSPMediaFactory *factory; // 媒体工厂
    guint port;
    gchar *uri_path;
    GstElement *bin;           // RTSP流的bin
    gboolean is_streaming;

} GstRtspServer;

gboolean rtsp_server_init(GstRtspServer *self, guint port);
void rtsp_server_destroy(GstRtspServer *self);
gboolean rtsp_link(GstRtspServer *self, GstMedia *media);
gboolean rtsp_unlink(GstRtspServer *self, GstMedia *media);
gboolean rtsp_start(GstRtspServer *self);
gboolean rtsp_stop(GstRtspServer *self);

#endif