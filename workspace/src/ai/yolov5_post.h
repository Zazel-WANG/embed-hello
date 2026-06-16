#pragma once
#include "rknn_api.h"

typedef struct { float x1, y1, x2, y2; } Box;
typedef struct {
    Box   box;
    float score;
    int   cls;
} Detection;

int yolov5_post_process(int8_t *output_data[3],
                         rknn_tensor_attr output_attr[3],
                         Detection *result);
