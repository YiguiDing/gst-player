#include "gst-player.h"
#include "gst-recorder.h"

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

    GstPlayer player;
    GstRecorder recorder;
    
    player_init(&player);
    player_set_uri(&player, "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm");
    
    recorder_init(&recorder);
    recorder_link(&recorder, &player);
    recorder_start(&recorder, "output.mp4");

    player_play(&player);

    g_main_loop_run(main_loop);

    player_destroy(&player);
    recorder_destroy(&recorder);
    return 0;
}
