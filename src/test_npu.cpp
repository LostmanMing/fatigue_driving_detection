 //
// Created by 迷路的男人 on 2023/8/22.
//
#include "memory"
#include "Yolov6Base.h"
#include "Yolov6PP.h"
#include "Yolov6EM.h"
#include "im2d_type.h"
 int main(){
    spdlog::set_level(spdlog::level::debug);
     cv::Mat mat = cv::imread("../model/driver.jpg");
//     cv::Mat img;
//     cv::cvtColor(mat, img, cv::COLOR_BGR2RGB);
     std::string rknn_path_1 = "../model/RK356X/opt_224_93.rknn";
     std::unique_ptr<Yolov6Base> yolov6pp = Yolov6PP::createYolov6PPInfer(rknn_path_1,3,224);

     yolov6pp->infer(mat);


//     int ret;
//     // init rga context
//     int dst_height = 0;
//     int dst_weight = 0;
//
//     auto ori_width = img.cols;
//     auto ori_height = img.rows;
//     spdlog::info("img width = {}, img height = {}", ori_width, ori_height);
//
//     // You may not need resize when src resulotion equals to resize_dst resulotion
//
//     auto width = 640;
//     auto height = 640;
//     int channel = 3;
//
//     rknn_input inputs[1];
//     memset(inputs, 0, sizeof(inputs));
//     inputs[0].index = 0;
//     inputs[0].type = RKNN_TENSOR_UINT8;
//     inputs[0].size = width * height * channel;
//     inputs[0].fmt = RKNN_TENSOR_NHWC;
//     inputs[0].pass_through = 0;
//     if (ori_width != width || ori_height != height) {
////        auto ver = querystring(RGA_VERSION);
////        spdlog::info("RGA_VERSION :{} ",ver);
//         spdlog::info("resize and padding with RGA!");
//
//         dst_height = ori_height > ori_width ? height : ((float)width / (float)ori_width) * ori_height - int(((float)width / (float)ori_width) * ori_height) % 4;
//         dst_weight = ori_width > ori_height ? width  : ((float)height / (float)ori_height) * ori_width - int(((float)height / (float)ori_height) * ori_width) % 4;
//
//         //resize
//         cv::Mat resized;
//         cv::resize(img,resized,cv::Size(dst_weight,dst_height));
//
//
//        cv::imwrite("/home/marvsmart/workspace/sample/resize_output.jpg", resized);
//         //padding
//         int top = (height - resized.rows) / 2;
//         int left = (width - resized.cols) / 2;
//         int bottom = height - resized.rows - top;
//         int right = width - resized.cols - left;
//
//         cv::Mat padded;
//         cv::copyMakeBorder(resized,padded,top,bottom,left,right,cv::BORDER_CONSTANT,cv::Scalar(0,0,0));
//
//         // for debug
//        cv::imwrite("/home/marvsmart/workspace/sample/padded_output.jpg", padded);
//         inputs[0].buf = (void*)padded.data;
//     } else {
//         inputs[0].buf = (void *) img.data;
//     }

     return 0;
 }