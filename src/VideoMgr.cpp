//
// Created by 迷路的男人 on 2023/8/21.
//
#include <pthread.h>
#include <sched.h>
#include "VideoMgr.h"
std::string SeqProcesser::token = "";

GstFlowReturn VideoMgr::Excute(GstElement *bin) {

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(bin));
    GstElement* source;
    GstFlowReturn ret;
    cv::Mat img_bgr,img_bgr_clone;
    if (sample) {
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        // for debug
        GstClockTime timestamp;
        timestamp = GST_BUFFER_PTS(buffer);
        g_print("Timestamp: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(timestamp));
//        sleep(1);
//        GstCaps *caps = gst_sample_get_caps(sample);
//        if (caps) {
//            gchar *caps_str = gst_caps_to_string(caps);
//            g_print("Sample format: %s\n", caps_str);
//            g_free(caps_str);
//        }
        GstMapInfo info;
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            // 在这里处理 NV12 数据
            // info.data 包含 NV12 图像数据
            // info.size 是数据的大小
            cv::Mat img_NV12(opts.height * 1.5, opts.width, CV_8UC1, info.data);
            cv::cvtColor(img_NV12,img_bgr,cv::COLOR_YUV2BGR_NV12);
            img_bgr_clone = img_bgr.clone();
            {
                std::lock_guard<std::mutex> grd(processer.mutex_);
                processer.frames_.emplace(img_bgr,cnt,need_infer);
                need_infer = !need_infer;
//                spdlog::info("frame queue size :{}",processer.frames_.size());
                processer.cv_.notify_one();
                auto now = std::chrono::steady_clock::now();
                int y = 50;
                for (auto it = processer.cache.begin(); it != processer.cache.end();)
                {
                    if (now - it->first < std::chrono::seconds(1))
                    {
                        // 将结果写到图像上
                        cv::putText(img_bgr_clone,it->second,cv::Point(10,y),cv::FONT_HERSHEY_COMPLEX,0.7,cv::Scalar(60,33,19),2);
                        y += 30;
                        ++it;
                    }
                    else
                    {
                        // 删除过期的结果
                        it = processer.cache.erase(it);
                    }
                }
            }
            gst_buffer_unmap(buffer, &info);
        }
    }

    source = gst_bin_get_by_name(GST_BIN(sink), "video_src");
    if(source){
        GstBuffer *new_buffer;
        GstSample *new_sample;
        GstCaps *new_caps;
        new_buffer = gst_buffer_new_allocate(NULL, img_bgr_clone.total() * img_bgr_clone.elemSize(), NULL);
        gst_buffer_fill(new_buffer, 0, img_bgr_clone.data, img_bgr_clone.total() * img_bgr_clone.elemSize());
        new_caps = gst_caps_new_simple("video/x-raw",
                                       "format", G_TYPE_STRING, "BGR",
                                       "width", G_TYPE_INT, img_bgr_clone.cols,
                                       "height", G_TYPE_INT, img_bgr_clone.rows,
                                       NULL);
        new_sample = gst_sample_new(new_buffer, new_caps, NULL, NULL);
//        spdlog::info("push sample");
        ret = gst_app_src_push_sample(GST_APP_SRC(source), new_sample);
        gst_object_unref(source);
        gst_sample_unref(new_sample);
        gst_buffer_unref(new_buffer);
        gst_caps_unref(new_caps);
    }

    /* we don't need the appsink sample anymore */
    gst_sample_unref(sample);
    return ret;
}

void VideoMgr::Init() {
    video_sink = gst_bin_get_by_name(GST_BIN(src), "video_sink");

    g_signal_connect(video_sink, "new-sample",
                     G_CALLBACK(sourceCallback), this);
    //指定数据类型，宽高不要指定死，能动态设置宽高
    if(opts.deviceType==DEVICE_TYPE::FILE){
        g_object_set(
                G_OBJECT(video_sink),
                "sync", true ,
                "emit-signals", true,
//            "caps", gst_caps_new_simple("video/x-raw",
//                     "width", G_TYPE_INT, opts.width,
//                     "height", G_TYPE_INT, opts.height,
//                     "framerate", GST_TYPE_FRACTION, opts.frameRate, 1,
//                     "format", G_TYPE_STRING, "BGRx", NULL),
                nullptr
        );
        gst_object_unref(video_sink);
    }
    if(opts.deviceType==DEVICE_TYPE::RTSP){
        g_object_set(
                G_OBJECT(video_sink),
//                "sync", false ,
                "emit-signals", true,
//            "caps", gst_caps_new_simple("video/x-raw",
//                     "width", G_TYPE_INT, opts.width,
//                     "height", G_TYPE_INT, opts.height,
//                     "framerate", GST_TYPE_FRACTION, opts.frameRate, 1,
//                     "format", G_TYPE_STRING, "BGRx", NULL),
                nullptr
        );
        gst_object_unref(video_sink);
    }

    /* 插件的 "format" 属性为 GST_FORMAT_TIME。这意味着 "video_src" 插件将会按照时间来处理数据，而不是按照字节或帧数。
     * 这对于同步视频流是很重要的，因为它允许 GStreamer 根据时间戳来正确地调度视频帧的处理和播放。
     * time-based format
     * */
//    g_object_set(video_src, "format", GST_FORMAT_TIME, NULL);



}