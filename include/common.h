//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_COMMON_H
#define FATIGUE_DRIVING_DETECTION_COMMON_H

#include <glib.h>
#include <gst/gstelement.h>
#include "opencv2/opencv.hpp"
#include "postprocess.h"
#define NETWORK_PATH    "/sys/class/net/enp7s0/carrier"  // 有线网络节点

enum class DEVICE_TYPE{
    FILE,RTSP,V4L2
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
    std::string device_id;
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
    bool restart_pipeline = FALSE;
} ProgramData;

struct FrameOpts{
    FrameOpts(cv::Mat &img,uint32_t idx,bool need_infer) : det_res(std::vector<int>(5,0)), img(img), frame_idx(idx), need_infer(need_infer) {}
    FrameOpts(){};
    FrameOpts(const FrameOpts &opts){
        img = opts.img.clone();
        det_res = opts.det_res;
        is_det = opts.is_det;
        is_cal = opts.is_cal;
        frame_idx = opts.frame_idx;
        need_infer = opts.need_infer;
        driver_bbox = opts.driver_bbox;
    }
    FrameOpts& operator=(const FrameOpts & opts){
        if(this == &opts){
            return *this;
        }
        img = opts.img.clone();
        det_res = opts.det_res;
        is_det = opts.is_det;
        is_cal = opts.is_cal;
        frame_idx = opts.frame_idx;
        need_infer = opts.need_infer;
        driver_bbox = opts.driver_bbox;
        return *this;
    }
    cv::Mat img;
    std::vector<int> det_res;
    bool is_det = false;
    bool is_cal = false;
    uint32_t frame_idx;
    bool need_infer = false;
    Object driver_bbox;
};

struct Config_struct
{
    uint8_t config_channel_num;
    uint8_t set_channel_sum;/*通道总数*/
    uint8_t set_channel_num;/*第几个通道*/
    uint8_t set_channel_stutas;/*通道状态*/
    uint8_t set_ai_sum;/*算法总数*/
    uint8_t set_ai_num;/*算法个数*/
    std::vector<std::string> ai_type;/*算法类型*/
    uint8_t rtsp_ul_len;
    std::string rtsp_ul;/*推流地址*/
    uint8_t rtmp_ul_len;
    std::string rtmp_ul;/*拉流地址*/
    uint8_t draw_type;/*绘制报警线类型*/
    uint8_t pointx_num;/*坐标个数*/
    uint16_t pointx[10];/*坐标x*/
    uint16_t pointy[10];/*坐标y*/

    std::string   server_ip;/*平台ip*/
    uint8_t   server_ip_len;/*平台ip长度*/
    uint16_t   server_port;/*平台端口*/

    std::string   ftp_ip;/*ftpip*/
    uint8_t   ftp_ip_len;/*ftp ip长度*/

    std::string   ftp_user;/*ftp user*/
    uint8_t   ftp_user_len;
    std::string   ftp_password;/*ftp 密码*/
    uint8_t   ftp_password_len;

    std::string   id_code;/*服务器识别码*/
    uint8_t   id_code_len;
    std::string code_ver;
    Config_struct()
    {
        set_channel_sum = 0;/*通道总数*/
        set_channel_num = 0;/*第几个通道*/
        set_channel_stutas = 0;/*通道状态*/
        set_ai_num = 0;/*算法个数*/
        draw_type = 0;/*算法类型*/
        pointx_num = 0;/*算法类型*/

        server_ip_len = 0;/*平台ip长度*/
        server_port = 0;/*平台端口*/
        ftp_ip_len = 0;/*ftp ip长度*/
        ftp_password_len = 0;
        id_code_len = 0;
    }

};

#endif //FATIGUE_DRIVING_DETECTION_COMMON_H
