//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_STREAMMGR_H
#define FATIGUE_DRIVING_DETECTION_STREAMMGR_H
#include "VideoMgr.h"
#include <gst/gst.h>
#include <functional>
#include <sstream>
#include "common.h"

class StreamMgr {
public:
    StreamMgr(PipelineOptions & opt) : opts(opt){}

    gboolean Init();

    gboolean Start();

    int Realse();
private:
    gboolean buildDecodeStr();

    ProgramData* data = nullptr;

    void setCaps();

    void checkBus(_GstElement* bin);

    gboolean buildEecodeStr();

    PipelineOptions opts;
    GstBus* bus = nullptr;
    std::unique_ptr<VideoMgr>  video;

};


#endif //FATIGUE_DRIVING_DETECTION_STREAMMGR_H
