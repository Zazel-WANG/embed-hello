/*
 * Phase 2 Step 1: 验证 RKNN C API 构建链
 * 加载模型 → 查询输入输出 → 释放
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rknn_api.h"

static void dump_tensor(rknn_tensor_attr *attr, const char *tag) {
    printf("  %s: idx=%d name=%s dims=[%d,%d,%d,%d] fmt=%d type=%d n_elems=%d size=%d\n",
           tag,
           attr->index,
           attr->name,
           attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->fmt,
           attr->type,
           attr->n_elems,
           attr->size);
}

int main(int argc, char **argv) {
    const char *model_path = (argc > 1) ? argv[1]
        : "/home/cat/models/yolov5s-640-640.rknn";
    int ret;

    /* 1. 读取模型文件到内存 */
    printf("--> Load model file: %s\n", model_path);
    FILE *fp = fopen(model_path, "rb");
    if (!fp) { perror("fopen"); return -1; }
    fseek(fp, 0, SEEK_END);
    long model_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *model_data = malloc(model_size);
    fread(model_data, 1, model_size, fp);
    fclose(fp);
    printf("    model size = %ld bytes\n", model_size);

    /* 2. 初始化 NPU */
    printf("--> Init RKNN\n");
    rknn_context ctx;
    ret = rknn_init(&ctx, model_data, (uint32_t)model_size, 0, NULL);
    if (ret < 0) { printf("rknn_init failed: %d\n", ret); return -1; }
    printf("    done\n");

    /* 3. 查询 SDK 和驱动版本 */
    rknn_sdk_version sdk_ver;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &sdk_ver, sizeof(sdk_ver));
    if (ret == 0) {
        printf("    SDK: %s, driver: %s\n", sdk_ver.api_version, sdk_ver.drv_version);
    }

    /* 4. 查询输入张量 */
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) { printf("query IO num failed: %d\n", ret); return -1; }
    printf("--> IO: %d input(s), %d output(s)\n", io_num.n_input, io_num.n_output);

    printf("  Input tensors:\n");
    for (int i = 0; i < io_num.n_input; i++) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &attr, sizeof(attr));
        dump_tensor(&attr, "INPUT");
    }
    printf("  Output tensors:\n");
    for (int i = 0; i < io_num.n_output; i++) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr));
        dump_tensor(&attr, "OUTPUT");
    }

    /* 5. 释放 */
    rknn_destroy(ctx);
    free(model_data);
    printf("--> Done. RKNN C API OK.\n");
    return 0;
}
