//
// Created by 迷路的男人 on 2023/5/31.
//

#ifndef RKNN_YOLOV5_DEMO_YOLOV6EM_H
#define RKNN_YOLOV5_DEMO_YOLOV6EM_H
#include "Yolov6Base.h"
#include "postprocess.h"
class Yolov6EM final : public Yolov6Base{

    void Preprocess(cv::Mat &img) override;
    void Postpocess(float *prob, std::vector<Object> &objects, float scale) override;
    std::vector<Object> infer(cv::Mat &mat) override;
    void decode_outputs(float * prob, int output_size, std::vector<Object>& objects, float scale, const int img_w, const int img_h, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor);
    void draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects);
public:
    static std::unique_ptr<Yolov6EM> createYolov6PPInfer(std::string &rknn_path,int nc,int imgsz){
        auto res = new Yolov6EM(rknn_path.data(),nc,imgsz);
        return std::unique_ptr<Yolov6EM>(res);
    }

    ~Yolov6EM() final;

private:
    Yolov6EM(char *rknn_path, int class_num, int imgsz) : Yolov6Base(rknn_path, class_num, imgsz){}

    int ori_width;
    int ori_height;

    static float EM_BOX_THRESH;
    static float EM_NMS_THRESH;
    std::vector<std::string> class_list{"eye_o","mouth_c","eye_c","mouth_o"};
};


#endif //RKNN_YOLOV5_DEMO_YOLOV6EM_H
