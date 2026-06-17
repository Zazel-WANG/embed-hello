/*
 * 花架子 Phase 2 Step 6 — 全链路整合
 *
 * GStreamer(v4l2src→640×640 BGR) → NPU(YOLOv5s) → 后处理(NMS) → 画框 → X11 显示
 * 按 ESC 退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <gst/gst.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <rknn_api.h>
#include "yolov5_post.h"

#define WIN_W  640
#define WIN_H  640
#define MAX_RESULTS 100
#define EMA_ALPHA 0.1f

/* COCO class names (80 classes) */
static const char *coco_names[80] = {
    "person","bicycle","car","motorcycle","airplane","bus","train","truck","boat",
    "traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat",
    "dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack",
    "umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball",
    "kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket",
    "bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple",
    "sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair",
    "couch","potted plant","bed","dining table","toilet","tv","laptop","mouse",
    "remote","keyboard","cell phone","microwave","oven","toaster","sink",
    "refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
};

/* 全局状态 */
static int g_running = 1;
static uint8_t g_frame[WIN_W * WIN_H * 3];  /* BGR */
static int      g_frame_ready = 0;

/* ── GStreamer 回调 ── */
static GstFlowReturn on_new_sample(GstElement *sink, gpointer data) {
    (void)data;
    GstSample *sample;
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (!sample) return GST_FLOW_ERROR;
    GstBuffer *buf = gst_sample_get_buffer(sample);
    GstMapInfo map;
    gst_buffer_map(buf, &map, GST_MAP_READ);
    memcpy(g_frame, map.data, WIN_W * WIN_H * 3);
    gst_buffer_unmap(buf, &map);
    gst_sample_unref(sample);
    g_frame_ready = 1;
    return GST_FLOW_OK;
}

/* ── 画检测框 (BGR buffer, 颜色 BBGGRR) ── */
static void draw_box(uint8_t *buf, int x1, int y1, int x2, int y2,
                     const char *label, uint32_t color) {
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= WIN_W) x2 = WIN_W - 1;
    if (y2 >= WIN_H) y2 = WIN_H - 1;

    uint8_t b = (color >> 16) & 0xff;
    uint8_t g = (color >>  8) & 0xff;
    uint8_t r = (color >>  0) & 0xff;

    /* 画边框 (2px) */
    for (int y = y1; y <= y2; y++) {
        for (int t = 0; t < 2; t++) {
            if (x1 + t < WIN_W) { int o = (y * WIN_W + x1 + t) * 3; buf[o]=b; buf[o+1]=g; buf[o+2]=r; }
            if (x2 - t >= 0)     { int o = (y * WIN_W + x2 - t) * 3; buf[o]=b; buf[o+1]=g; buf[o+2]=r; }
        }
    }
    for (int x = x1; x <= x2; x++) {
        for (int t = 0; t < 2; t++) {
            if (y1 + t < WIN_H) { int o = ((y1 + t) * WIN_W + x) * 3; buf[o]=b; buf[o+1]=g; buf[o+2]=r; }
            if (y2 - t >= 0)    { int o = ((y2 - t) * WIN_W + x) * 3; buf[o]=b; buf[o+1]=g; buf[o+2]=r; }
        }
    }
    /* 标签背景 */
    int lw = strlen(label) * 8 + 8;
    int ly = y1 - 22;
    if (ly < 0) ly = y1 + 2;
    for (int y = ly; y < ly + 20 && y < WIN_H; y++)
        for (int x = x1; x < x1 + lw && x < WIN_W; x++) {
            int o = (y * WIN_W + x) * 3; buf[o]=b; buf[o+1]=g; buf[o+2]=r;
        }
    /* 标签文字 (简易: 画白色像素块代替文字) */
}

/* ── HUD 叠加：推理耗时 + FPS (X11 字体) ── */
static void draw_hud(Display *dpy, Window win, GC gc,
                     float infer_ms, float fps) {
    char text[64];
    snprintf(text, sizeof(text), "Infer: %.1fms | FPS: %.1f", infer_ms, fps);

    Font font = XLoadFont(dpy, "fixed");
    if (!font) return;
    XSetFont(dpy, gc, font);

    XSetForeground(dpy, gc, 0xFFFFFF);
    XDrawString(dpy, win, gc, WIN_W - 190, 18, text, strlen(text));

    XUnloadFont(dpy, font);
}

/* ── X11 事件处理 ── */
static void handle_x11(Display *dpy) {
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Escape) g_running = 0;
        }
    }
}

