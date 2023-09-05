//
// Created by 迷路的男人 on 2023/8/24.
//

#ifndef FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
#define FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP

#include <RequestParams.h>
#include <signer.h>
#include "common.h"
#include "Yolov6PP.h"
#include "Yolov6EM.h"
#include "numeric"
#include "curl/curl.h"
#include "signal.h"

class SeqProcesser {
public:
    SeqProcesser(std::unique_ptr<Yolov6Base> inferpp, std::unique_ptr<Yolov6Base> inferem,PipelineOptions &opts)
    {
        // init
        curl_global_init(CURL_GLOBAL_ALL);
        ppinfer = std::move(inferpp);
        eminfer = std::move(inferem);
        width = opts.width;
        height = opts.height;
        fps = opts.frameRate;
        max_size = opts.frameRate * 3;
        during_fatigue = std::vector<int>(4,0);

        /* 请求 token 写成方法会 double free，搞不懂 */
        RequestParams* request = new RequestParams("POST", "iam.cn-east-3.myhuaweicloud.com", "/v3/auth/tokens",
                                                   "limit=2", "{\"auth\":{\"identity\":{\"methods\":[\"password\"],\"password\":{\"user\":{\"domain\":{\"name\":\"hw099942894\"},\"name\":\"jiete\",\"password\":\"0$3#ntGvcgw0w\"}}},\"scope\":{\"domain\":{\"id\":\"25432bf1c42a4d2791563991ce747e98\"},\"project\":{\"name\":\"cn-north-4\"}}}}");
        request->addHeader("X-Domain-Id", "25432bf1c42a4d2791563991ce747e98");
        request->addHeader("Content-Type", "application/json;charset=UTF-8");

        Signer signer("7D7IGDX4AEYESWDQJLZ7", "Qcmc65cFnUF0zyqjTfyweNf6FWVoMHfTuH7WLGzZ");
        signer.createSignature(request);

        CURL *curl = curl_easy_init();
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request->getMethod().c_str());
        std::string url = "https://" + request->getHost() + request->getUri() + "?" + request->getQueryParams();
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        struct curl_slist *chunk = NULL;
        std::set<Header>::iterator it;
        for (auto header : *request->getHeaders()) {
            std::string headerEntry = header.getKey() + ": " + header.getValue();
            spdlog::info("{}",headerEntry.c_str());
            chunk = curl_slist_append(chunk, headerEntry.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, request->getPayload().c_str());
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
//        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK){
            spdlog::error("Token request failed!");
        }
        delete request;
        curl_easy_cleanup(curl);


