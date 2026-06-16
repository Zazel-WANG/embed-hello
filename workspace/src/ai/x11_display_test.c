/*
 * Phase 2 Step 3-1: X11 显示通路验证
 * 创建一个窗口，画彩色矩形 + 文字标记，显示 5 秒后退出
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define WIN_W 1024
#define WIN_H 600

/* 在 RGB buffer 上画一个填充矩形 */
static void fill_rect(uint8_t *buf, int bw, int x, int y, int w, int h,
                      uint8_t r, uint8_t g, uint8_t b) {
    for (int row = y; row < y + h && row < WIN_H; row++) {
        for (int col = x; col < x + w && col < WIN_W; col++) {
            int off = (row * bw + col) * 4;
            buf[off+0] = b;
            buf[off+1] = g;
            buf[off+2] = r;
            buf[off+3] = 0;
        }
    }
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen),
                                     100, 100, WIN_W, WIN_H, 1,
                                     BlackPixel(dpy, screen),
                                     WhitePixel(dpy, screen));

    /* 告诉窗口管理器"这个窗口叫花架子测试" */
    XStoreName(dpy, win, "AI Demo - Display Test");
    XMapWindow(dpy, win);
    XFlush(dpy);

    /* 用 X11 要求的 BGRA 格式创建图像 buffer */
    uint8_t *buf = calloc(1, WIN_W * WIN_H * 4);

    /* 画测试图案: 红绿蓝三条 */
    fill_rect(buf, WIN_W, 0,   0, 341, WIN_H, 255, 0,   0);
    fill_rect(buf, WIN_W, 341, 0, 342, WIN_H, 0,   255, 0);
    fill_rect(buf, WIN_W, 683, 0, 341, WIN_H, 0,   0,   255);

    Visual *vis = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    printf("--> depth=%d, visual=0x%lx\n", depth, (long)vis->visualid);

    XImage *ximg = XCreateImage(dpy, vis, depth, ZPixmap, 0,
                                 (char*)buf, WIN_W, WIN_H, 32, WIN_W * 4);
    GC gc = XCreateGC(dpy, win, 0, NULL);

    printf("--> Showing test pattern for 5 seconds...\n");
    XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, WIN_W, WIN_H);
    XFlush(dpy);
    sleep(5);

    ximg->data = NULL; /* 让 XDestroyImage 不释放我们的 buf */
    XDestroyImage(ximg);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(buf);
    printf("--> Display test OK.\n");
    return 0;
}
