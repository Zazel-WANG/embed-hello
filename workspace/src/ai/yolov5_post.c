/*
 * YOLOv5s 后处理 — 算法重写 (方案 B)
 *
 * 输入: 3 层 RKNN INT8 output → dequant → float
 * 输出: 检测框列表 (最多 100 个)
 *
 * YOLOv5s anchors (640×640 输入):
 *   stride=8:  [[10,13],[16,30],[33,23]]
 *   stride=16: [[30,61],[62,45],[59,119]]
 *   stride=32: [[116,90],[156,198],[373,326]]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── 配置 ── */
#define MAX_CANDIDATES 300
#define MAX_RESULTS    100
#define OBJ_THRESH     0.25f
#define NMS_THRESH     0.45f
#define NUM_CLASSES    80
#define INPUT_W        640
#define INPUT_H        640

/* ── 类型 ── */
typedef struct { float x1, y1, x2, y2; } Box;
typedef struct {
    Box   box;
    float score;
    int   cls;
} Detection;

/* ── YOLOv5 anchors (3 层, 每层 3 个) ── */
static const float anchors[3][3][2] = {
    {{10,13},{16,30},{33,23}},
    {{30,61},{62,45},{59,119}},
    {{116,90},{156,198},{373,326}},
};
static const int strides[3] = {8, 16, 32};

/* ── 工具函数 ── */
static inline float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }
static inline float clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static int score_cmp(const void *a, const void *b) {
    float sa = ((const Detection*)a)->score;
    float sb = ((const Detection*)b)->score;
    return (sa < sb) ? 1 : ((sa > sb) ? -1 : 0);
}

static float iou(const Box *a, const Box *b) {
    float x1 = fmaxf(a->x1, b->x1), y1 = fmaxf(a->y1, b->y1);
    float x2 = fminf(a->x2, b->x2), y2 = fminf(a->y2, b->y2);
    if (x2 <= x1 || y2 <= y1) return 0;
    float inter = (x2 - x1) * (y2 - y1);
    float area_a = (a->x2 - a->x1) * (a->y2 - a->y1);
    float area_b = (b->x2 - b->x1) * (b->y2 - b->y1);
    return inter / (area_a + area_b - inter + 1e-6f);
}

/*
 * dequant_int8: INT8 → float (per-tensor affine)
 *   out[i] = (in[i] - zp) * scale
 */
static void dequant_int8(const int8_t *src, float *dst, int n, int zp, float scale) {
    for (int i = 0; i < n; i++)
        dst[i] = ((float)src[i] - (float)zp) * scale;
}

/*
 * yolov5_decode: 解码一层输出 → 生成候选框
 *   data:   dequant float buffer [3*85 * grid_h * grid_w]
 *   stride: 该层的 stride
 *   dets:   输出 Detection 数组 (append)
 *   count:  当前 dets 数量 (in/out)
 */
static void yolov5_decode(const float *data, int stride, int grid_w, int grid_h,
                          int layer_idx, Detection *dets, int *count) {
    int ai = layer_idx;

    for (int gy = 0; gy < grid_h; gy++) {
        for (int gx = 0; gx < grid_w; gx++) {
            for (int a = 0; a < 3; a++) {
                /* 每个 anchor 占 85 个值: tx,ty,tw,th,conf + 80 classes */
                int base = ((a * 85 + 0) * grid_h + gy) * grid_w + gx;
                int base_conf = ((a * 85 + 4) * grid_h + gy) * grid_w + gx;
                int base_cls  = ((a * 85 + 5) * grid_h + gy) * grid_w + gx;

                /* 读 tx, ty, tw, th (已经 sigmoid/scaled 在 Python 里, 这里需要自己算) */
                float tx = data[base + 0 * grid_h * grid_w];
                float ty = data[base + 1 * grid_h * grid_w];
                float tw = data[base + 2 * grid_h * grid_w];
                float th = data[base + 3 * grid_h * grid_w];
                float conf = sigmoid(data[base_conf]);

                /* box 中心解码 */
                float cx = (sigmoid(tx) * 2.0f - 0.5f + (float)gx) * (float)stride;
                float cy = (sigmoid(ty) * 2.0f - 0.5f + (float)gy) * (float)stride;

                /* box 宽高解码 */
                float bw_anchor = anchors[ai][a][0];
                float bh_anchor = anchors[ai][a][1];
                float bw = powf(sigmoid(tw) * 2.0f, 2) * bw_anchor;
                float bh = powf(sigmoid(th) * 2.0f, 2) * bh_anchor;

                /* 找最大 class 概率 */
                float max_cls = 0;
                int   max_idx = 0;
                for (int c = 0; c < NUM_CLASSES; c++) {
                    float v = data[base_cls + c * grid_h * grid_w];
                    if (v > max_cls) { max_cls = v; max_idx = c; }
                }
                /* class score 用独立 logistic (sigmoid) */
                float cls_score = conf * sigmoid(max_cls);

                if (cls_score > OBJ_THRESH && *count < MAX_CANDIDATES) {
                    dets[*count].box.x1 = cx - bw * 0.5f;
                    dets[*count].box.y1 = cy - bh * 0.5f;
                    dets[*count].box.x2 = cx + bw * 0.5f;
                    dets[*count].box.y2 = cy + bh * 0.5f;
                    dets[*count].score = cls_score;
                    dets[*count].cls   = max_idx;
                    (*count)++;
                }
            }
        }
    }
}

/*
 * yolov5_nms: 按类别 NMS 去重
 *   dets:  候选框 (会被排序 + 标记)
 *   count: 候选数量
 *   result: 去重后结果
 *  返回值: 去重后数量
 */
static int yolov5_nms(Detection *dets, int count, Detection *result) {
    if (count == 0) return 0;

    /* 按 score 降序 */
    qsort(dets, count, sizeof(Detection), score_cmp);

    int kept[MAX_CANDIDATES];
    memset(kept, 1, sizeof(int) * count);

    int out = 0;
    for (int i = 0; i < count && out < MAX_RESULTS; i++) {
        if (!kept[i]) continue;
        result[out++] = dets[i];

        /* 同类 NMS */
        for (int j = i + 1; j < count; j++) {
            if (!kept[j] || dets[j].cls != dets[i].cls) continue;
            if (iou(&dets[i].box, &dets[j].box) > NMS_THRESH)
                kept[j] = 0;
        }
    }
    return out;
}

/*
 * yolov5_post_process: 主入口
 *
 * 输入: output_data[3] — RKNN 输出的 INT8 buffer (3 层, 每层不同大小)
 *       output_attr[3] — 对应的量化参数 (zp, scale)
 * 输出: result[] — 检测结果, 返回值 = 结果数量
 */
int yolov5_post_process(int8_t *output_data[3],
                         rknn_tensor_attr output_attr[3],
                         Detection *result) {
    Detection candidates[MAX_CANDIDATES];
    int count = 0;

    /* 3 层 decode */
    for (int l = 0; l < 3; l++) {
        int grid = output_attr[l].dims[2]; /* 80, 40, 20 */
        int n_elems = output_attr[l].n_elems;
        int zp = output_attr[l].zp;
        float scale = output_attr[l].scale;
        float *fdata = malloc(n_elems * sizeof(float));
        dequant_int8(output_data[l], fdata, n_elems, zp, scale);
        yolov5_decode(fdata, strides[l], grid, grid, l, candidates, &count);
        free(fdata);
    }

    printf("yolov5: %d candidates -> ", count);
    int n = yolov5_nms(candidates, count, result);
    printf("%d detections\n", n);
    return n;
}
