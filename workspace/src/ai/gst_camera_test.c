/*
 * Phase 2 Step 4: GStreamer 摄像头采集验证
 * v4l2src → videoscale(640×640) → videoconvert(BGR) → appsink
 * 抓 10 帧, 计算 FPS, 保存第一帧
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <gst/gst.h>

#define WIDTH  640
#define HEIGHT 640
#define FRAMES 10

static GMainLoop *loop;
static int count = 0;
static uint8_t *first_frame = NULL;

static GstFlowReturn on_sample(GstElement *sink, gpointer data) {
    (void)data;
    GstSample *sample;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer *buf = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buf, &map, GST_MAP_READ);

    if (count == 0) {
        first_frame = malloc(map.size);
        memcpy(first_frame, map.data, map.size);
    }

    gst_buffer_unmap(buf, &map);
    gst_sample_unref(sample);
    count++;

    if (count >= FRAMES)
        g_main_loop_quit(loop);
    return GST_FLOW_OK;
}

int main(void) {
    gst_init(NULL, NULL);
    printf("--> GStreamer camera test: %dx%d BGR, %d frames\n", WIDTH, HEIGHT, FRAMES);

    /* 构建 pipeline */
    char pipe_str[512];
    snprintf(pipe_str, sizeof(pipe_str),
        "v4l2src device=/dev/video-camera0 ! "
        "video/x-raw,format=NV12,width=%d,height=%d ! "
        "videoconvert ! video/x-raw,format=BGR ! "
        "appsink name=sink emit-signals=true sync=false",
        WIDTH, HEIGHT);
    GstElement *pipe = gst_parse_launch(pipe_str, NULL);
    if (!pipe) { fprintf(stderr, "Pipeline failed\n"); return 1; }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipe), "sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_sample), NULL);
    gst_object_unref(sink);

    loop = g_main_loop_new(NULL, FALSE);

    gst_element_set_state(pipe, GST_STATE_PLAYING);
    g_main_loop_run(loop);
    gst_element_set_state(pipe, GST_STATE_NULL);

    /* 保存第一帧 */
    FILE *fp = fopen("/tmp/camera_bgr_640x640.raw", "wb");
    fwrite(first_frame, 1, WIDTH * HEIGHT * 3, fp);
    fclose(fp);
    free(first_frame);

    printf("--> Done. %d frames captured. First frame saved.\n", count);
    gst_object_unref(pipe);
    return 0;
}
