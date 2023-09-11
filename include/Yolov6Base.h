//
// Created by 迷路的男人 on 2023/5/29.
//

#ifndef RKNN_YOLOV5_DEMO_YOLOV6BASE_H
#define RKNN_YOLOV5_DEMO_YOLOV6BASE_H
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include "rknn_api.h"
#include "rga.h"
#include "postprocess.h"
#include <sys/time.h>

class Yolov6Base {
public:
    Yolov6Base(char *rknn_path, int class_num, int imgsz);
    virtual std::vector<Object> infer(cv::Mat &mat) = 0;
    virtual ~Yolov6Base();

private:
    unsigned char *load_model(const char *filename, int *model_size) ;
    unsigned char *load_data(FILE *fp, size_t ofst, size_t sz);
    void dump_tensor_attr(rknn_tensor_attr *attr);
    void generateAnchors(const std::vector<int> &fpn_strides, std::vector<AnchorPoints> &anchor_points,std::vector<float> &stride_tensor, int height ,int width) ;
    virtual void Preprocess(cv::Mat &img) = 0;
    virtual void Postpocess(float * prob, std::vector<Object>& objects, float scale) = 0;
private:
    char *rknn_path;
    int imgsz;
    unsigned char *model_data = nullptr;
protected:
    rknn_input inputs[1];
    int width;
    int height;
    int channel;
    std::vector<int> fpn_strides{8, 16, 32};  //三种尺度的stride
    std::vector<AnchorPoints> anchor_points;  //手动造anchor
    std::vector<float> stride_tensor;
    int output_size;
    rknn_context ctx;
    cv::Mat input;
    rknn_input_output_num rknn_io_num;
    const int CLASS_NUM;

    void generate_yolo_proposals(float * feat_blob, int output_size, float prob_threshold, std::vector<Object>& objects, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor );
};

#endif //RKNN_YOLOV5_DEMO_YOLOV6BASE_H
