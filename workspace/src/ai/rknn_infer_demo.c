/*
 * Phase 2 Step 2: 真实图片推理
 * 命令行: ./ai-infer [model.rknn] [image.webp/jpg/png]
 *
 * 流程: stb_image 解码 → 缩放到 224×224 → INT8 量化 → NPU 推理 → Top-5
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "rknn_api.h"

/* ImageNet 1000 类名 (节选) */
static const char *imagenet_classes[1000] = {
    [281] = "tabby cat",
    [282] = "tiger cat",
    [283] = "Persian cat",
    [284] = "Siamese cat",
    [285] = "Egyptian cat",
    [286] = "cougar",
    [287] = "lynx",
    [340] = "zebra",
    [386] = "African elephant",
    [388] = "giant panda",
    [404] = "airliner",
    [466] = "bulletproof vest",
    [657] = "mortar",
    [812] = "space shuttle",
    [833] = "suit of armor",
};

static const char *class_name(int idx) {
    if (idx >= 0 && idx < 1000 && imagenet_classes[idx])
        return imagenet_classes[idx];
    return "(unknown)";
}

/* 最近邻缩放到 224×224 RGB */
static void resize_rgb(const uint8_t *src, int sw, int sh,
                       uint8_t *dst, int dw, int dh) {
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            int sy = y * sh / dh;
            int si = (sy * sw + sx) * 3;
            int di = (y * dw + x) * 3;
            dst[di+0] = src[si+0];
            dst[di+1] = src[si+1];
            dst[di+2] = src[si+2];
        }
    }
}

/* softmax + top-5 */
static void top5(const int8_t *output, float scale, int zp) {
    float scores[1000];
    float exp_sum = 0;

    /* dequant + softmax */
    float max_val = -1e9;
    for (int i = 0; i < 1000; i++) {
        scores[i] = (output[i] - zp) * scale;
        if (scores[i] > max_val) max_val = scores[i];
    }
    for (int i = 0; i < 1000; i++) {
        scores[i] = expf(scores[i] - max_val);
        exp_sum += scores[i];
    }
    for (int i = 0; i < 1000; i++)
        scores[i] /= exp_sum;

    /* 找 top-5 */
    int top_idx[5] = {0};
    float top_val[5] = {0};
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 5; j++) {
            if (scores[i] > top_val[j]) {
                for (int k = 4; k > j; k--) {
                    top_idx[k] = top_idx[k-1];
                    top_val[k] = top_val[k-1];
                }
                top_idx[j] = i;
                top_val[j] = scores[i];
                break;
            }
        }
    }

    printf("----- Top 5 -----\n");
    for (int i = 0; i < 5; i++)
        printf("  %2d: %-20s %.4f\n", top_idx[i], class_name(top_idx[i]), top_val[i]);
}

int main(int argc, char **argv) {
    const char *model_path = argc > 1 ? argv[1]
        : "/home/cat/models/yolov5s-640-640.rknn";
    const char *img_path   = argc > 2 ? argv[2] : "cat.webp";
    int ret;

    /* ── 1. stb_image 解码 ── */
    printf("--> Load image: %s\n", img_path);
    int w, h, c;
    uint8_t *img = stbi_load(img_path, &w, &h, &c, 3); /* 强制 RGB */
    if (!img) { printf("stbi_load failed: %s\n", stbi_failure_reason()); return -1; }
    printf("    %dx%d, %d channels\n", w, h, c);

    /* ── 2. 缩放到 224×224 ── */
    uint8_t img_224[224 * 224 * 3];
    resize_rgb(img, w, h, img_224, 224, 224);
    stbi_image_free(img);

    /* ── 3. RGB → INT8 (NHWC, 量化参数来自模型) ── */
    int8_t input[224 * 224 * 3];
    const float scale_in = 0.018478f;
    const int   zp_in    = -13;
    for (int i = 0; i < 224*224*3; i++) {
        int val = (int)roundf(img_224[i] / scale_in) + zp_in;
        if (val >  127) val =  127;
        if (val < -128) val = -128;
        input[i] = (int8_t)val;
    }

    /* ── 4. 加载模型 ── */
    printf("--> Load model\n");
    FILE *fp = fopen(model_path, "rb");
    if (!fp) { perror("fopen"); return -1; }
    fseek(fp, 0, SEEK_END);
    long msize = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *mdata = malloc(msize);
    fread(mdata, 1, msize, fp); fclose(fp);

    rknn_context ctx;
    ret = rknn_init(&ctx, mdata, msize, 0, NULL);
    if (ret < 0) { printf("rknn_init: %d\n", ret); return -1; }

    /* ── 5. 查输出量化参数 ── */
    rknn_tensor_attr out_attr = {.index = 0};
    rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &out_attr, sizeof(out_attr));
    float scale_out = out_attr.scale;
    int   zp_out    = out_attr.zp;

    /* ── 6. 设置输入 → 推理 → 取输出 ── */
    rknn_input rk_in = {
        .index  = 0,
        .buf    = input,
        .size   = sizeof(input),
        .pass_through = 0,
        .type   = RKNN_TENSOR_INT8,
        .fmt    = RKNN_TENSOR_NHWC,
    };
    ret = rknn_inputs_set(ctx, 1, &rk_in);
    if (ret < 0) { printf("inputs_set: %d\n", ret); return -1; }

    printf("--> Running inference...\n");
    ret = rknn_run(ctx, NULL);
    if (ret < 0) { printf("rknn_run: %d\n", ret); return -1; }

    rknn_output outputs[1];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].want_float = 0;
    ret = rknn_outputs_get(ctx, 1, outputs, NULL);
    if (ret < 0) { printf("outputs_get: %d\n", ret); return -1; }

    /* ── 7. 解析结果 ── */
    top5((int8_t*)outputs[0].buf, scale_out, zp_out);

    /* ── 8. 释放 ── */
    rknn_outputs_release(ctx, 1, outputs);
    rknn_destroy(ctx);
    free(mdata);
    printf("--> Done.\n");
    return 0;
}
