#ifndef _RKNN_ZERO_COPY_DEMO_POSTPROCESS_H_
#define _RKNN_ZERO_COPY_DEMO_POSTPROCESS_H_

#include <stdint.h>
#include <vector>
#include <map>
#include "spdlog/spdlog.h"
#include "im2d.h"
#include "im2d_single.h"
#include "RgaUtils.h"
#include <opencv2/opencv.hpp>

#define OBJ_NAME_MAX_SIZE 16
#define OBJ_NUMB_MAX_SIZE 64
#define OBJ_CLASS_NUM     2
#define NMS_THRESH        0.45
#define CONF_THRESH       0.5
#define BOX_THRESH        0.4
#define PROP_BOX_SIZE     (5+OBJ_CLASS_NUM)

struct Object
{
    cv::Rect_<float> rect;
    int label;
    float prob;
};
struct AnchorPoints {
    float x;
    float y;
};
namespace Yolo
{
    static constexpr int CHECK_COUNT = 3;
    static constexpr float IGNORE_THRESH = 0.1f;

    static constexpr int MAX_OUTPUT_BBOX_COUNT = 1000;
    static constexpr int CLASS_NUM = 80;
    static constexpr int INPUT_H = 640;  // yolov5's input height and width must be divisible by 32.
    static constexpr int INPUT_W = 640;

    static constexpr int LOCATIONS = 4;
    struct alignas(float) Detection {
        //center_x center_y w h
        float bbox[LOCATIONS];
        float conf;  // bbox_conf * cls_conf
        float class_id;
    };
}
typedef struct _BOX_RECT
{
    int left;
    int right;
    int top;
    int bottom;
} BOX_RECT;

typedef struct __detect_result_t
{
    char name[OBJ_NAME_MAX_SIZE];
    BOX_RECT box;
    float prop;
} detect_result_t;

typedef struct _detect_result_group_t
{
    int id;
    int count;
    detect_result_t results[OBJ_NUMB_MAX_SIZE];
} detect_result_group_t;

int post_process(int8_t *input0, int8_t *input1, int8_t *input2, int model_in_h, int model_in_w,
                 float conf_threshold, float nms_threshold, float scale_w, float scale_h,
                 std::vector<int32_t> &qnt_zps, std::vector<float> &qnt_scales,
                 detect_result_group_t *group);

void deinitPostProcess();

void decode_outputs(float * prob, int output_size, std::vector<Object>& objects, float scale, const int img_w, const int img_h, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor ) ;
std::tuple<float,float,float,float>yolov5BoxConvert(float x1,float y1,float x2,float y2,int originWidth,int originHeight,int targetWidth,int targetHeight);

void generateAnchors(
        const std::vector<int>& fpn_strides,
        std::vector<AnchorPoints> &anchor_points,
        std::vector<float> &stride_tensor,
        int height,
        int width
);
cv::Rect get_rect(cv::Mat& img, float bbox[4]);
float iou(float lbox[4], float rbox[4]);
bool cmp(const Yolo::Detection& a, const Yolo::Detection& b);
void nms(std::vector<Yolo::Detection>& res, float *output, float conf_thresh, float nms_thresh = 0.5);
void draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects);
const float color_list[80][3] =
        {
                {0.000, 0.447, 0.741},
                {0.850, 0.325, 0.098},
                {0.929, 0.694, 0.125},
                {0.494, 0.184, 0.556},
                {0.466, 0.674, 0.188},
                {0.301, 0.745, 0.933},
                {0.635, 0.078, 0.184},
                {0.300, 0.300, 0.300},
                {0.600, 0.600, 0.600},
                {1.000, 0.000, 0.000},
                {1.000, 0.500, 0.000},
                {0.749, 0.749, 0.000},
                {0.000, 1.000, 0.000},
                {0.000, 0.000, 1.000},
                {0.667, 0.000, 1.000},
                {0.333, 0.333, 0.000},
                {0.333, 0.667, 0.000},
                {0.333, 1.000, 0.000},
                {0.667, 0.333, 0.000},
                {0.667, 0.667, 0.000},
                {0.667, 1.000, 0.000},
                {1.000, 0.333, 0.000},
                {1.000, 0.667, 0.000},
                {1.000, 1.000, 0.000},
                {0.000, 0.333, 0.500},
                {0.000, 0.667, 0.500},
                {0.000, 1.000, 0.500},
                {0.333, 0.000, 0.500},
                {0.333, 0.333, 0.500},
                {0.333, 0.667, 0.500},
                {0.333, 1.000, 0.500},
                {0.667, 0.000, 0.500},
                {0.667, 0.333, 0.500},
                {0.667, 0.667, 0.500},
                {0.667, 1.000, 0.500},
                {1.000, 0.000, 0.500},
                {1.000, 0.333, 0.500},
                {1.000, 0.667, 0.500},
                {1.000, 1.000, 0.500},
                {0.000, 0.333, 1.000},
                {0.000, 0.667, 1.000},
                {0.000, 1.000, 1.000},
                {0.333, 0.000, 1.000},
                {0.333, 0.333, 1.000},
                {0.333, 0.667, 1.000},
                {0.333, 1.000, 1.000},
                {0.667, 0.000, 1.000},
                {0.667, 0.333, 1.000},
                {0.667, 0.667, 1.000},
                {0.667, 1.000, 1.000},
                {1.000, 0.000, 1.000},
                {1.000, 0.333, 1.000},
                {1.000, 0.667, 1.000},
                {0.333, 0.000, 0.000},
                {0.500, 0.000, 0.000},
                {0.667, 0.000, 0.000},
                {0.833, 0.000, 0.000},
                {1.000, 0.000, 0.000},
                {0.000, 0.167, 0.000},
                {0.000, 0.333, 0.000},
                {0.000, 0.500, 0.000},
                {0.000, 0.667, 0.000},
                {0.000, 0.833, 0.000},
                {0.000, 1.000, 0.000},
                {0.000, 0.000, 0.167},
                {0.000, 0.000, 0.333},
                {0.000, 0.000, 0.500},
                {0.000, 0.000, 0.667},
                {0.000, 0.000, 0.833},
                {0.000, 0.000, 1.000},
                {0.000, 0.000, 0.000},
                {0.143, 0.143, 0.143},
                {0.286, 0.286, 0.286},
                {0.429, 0.429, 0.429},
                {0.571, 0.571, 0.571},
                {0.714, 0.714, 0.714},
                {0.857, 0.857, 0.857},
                {0.000, 0.447, 0.741},
                {0.314, 0.717, 0.741},
                {0.50, 0.5, 0}
        };
static const char* class_names[] = {
        "person", "phone"
};

#endif //_RKNN_ZERO_COPY_DEMO_POSTPROCESS_H_
