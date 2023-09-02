//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_COMMON_H
#define FATIGUE_DRIVING_DETECTION_COMMON_H

#include <glib.h>
#include <gst/gstelement.h>
#include "opencv2/opencv.hpp"

enum class DEVICE_TYPE{
    FILE,RTSP
};
enum class IMAGE_FORMAT{
    JPEG,BGRA,BGR,GRAY16,GRAY8,RGB,RGBA
};

class PipelineOptions{
public:
    IMAGE_FORMAT imageFormat;
    DEVICE_TYPE deviceType;
    int frameRate;
    int width;
    int height;
    std::string file_path;
    std::string rtsp_uri;
    std::string rtmp_uri;
    bool video_sink_sync;
    bool video_sink_emit_signals;
    std::string sink_format;
    int frame_count;
    std::string token;
    std::string pp_path;
    std::string em_path;
    int pp_size;
    int em_size;
    int pp_nc;
    int em_nc;
};




typedef struct
{
    GMainLoop* loop;
    GstElement* source;
    GstElement* sink;
} ProgramData;

struct FrameOpts{
    FrameOpts(cv::Mat &img,uint32_t idx) : det_res(std::vector<int>(4,0)), img(img), frame_idx(idx) {}
    FrameOpts(){};
    FrameOpts(const FrameOpts &opts){
        img = opts.img.clone();
        det_res = opts.det_res;
        is_det = opts.is_det;
        is_cal = opts.is_cal;
    }
    FrameOpts& operator=(const FrameOpts & opts){
        if(this == &opts){
            return *this;
        }
        img = opts.img.clone();
        det_res = opts.det_res;
        is_det = opts.is_det;
        is_cal = opts.is_cal;
        return *this;
    }
    cv::Mat img;
    std::vector<int> det_res;
    bool is_det = false;
    bool is_cal = false;
    uint32_t frame_idx;
};

#endif //FATIGUE_DRIVING_DETECTION_COMMON_H
