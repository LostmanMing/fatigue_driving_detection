//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_SOURCEMGR_H
#define FATIGUE_DRIVING_DETECTION_SOURCEMGR_H
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "spdlog/spdlog.h"
#include <cmath>
#include <functional>
#include "common.h"

class SourceMgr {
private:

public:
    virtual ~SourceMgr(){}

    virtual void Init() = 0;

    virtual GstFlowReturn Excute(GstElement* bin) = 0;


protected:
    explicit SourceMgr(ProgramData* data,PipelineOptions &opts): src(data->source),sink(data->sink),opts(opts){}
    //    std::function< GstFlowReturn(GstElement* , ProgramData* )> gst_cb;
    PipelineOptions opts;
    GstElement* src = nullptr;
    GstElement* sink = nullptr;
};


#endif //FATIGUE_DRIVING_DETECTION_SOURCEMGR_H