        /*  处理线程 --主要检测逻辑  */
        workerThread = std::thread([&](){
            while(true){
                std::unique_lock<std::mutex> uniqueLock(mutex_);
                cv_.wait(uniqueLock,[this](){
                    return !frames_.empty();
                });
                uniqueLock.unlock();

                if(proposal_.size() < max_size){
                    std::lock_guard<std::mutex> guard(mutex_);
                    proposal_.push_back(frames_.front());
                    frames_.pop();
                } else{
                    if(major_judge()){
                        for(auto& p :proposal_){
                            if(!p.is_cal && p.need_infer) {
                                for(int i = 0;i < p.det_res.size();i++){
                                    if(p.det_res[i] == 1) during_fatigue[i] += 2; //跳帧需要修改
                                    else during_fatigue[i] = 0;
                                }
                            }
                        }
                        std::vector<int> needs(4,0);
                        bool flag = false; //是否需要补帧
                        for(int i = 0;i < needs.size();i++){
                            needs[i] = max_size - during_fatigue[i];
                            if(needs[i] <= 0){
                                flag = true;
                                break;
                            }
                        }
                        if(flag){
                            if(get_status_res(1,needs)){
                                SaveAndUpload(1);
                                continue;
                            }
                            if(get_status_res(0,needs)){
                                SaveAndUpload(0);
                                continue;
                            }
                            if(get_status_res(3,needs)){
                                SaveAndUpload(3);
                                continue;
                            }
                            if(get_status_res(2,needs)){
                                SaveAndUpload(2);
                                continue;
                            }
                            proposal_.clear();
                        } else{
                            auto max_during = *std::max_element(during_fatigue.begin(),during_fatigue.end());
                            int gap = proposal_.size() - max_during;
                            auto it = proposal_.begin();
                            std::advance(it,gap);
                            proposal_.erase(proposal_.begin(), it);
                        }
                    }
                }
//                spdlog::info("proprosal_ size :{}",proposal_.size());
            }
        });
    }
    inline int clamp(int x, int bound){
        if(x < 0) x = 0;
        if(x >= bound) x = bound - 4;
        return x;
    }
    bool cmp(Object &a, Object &b, int height, int width){
        float xra = a.rect.x + a.rect.width;
        float yra = a.rect.y + a.rect.height;
        float xrb = b.rect.x + b.rect.width;
        float yrb = b.rect.y + b.rect.height;
        auto distance_a = int(xra - width)^2 +  int(yra - height)^2;
        auto distance_b = int(xrb - width)^2 +  int(yrb - height)^2;
        return distance_a > distance_b;
    }
    bool major_judge(){
        for (auto it = proposal_.rbegin(); it != proposal_.rend(); ++it) {
            if(!it->is_det){
                if(it->need_infer){
                    infer(*it);
                    auto sum = std::accumulate(it->det_res.begin(),it->det_res.end(),0);
                    if(sum == 0){
                        proposal_.erase(proposal_.begin(), it.base());
                        return false;
                    }
                }
                it->is_det = true;
            } else{
                return true;
            }
        }
        return true;
    }

    // 构造文件名
    std::string getCurrentTimeFilename(const std::string& prefix, const std::string& extension) {
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        // 格式化时间
        std::tm buf;
        localtime_r(&now_time, &buf);
        char timeStr[100];
        strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", &buf);

        // 创建文件名
        std::ostringstream filename;
        filename << prefix << timeStr << extension;

        return filename.str();
    }

    void get_end(int idx){
        auto it = proposal_.begin();
        while(it->det_res[idx] == 0){
            it++;
        }
        for( ;it != proposal_.end();it++){
            if(it->det_res[idx] == 0 && it->need_infer) return ;
        }
        while(true){
            if(proposal_.size() >= fps * 7){
                return;
            }
            std::unique_lock<std::mutex> uniqueLock(mutex_);
            cv_.wait(uniqueLock,[this](){
                return !frames_.empty();
            });

            auto frame = frames_.front();
            frames_.pop();

            uniqueLock.unlock();
            if(frame.need_infer){
                infer(frame);
                if(frame.det_res[idx] != 0){
                    proposal_.push_back(frame);
                } else break;
            }
        }
    }

    bool get_status_res(int idx, std::vector<int>& need){
        if(during_fatigue[idx] >= max_size) return true;
        else{
            auto pre_len = proposal_.size();
            int num = need[idx] - (pre_len - max_size);
            if(during_fatigue[idx] != 0){
                for(int i = 0;i < num;i++){
                    std::unique_lock<std::mutex> uniqueLock(mutex_);
                    cv_.wait(uniqueLock,[this](){
                        return !frames_.empty();
                    });
                    proposal_.push_back(frames_.front());
                    frames_.pop();
                }
                for(auto it = proposal_.rbegin();it != proposal_.rend();it++){
                    if(it->is_det) return true;
                    else{
                        if(it->need_infer){
                            infer(*it);
                            if(it->det_res[idx] == 0) return false;
                        }
                    }
                }
            }
        }
    }

    // 推理逻辑
    void infer(FrameOpts &frame){
        frame.is_det = true;
        auto rect = cv::Rect(static_cast<int>(width/3) , 0, static_cast<int>(width-width/3)- static_cast<int>(width-width/3) % 4, height);
        auto after = frame.img(rect);
        auto res = ppinfer->infer(after);
        std::vector<Object> head;
        std::vector<Object> phone;
        Object driver_obj;
        cv::Mat dirver;
        std::vector<Object> eye;
        std::vector<Object> mouth;
        if(!res.empty()){
            for(auto &r :res){
                if(r.label == 0) head.push_back(r);
                else if (r.label == 1) phone.push_back(r);
            }
            if(!head.empty()){
                if(head.size() == 1) driver_obj = head[0];
                else if(head.size() > 1){
                    int head_idx = 0;
                    for(int i = 1;i < head.size();i++){
                        head_idx = (head[i].prob > head[head_idx].prob) ? i : head_idx;
                    }
                    driver_obj = head[head_idx];
                }
                if(!phone.empty()){
                    for(auto &p : phone){
                        if(p.rect.y - (driver_obj.rect.y + driver_obj.rect.height) < 0) frame.det_res[1] = 1;
                    }
                }
                int x1 = clamp((driver_obj.rect.x - driver_obj.rect.width * 0.05), static_cast<int>(2*width/3));
                int y1 = clamp(driver_obj.rect.y , height);
                int x2 = clamp((driver_obj.rect.x + driver_obj.rect.width + driver_obj.rect.width * 0.05), static_cast<int>(2*width/3));
                int y2 = clamp((driver_obj.rect.y + driver_obj.rect.height + driver_obj.rect.height * 0.05), height);
                if(x1 - x2 == 0 || y1 - y2 == 0){
                    frame.det_res[0] = 0;
                    return ;
                }
                driver_obj.rect.x = x1 + static_cast<int>(width/3); driver_obj.rect.y = y1;
                driver_obj.rect.width = x2 - x1; driver_obj.rect.height = y2 - y1;
                frame.driver_bbox = driver_obj;
                int d_width = (x2 - x1) - (x2 - x1) % 4;
                int d_height = (y2 - y1) - (y2 - y1) % 4;
                auto d_rect = cv::Rect(x1, y1, d_width, d_height);
                dirver = after(d_rect);

                auto em = eminfer->infer(dirver);
                frame.det_res[0] = em.back().label; //左顾右盼
                for(int i = 0; i < em.size()-1;i++){
                    if(em[i].label == 0 || em[i].label == 2) eye.push_back(em[i]);
                    else mouth.push_back(em[i]);
                }
                if(!eye.empty()){
                    for(auto&e : eye){
                        if(e.label == 2) {
                            frame.det_res[2] = 1;
                            break;
                        }
                    }
                } else frame.det_res[2] = 0;
                if(!mouth.empty()) {
                    for(auto& m : mouth){
                        if(m.label == 3){
                            frame.det_res[3] = 1;
                            break;
                        }
                    }
                } else frame.det_res[3] = 0;
            } else frame.det_res[0] = 0;
        } else frame.det_res[0] = 0;

    }

    // 满足条件时保存视频文件并上传ModelArts
    void SaveAndUpload(int idx){
        std::thread::id this_id = std::this_thread::get_id();
        get_end(idx);
//        spdlog::info("proposal_.size() :{}",proposal_.size());
        std::thread save([&,idx](){
            /* save vide file */
            std::unique_lock<std::mutex> uniqueLock(mutex_);
            std::vector<cv::Mat> save_proposal(proposal_.size());
            std::vector<int>res;
            for(auto&& p :proposal_){
                if(!p.need_infer && !res.empty()){
                    p.det_res = res;
                }
                draw_objects(p.img,p.driver_bbox,p.det_res,FatigueClass[idx]);
                save_proposal.emplace_back(p.img.clone());
                res = p.det_res;
            }
            proposal_.clear();
            uniqueLock.unlock();
            std::string prefix = "/home/linaro/workspace/data/" + FatigueClass[idx];
            auto file_name = getCurrentTimeFilename(prefix,".avi");
            spdlog::info("[Thread {}] Detected {} behavior, saving file to \"{}\"...", spdlog::details::os::thread_id(),FatigueClass[idx],file_name);
            cv::VideoWriter writer(file_name,cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),this->fps,cv::Size(width, height));

            for(const auto& img : save_proposal){
                writer.write(img);
            }
            spdlog::info("[Thread {}] File \"{}\" saved successfully, uploading to the cloud...",spdlog::details::os::thread_id(),file_name);
            writer.release();
            /* upload vide file */
            CURL *curl;
            curl = curl_easy_init();
            std::string readBuffer;
            if(curl){
                struct curl_httppost *formpost = nullptr;
                struct curl_httppost *lastptr = nullptr;
                // 添加表单数据
                curl_formadd(&formpost,
                             &lastptr,
                             CURLFORM_COPYNAME, "input_video",
                             CURLFORM_FILE, file_name.c_str(),
                             CURLFORM_END);
                curl_easy_setopt(curl, CURLOPT_URL, "https://71696f82bccb4c899786e3e7d3757882.apig.cn-north-4.huaweicloudapis.com/v1/infers/8f0a3dda-d67e-417a-be5e-4eaeaf2c82c8");
                curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
//                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // 开启详细输出
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // 设置写回调函数
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer); // 设置写回调函数的第四个参数
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 跳过SSL验证
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // 跳过SSL验证
                // 设置HTTP头
                struct curl_slist *headers = nullptr;
                headers = curl_slist_append(headers,token.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                auto res = curl_easy_perform(curl);

                if(res != CURLE_OK){
                    spdlog::error("curl_easy_perform() failed: {}",curl_easy_strerror(res));
                } else{
                    spdlog::info("[Thread {}] \"{}\" upload successful, response from ModelArts: {}",spdlog::details::os::thread_id(),file_name, readBuffer);
                }
                curl_easy_cleanup(curl);
                curl_formfree(formpost);
                curl_slist_free_all(headers);
            }else{
                spdlog::error("curl init failed!");
            }
            save_proposal.clear();
        });
        save.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    //响应报文回调函数
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
        size_t totalSize = size * nmemb;
        out->append((char*)contents, totalSize);
        return totalSize;
    }
    //请求token回调函数
    static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
        std::string header(buffer, size*nitems);

        // 查找'token'字段
        std::string field = "X-Subject-Token:";
        size_t start = header.find(field);
        if(start != std::string::npos) {
            start += field.length();
            size_t end = header.find("\r\n", start);
            std::string value = header.substr(start, end - start);
            token = "X-Auth-Token:"+ value;
            spdlog::info("----Curl Get X-Subject-Token:{}",value);
        }
        return size*nitems;
    }

    void draw_objects(cv::Mat& bgr, const Object& obj, const std::vector<int> &det_res,std::string res)
    {
        cv::Scalar fontColorRed(60, 76, 212); // 红色
        cv::Scalar fontColorGreen(144, 171, 78); // 蓝色
//        spdlog::debug("{} = {:03.2f}\t at {:03.2f}\t {:03.2f}\t  {:03.2f}\t x {:03.2f}\t ", obj.label, obj.prob,
//                      obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

        std::string eyeStatus   = det_res[2] == 1 ? "Eyes Closed" : "Eyes Open";
        std::string mouthStatus = det_res[3] == 1 ? "Mouth Open" : "Mouth Closed";
        std::string headStatus  = det_res[0] == 1 ? "Head Turning" : "Head Forward";
        std::string phoneStatus = det_res[1] == 1 ? "Using Phone" : "Not Using Phone";

        res.pop_back();
        // 定义文字的位置
        int x = 10;
        int y = 50;

        float fontSize = 0.7;
        int fontFace = cv::FONT_HERSHEY_COMPLEX;
        int fontThickness = 2;

        cv::putText(bgr, "STATUS", cv::Point(x, y + 30), fontFace, 1, cv::Scalar(60,33,19), fontThickness);
        cv::putText(bgr, "Eyes: ", cv::Point(x + 40, y + 60),fontFace,fontSize,cv::Scalar(142,204,242),fontThickness);
        cv::putText(bgr, eyeStatus, cv::Point(x + 140, y + 60), fontFace, fontSize, (det_res[2] == 1) ? fontColorRed : fontColorGreen, fontThickness);
        cv::putText(bgr, "Mouth: ", cv::Point(x + 40, y + 90),fontFace,fontSize,cv::Scalar(142,204,242),fontThickness);
        cv::putText(bgr, mouthStatus, cv::Point(x + 140, y + 90), fontFace, fontSize, (det_res[3] == 1) ? fontColorRed : fontColorGreen, fontThickness);
        cv::putText(bgr, "Head: ", cv::Point(x + 40, y + 120),fontFace,fontSize,cv::Scalar(142,204,242),fontThickness);
        cv::putText(bgr, headStatus, cv::Point(x + 140, y + 120), fontFace, fontSize, (det_res[0] == 1) ? fontColorRed : fontColorGreen, fontThickness);
        cv::putText(bgr, "Phone: ", cv::Point(x + 40, y + 150),fontFace,fontSize,cv::Scalar(142,204,242),fontThickness);
        cv::putText(bgr, phoneStatus, cv::Point(x + 140, y + 150), fontFace, fontSize, (det_res[1] == 1) ? fontColorRed : fontColorGreen, fontThickness);



        cv::putText(bgr, "Behavior Detected: " , cv::Point(x, bgr.rows-70), fontFace, 1, cv::Scalar(188,158,33), fontThickness);
        cv::putText(bgr, res, cv::Point(x + 350, bgr.rows - 70), fontFace, fontSize, cv::Scalar(36,49,219), fontThickness);


//    cv::imwrite("/mnt/mmc/zgm/rknn/rknn_yolov5_demo/model/det_res.jpg", image);
        //fprintf(stderr, "save vis file\n");
//    cv::imshow("image", image);
//    cv::waitKey(0);
    }

    ~SeqProcesser(){
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    std::queue<FrameOpts>  frames_;
    std::mutex mutex_;
    std::condition_variable cv_;
protected:
    std::list<FrameOpts>   proposal_;
private:
    std::unordered_map<int,std::string> FatigueClass{
            {0, "Look_around_"},
            {1, "Phone_call_"},
            {2, "Squint_"},
            {3, "Yawn_"}
    };

    std::unique_ptr<Yolov6Base> ppinfer;
    std::unique_ptr<Yolov6Base> eminfer;
    std::vector<int> during_fatigue;      //保存每个类别持续时间

    int max_size;
    int width;
    int height;
    int fps;
    static std::string token;
    std::thread workerThread;
};



#endif //FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
