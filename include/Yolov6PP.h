//
// Created by 迷路的男人 on 2023/5/29.
//

#ifndef RKNN_YOLOV5_DEMO_YOLOV6PP_H
#define RKNN_YOLOV5_DEMO_YOLOV6PP_H
#include "Yolov6Base.h"
#include "postprocess.h"
class Yolov6PP final : public Yolov6Base{

    void Preprocess(cv::Mat &img) override;
    void Postpocess(float *prob, std::vector<Object> &objects, float scale) override;
    std::vector<Object>   infer(cv::Mat &mat) override;
    void decode_outputs(float * prob, int output_size, std::vector<Object>& objects, float scale, const int img_w, const int img_h, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor);
    void draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects);
public:
    static std::unique_ptr<Yolov6PP> createYolov6PPInfer(std::string &rknn_path,int nc,int imgsz){
        auto res = new Yolov6PP(rknn_path.data(),nc,imgsz);
        return std::unique_ptr<Yolov6PP>(res);
    }

    ~Yolov6PP() final;

private:
    Yolov6PP(char *rknn_path, int class_num, int imgsz) : Yolov6Base(rknn_path, class_num, imgsz){}
    int ori_width;
    int ori_height;
    static float PP_BOX_THRESH;
    static float PP_NMS_THRESH;
    std::vector<std::string> class_list{"person","phone"} ;
};


#endif //RKNN_YOLOV5_DEMO_YOLOV6PP_H
