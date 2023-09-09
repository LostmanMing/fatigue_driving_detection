//
// Created by 迷路的男人 on 2023/8/21.
//
#include <iostream>
#include <sstream>
#include "StreamMgr.h"
#include <gst/pbutils/pbutils.h>

static gboolean
on_source_message(GstBus* bus, GstMessage* message, ProgramData* data)
{
    GstElement* source;

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS:
            spdlog::debug("The source got EOS");
            source = gst_bin_get_by_name(GST_BIN(data->sink), "video_src");
            gst_app_src_end_of_stream(GST_APP_SRC(source));
            gst_object_unref(source);
            break;
        case GST_MESSAGE_ERROR:
            spdlog::error("SRC Received error");
            g_main_loop_quit(data->loop);
            break;
        default:
            break;
    }
    return TRUE;
}
/* called when we get a GstMessage from the sink pipeline when we get EOS, we
 * exit the mainloop and this app. */
static gboolean
on_sink_message(GstBus* bus, GstMessage* message, ProgramData* data)
{
    /* nil */
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS:
            spdlog::warn("Finished playback");
            g_main_loop_quit(data->loop);
            break;
        case GST_MESSAGE_ERROR:
            g_print("SINK Received error");
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(message, &err, &debug_info);
            spdlog::error("Error received from element: {}", GST_OBJECT_NAME(message->src));
            spdlog::error("Error message: {}", err->message);
            spdlog::error("Debug info: {}", debug_info ? debug_info : "none");
            g_error_free(err);
            g_free(debug_info);
            g_main_loop_quit(data->loop);
            break; 
        default:
            break;
    }
    return TRUE;
}
gboolean StreamMgr::Init() {
    gst_init(nullptr, nullptr);
    data = g_new0(ProgramData, 1);
    data->loop = g_main_loop_new(nullptr, FALSE);


//    GstDiscoverer *discoverer = gst_discoverer_new(5 * GST_SECOND, NULL);
//    GError *error = NULL;
    if(opts.deviceType == DEVICE_TYPE::FILE){
        cv::VideoCapture cap(opts.file_path);
        opts.frame_count =  cap.get(cv::CAP_PROP_FRAME_COUNT);
        opts.width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
        opts.height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
        opts.frameRate = cap.get(cv::CAP_PROP_FPS);
        cap.release();
    }
//
//    g_object_unref(discoverer);

    /* init pipeline */
    if(!buildDecodeStr()){
        spdlog::error("failed to init gstreamer decoder pipeline");
        return FALSE;
    }

    if(!buildEecodeStr()){
        spdlog::error("failed to init gstreamer encoder pipeline");
        return FALSE;
    }

    video = std::make_unique<VideoMgr>(data,opts);

    /* to be notified of messages from this pipeline, mostly EOS */
    checkBus(data->source);

    /* set callback and video appsink&appsrc / audio appsink&appsrc caps*/
    setCaps();

    /* to be notified of messages from this pipeline, mostly EOS */
    checkBus(data->sink);
    return TRUE;
}

gboolean StreamMgr::buildDecodeStr() {
    GError * err = nullptr;
    std::ostringstream ss;
    if(!(opts.sink_format == "BGRx")){
        spdlog::error("error ! currently only sport bgrx!");
        return FALSE;
    }
    if(opts.deviceType == DEVICE_TYPE::FILE){
        ss << "filesrc location=" << opts.file_path << " name=src  ! qtdemux  ! h264parse ! mppvideodec ! queue ! videoconvert  ! appsink name=video_sink";
//        ss << "filesrc location=" << opts.file_path << " name=src  ! qtdemux  ! queue ! decodebin ! videoconvert ! appsink name=video_sink";
    } else if(opts.deviceType == DEVICE_TYPE::RTSP){
        ss << "rtspsrc location=" << opts.rtsp_uri << "name=src ! rtph264depay ! h264parse ! mppvideodec ! queue ! videoconvert ! appsink name=video_sink";
    } else if(opts.deviceType == DEVICE_TYPE::V4L2){
        ss << "v4l2src device=" << opts.device_id << "name=src ! queue max-size-buffers=10 max-size-time=500000000 leaky=upstream ! image/jpeg, width=1280, height=720, framerate=25/1 ! mppjpegdec ! appsink name=video_sink";
    }

    //ss << "rtspsrc location=" << opts.rtsp_uri << " name=src ! application/x-rtp,media=audio ! rtpmp4gdepay ! aacparse ! avdec_aac ! audioconvert ! audioresample ! appsink name=audio_sink src. ! application/x-rtp,media=video ! rtph264depay ! h264parse ! nvv4l2decoder cudadec-memtype=2 ! nvvideoconvert ! appsink name=video_sink";
    std::string mLaunchStr = ss.str();
    spdlog::debug("gstreamer decode pipeline string {}",mLaunchStr);
    data->source = gst_parse_launch(mLaunchStr.c_str(), &err);
    if(err!= nullptr){
        spdlog::error("gst failed to create decode pipeline");
        spdlog::error("{}",err->message);
        g_main_loop_unref(data->loop);
        g_free(data);
        g_error_free(err);
        return FALSE;
    }
    return TRUE;
}

gboolean StreamMgr::buildEecodeStr() {
    GError * err = nullptr;
    std::ostringstream ss;
    ss << "appsrc name=video_src ! videoconvert ! video/x-raw,format=NV12 ! mpph264enc ! h264parse  ! flvmux ! rtmpsink name=sink location=\'" << opts.rtmp_uri <<" live=1\'";
    std::string mLaunchStr = ss.str();
    spdlog::debug("gstreamer encode pipeline string {}",mLaunchStr);
    data->sink = gst_parse_launch(mLaunchStr.c_str(), &err);
    if(err!= nullptr){
        spdlog::error("gst failed to create encode pipeline");
        spdlog::error("{}",err->message);
        g_main_loop_unref(data->loop);
        g_free(data);
        g_error_free(err);
        return FALSE;
    }
    return TRUE;
}

void StreamMgr::checkBus(_GstElement *bin) {
    if(bin == data->source){
        bus = gst_element_get_bus(data->source);
        gst_bus_add_watch(bus, (GstBusFunc)on_source_message, data);
    }
    if(bin == data->sink){
        bus = gst_element_get_bus(data->sink);
        gst_bus_add_watch(bus, (GstBusFunc)on_sink_message, data);
    }
    gst_object_unref(bus);
}


void StreamMgr::setCaps() {
    /* we use appsink in push mode, it sends us a signal when data is available
     * and we pull out the data in the signal callback. We want the appsink to
     * push as fast as it can, hence the sync=false */
    /* video part */
    video->Init();
}

gboolean StreamMgr::Start() {
    /* launching things */
    gst_element_set_state(data->sink, GST_STATE_PLAYING);
    gst_element_set_state(data->source, GST_STATE_PLAYING);


    /* let's run !, this loop will quit when the sink pipeline goes EOS or when an
     * error occurs in the source or sink pipelines. */
    spdlog::debug("Let's run!");
    g_main_loop_run(data->loop);

    return TRUE;
}

int StreamMgr::Realse() {
    spdlog::debug("Going out");

    // 释放资源
    gst_element_set_state(data->source, GST_STATE_NULL);
    gst_element_set_state(data->sink, GST_STATE_NULL);

    gst_object_unref(data->source);
    gst_object_unref(data->sink);
    g_main_loop_unref(data->loop);
    g_free(data);
}