int main(void) {
    /* ══ 1. GStreamer 初始化 ══ */
    gst_init(NULL, NULL);
    GstElement *pipe = gst_parse_launch(
        "v4l2src device=/dev/video-camera0 ! "
        "video/x-raw,format=NV12,width=3840,height=2160 ! "
        "videoscale ! video/x-raw,width=640,height=640 ! "
        "videoconvert ! video/x-raw,format=BGR ! "
        "appsink name=sink emit-signals=true sync=false", NULL);
    if (!pipe) { fprintf(stderr, "GStreamer pipeline failed\n"); return 1; }
    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipe), "sink");
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), NULL);
    gst_object_unref(sink);
    gst_element_set_state(pipe, GST_STATE_PLAYING);

    /* 等第一帧 */
    printf("Waiting for first frame...\n");
    while (!g_frame_ready) { g_usleep(10000); }
    printf("Camera ready.\n");

    /* ══ 2. 加载 YOLOv5s 模型 ══ */
    const char *model_path = "/home/cat/ai-demo/yolov5s-640-640.rknn";
    FILE *fp = fopen(model_path, "rb");
    if (!fp) { perror(model_path); return 1; }
    fseek(fp, 0, SEEK_END); long msize = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *mdata = malloc(msize); fread(mdata, 1, msize, fp); fclose(fp);
    rknn_context ctx;
    rknn_init(&ctx, mdata, msize, 0, NULL);

    /* 查询输出属性 */
    rknn_input_output_num io;
    rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io, sizeof(io));
    rknn_tensor_attr out_attr[3];
    for (int i = 0; i < 3; i++) {
        memset(&out_attr[i], 0, sizeof(out_attr[i]));
        out_attr[i].index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &out_attr[i], sizeof(out_attr[i]));
    }
    printf("Model loaded: %d outputs\n", io.n_output);

    /* ══ 3. X11 窗口 ══ */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }
    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     50, 0, WIN_W, WIN_H, 1,
                                     BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XStoreName(dpy, win, "AI Demo - YOLOv5 Real-time Detection");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    uint8_t *disp_buf = calloc(1, WIN_W * WIN_H * 4);
    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, screen), 24,
                                 ZPixmap, 0, (char*)disp_buf, WIN_W, WIN_H, 32, WIN_W * 4);

    /* ══ 4. 主循环 ══ */
    int frame_count = 0;
    struct timespec ts;
    double t_last_ns;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    t_last_ns = ts.tv_sec * 1e9 + ts.tv_nsec;
    float fps_ema = 0.0f, infer_ms = 0.0f;
    printf("Running... Press ESC to exit.\n");

    while (g_running) {
        /* 等新帧 */
        g_frame_ready = 0;
        while (!g_frame_ready && g_running) {
            handle_x11(dpy);
            g_usleep(5000);
        }
        if (!g_running) break;

        /* 帧开始时间戳 */
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t_frame_start = ts.tv_sec * 1e9 + ts.tv_nsec;

        /* BGR(RGB) → INT8 量化 (zp=-128) */
        int8_t *input = malloc(WIN_W * WIN_H * 3);
        for (int i = 0; i < WIN_W * WIN_H * 3; i++)
            input[i] = (int8_t)((int)g_frame[i] - 128);

        /* NPU 推理计时 */
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t_before_infer = ts.tv_sec * 1e9 + ts.tv_nsec;

        rknn_input rk_in = {0, input, (uint32_t)(WIN_W * WIN_H * 3),
                             0, RKNN_TENSOR_INT8, RKNN_TENSOR_NHWC};
        rknn_inputs_set(ctx, 1, &rk_in);
        rknn_run(ctx, NULL);

        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t_after_infer = ts.tv_sec * 1e9 + ts.tv_nsec;
        infer_ms = (float)((t_after_infer - t_before_infer) / 1e6);

        rknn_output outputs[3];
        memset(outputs, 0, sizeof(outputs));
        for (int i = 0; i < 3; i++) { outputs[i].want_float = 0; outputs[i].index = i; }
        rknn_outputs_get(ctx, 3, outputs, NULL);
        int8_t *out_data[3];
        for (int i = 0; i < 3; i++) out_data[i] = (int8_t*)outputs[i].buf;

        /* 后处理 */
        Detection dets[MAX_RESULTS];
        int nd = yolov5_post_process(out_data, out_attr, dets);

        /* 画框 */
        for (int i = 0; i < nd; i++) {
            uint32_t color = 0xFF0000; /* 蓝色框 */
            if (dets[i].cls == 0) color = 0x00FF00; /* person → 绿色 */
            char label[64];
            snprintf(label, sizeof(label), "%s %.2f",
                     (dets[i].cls < 80) ? coco_names[dets[i].cls] : "?", dets[i].score);
            draw_box(g_frame,
                     (int)dets[i].box.x1, (int)dets[i].box.y1,
                     (int)dets[i].box.x2, (int)dets[i].box.y2,
                     label, color);
        }

        /* FPS (EMA 平滑) */
        double dt_ns = t_frame_start - t_last_ns;
        t_last_ns = t_frame_start;
        float instant_fps = (dt_ns > 0.0) ? (float)(1e9 / dt_ns) : 0.0f;
        if (fps_ema < 0.1f) fps_ema = instant_fps;
        else fps_ema = fps_ema * (1.0f - EMA_ALPHA) + instant_fps * EMA_ALPHA;
        frame_count++;

        /* BGR(3-byte) → BGRA(4-byte) for X11 */
        for (int i = 0; i < WIN_W * WIN_H; i++) {
            disp_buf[i*4+0] = g_frame[i*3+0];   /* B */
            disp_buf[i*4+1] = g_frame[i*3+1];   /* G */
            disp_buf[i*4+2] = g_frame[i*3+2];   /* R */
            disp_buf[i*4+3] = 0;
        }

        /* 显示 + HUD 叠加 */
        XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, WIN_W, WIN_H);
        draw_hud(dpy, win, gc, infer_ms, fps_ema);
        handle_x11(dpy);

        rknn_outputs_release(ctx, 3, outputs);
        free(input);
    }

    /* ══ 5. 清理 ══ */
    printf("Frames: %d. Last infer: %.1fms, FPS: %.1f. Shutting down...\n",
           frame_count, infer_ms, fps_ema);
    XDestroyImage(ximg); /* ximg owns disp_buf */
    XFreeGC(dpy, gc); XDestroyWindow(dpy, win); XCloseDisplay(dpy);
    rknn_destroy(ctx); free(mdata);
    gst_element_set_state(pipe, GST_STATE_NULL); gst_object_unref(pipe);
    return 0;
}
