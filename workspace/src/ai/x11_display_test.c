/*
 * Phase 2 Step 3-1: X11 显示通路验证 (v2)
 * 全屏红色 → 等 3 秒 → 全屏绿色 → 等 3 秒 → 退出
 * 确保无论如何都能看到颜色变化
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static void fill(uint8_t *buf, int w, int h, uint8_t b, uint8_t g, uint8_t r) {
    for (int i = 0; i < w * h; i++) {
        buf[i*4+0] = b;
        buf[i*4+1] = g;
        buf[i*4+2] = r;
        buf[i*4+3] = 0;
    }
}

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "Cannot open display\n"); return 1; }

    int screen = DefaultScreen(dpy);
    int sw = DisplayWidth(dpy, screen);
    int sh = DisplayHeight(dpy, screen);
    printf("Screen: %dx%d, depth=%d\n", sw, sh, DefaultDepth(dpy, screen));

    /* 全屏 override_redirect 绕过窗口管理器 */
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    Window win = XCreateWindow(dpy, RootWindow(dpy, screen),
                               0, 0, sw, sh, 0,
                               DefaultDepth(dpy, screen),
                               InputOutput, DefaultVisual(dpy, screen),
                               CWOverrideRedirect, &attr);
    XMapWindow(dpy, win);
    XRaiseWindow(dpy, win);
    XFlush(dpy);

    uint8_t *buf = malloc(sw * sh * 4);
    XImage *ximg = XCreateImage(dpy, DefaultVisual(dpy, screen),
                                 DefaultDepth(dpy, screen), ZPixmap, 0,
                                 (char*)buf, sw, sh, 32, sw * 4);
    GC gc = XCreateGC(dpy, win, 0, NULL);

    /* 红色 3 秒 */
    printf("--> RED\n");
    fill(buf, sw, sh, 0, 0, 255);
    XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, sw, sh);
    XFlush(dpy);
    sleep(3);

    /* 绿色 3 秒 */
    printf("--> GREEN\n");
    fill(buf, sw, sh, 0, 255, 0);
    XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, sw, sh);
    XFlush(dpy);
    sleep(3);

    /* 蓝色 3 秒 */
    printf("--> BLUE\n");
    fill(buf, sw, sh, 255, 0, 0);
    XPutImage(dpy, win, gc, ximg, 0, 0, 0, 0, sw, sh);
    XFlush(dpy);
    sleep(3);

    printf("--> Done.\n");
    ximg->data = NULL;
    XDestroyImage(ximg);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    free(buf);
    return 0;
}
