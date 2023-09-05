//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_VIDEOMGR_H
#define FATIGUE_DRIVING_DETECTION_VIDEOMGR_H
#include "SourceMgr.h"
#include "SeqProcesser.hpp"
#include "gst_call_back_funcs.h"
#include <sys/time.h>
class VideoMgr :public SourceMgr{
public:
    explicit VideoMgr(ProgramData* data,PipelineOptions &opts) : SourceMgr(data,opts)
    ,processer(SeqProcesser(std::move(Yolov6PP::createYolov6PPInfer(opts.pp_path,opts.pp_nc,opts.pp_size)),
                           std::move(Yolov6EM::createYolov6PPInfer(opts.em_path,opts.em_nc,opts.em_size)),opts))
                           {

                           }

    void Init() override;

    GstFlowReturn Excute(GstElement *bin) override;

private:
    int cnt = 0;
    bool need_infer = true;
    GstElement* video_sink = nullptr;
    GstElement* video_src = nullptr;
    SeqProcesser processer;
};


#endif //FATIGUE_DRIVING_DETECTION_VIDEOMGR_H
