//
// Created by 迷路的男人 on 2023/8/21.
//
#include "spdlog/spdlog.h"
#include "fstream"
#include "json/json.h"
#include "common.h"
#include "StreamMgr.h"


static void Parse(PipelineOptions& config,const std::string& config_path){
    Json::Value root;
    Json::Reader g_reader;
    std::ifstream in(config_path,std::ios::binary);
    g_reader.parse(in, root);
    if(root.isMember("decode_config")){
        Json::Value decode_config = root["decode_config"];
        if(decode_config.isMember("stream")){
            Json::Value stream = decode_config["stream"];
            if(root["type"] == "rtsp"){
                config.deviceType = DEVICE_TYPE::RTSP;
                config.rtsp_uri = stream["uri"].asString();
                spdlog::debug("---Decoder: rtsp_uri: {}",config.rtsp_uri);
            } else if(root["type"] == "file"){
                config.deviceType = DEVICE_TYPE::FILE;
                config.file_path = stream["uri"].asString();
                spdlog::debug("---Decoder: file_path: {}",config.file_path);
            }
        }
        if(decode_config.isMember("video_sink")){
            Json::Value video_sink = decode_config["video_sink"];
            config.video_sink_sync = video_sink["sync"].asBool();
            config.video_sink_emit_signals = video_sink["emit-signals"].asBool();
            config.sink_format = video_sink["format"].asString();
            config.height = video_sink["height"].asInt();
            config.width = video_sink["width"].asInt();
        }
    }
    if(root.isMember("encode_config")){
        Json::Value encode_config = root["encode_config"];
        config.rtmp_uri = encode_config["stream"]["uri"].asString();
        spdlog::debug("---Eecoder: rtmp_uri: {}",config.rtmp_uri);
    }
    if(root.isMember("model_config")){
        Json::Value model_config = root["model_config"];
        config.pp_path = model_config["pp_path"].asString();
        config.em_path = model_config["em_path"].asString();
        config.pp_size = model_config["pp_size"].asInt();
        config.em_size = model_config["em_size"].asInt();
        config.pp_nc = model_config["pp_nc"].asInt();
        config.em_nc = model_config["em_nc"].asInt();
    }
    if(root.isMember("token")){
        Json::Value token = root["token"];
        config.token = token.asString();
    }
}

const std::string CONFIG_PATH = "../config/mpp_file_rtmp.json";

int main(){

    spdlog::set_level(spdlog::level::debug);

    PipelineOptions opts;
    Parse(opts,CONFIG_PATH);

    std::unique_ptr<StreamMgr> smgr = std::make_unique<StreamMgr>(opts);

    smgr->Init();

    smgr->Start();

    smgr->Realse();

    return 0;
}