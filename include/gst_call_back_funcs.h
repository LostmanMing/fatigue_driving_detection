//
// Created by 迷路的男人 on 2023/8/21.
//

#ifndef FATIGUE_DRIVING_DETECTION_GST_CALL_BACK_FUNCS_H
#define FATIGUE_DRIVING_DETECTION_GST_CALL_BACK_FUNCS_H
static  GstFlowReturn sourceCallback(GstElement* sink, gpointer data) {
    return ((SourceMgr*)data)->Excute(sink);
}
#endif //FATIGUE_DRIVING_DETECTION_GST_CALL_BACK_FUNCS_H
