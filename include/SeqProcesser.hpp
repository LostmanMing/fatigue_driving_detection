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
#include "tcp_clinent.h"
#include "ftp.h"
#include "filesystem"
#include "json/json.h"

const std::string red = "\033[31m";
const std::string green = "\033[32m";
const std::string yellow = "\033[33m";
const std::string cyan = "\033[36m";
const std::string blue = "\033[34m";
const std::string reset = "\033[0m";

extern  std::string  videofile_path;
extern struct tm * systime;
class SeqProcesser {
public:
    SeqProcesser(std::unique_ptr<Yolov6Base> inferpp, std::unique_ptr<Yolov6Base> inferem,PipelineOptions &opts)
    {
        // init
        curl_global_init(CURL_GLOBAL_ALL);
        systime_init_funtion();
        tcp_client_init();
        ftp_init();

        thread_tcp_rev = std::thread(tcp_client_rev_date_thread_funtion);
        thread_tcp_send = std::thread(tcp_client_send_date_thread_funtion);/**/
        thread_ftp = std::thread(ftp_thread_funtion);/*发送视频到ftp服务器线程*/

        ppinfer = std::move(inferpp);
        eminfer = std::move(inferem);
        width = opts.width;
        height = opts.height;
        fps = opts.frameRate;
        max_size = opts.frameRate * 3;
        during_fatigue = std::vector<int>(5,0);

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
//            spdlog::info("{}",headerEntry.c_str());
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
                        std::vector<int> needs(5,0);
                        bool flag = false; //是否需要补帧
                        for(int i = 0;i < needs.size();i++){
                            needs[i] = max_size - during_fatigue[i];
                            if(needs[i] <= 0){
                                flag = true;
                                break;
                            }
                        }
                        if(flag){
                            if(get_status_res(4,needs)){
                                spdlog::warn("3s内未检测到驾驶员，请检查摄像头角度或是否有遮挡。");
                                proposal_.clear();
                            }
                            if(get_status_res(1,needs)){
                                spdlog::info("{}Detected{} {} behavior", blue, reset, FatigueClass[1]);
                                SaveAndUpload(1,proposal_);
                                continue;
                            }
                            if(get_status_res(0,needs)){
                                spdlog::info("{}Detected{} {} behavior", blue, reset, FatigueClass[0]);
                                SaveAndUpload(0,proposal_);
                                continue;
                            }
                            if(get_status_res(3,needs)){
                                spdlog::info("{}Detected{} {} behavior", blue, reset, FatigueClass[3]);
                                SaveAndUpload(3,proposal_);
                                continue;
                            }
                            if(get_status_res(2,needs)){
                                spdlog::info("{}Detected{} {} behavior", blue, reset, FatigueClass[2]);
                                SaveAndUpload(2,proposal_);
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
            if(proposal_.size() >= fps * 8){
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
            }
            proposal_.push_back(frame);
            if(frame.det_res[idx] == 0 && frame.need_infer) break;
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
            } else frame.det_res[4] = 1;
        } else frame.det_res[4] = 1;

    }

    // 满足条件时保存视频文件并上传ModelArts
    void SaveAndUpload(int idx,std::list<FrameOpts> &proposal){
        std::thread::id this_id = std::this_thread::get_id();
        get_end(idx);
//        spdlog::info("proposal_.size() :{}",proposal_.size());
        if( (10 <= proposal_.front().frame_idx - pre_idx && pre_cls == idx) || pre_cls!= idx ) { // 需要修改
            std::thread save([&, idx, proposal]() {
                /* save vide file */
                std::vector<cv::Mat> save_proposal;
                std::vector<int> res;
                for (FrameOpts p: proposal) {
                    if (!p.need_infer && !res.empty()) {
                        p.det_res = res;
                    }
                    draw_objects(p.img, p.driver_bbox, p.det_res, FatigueClass[idx]);
                    cv::Mat resizedFrame;
                    cv::resize(p.img.clone(), resizedFrame, cv::Size(640, 480));
                    save_proposal.push_back(resizedFrame);
                    res = p.det_res;
                }
                std::string prefix = "/home/linaro/workspace/data/" + FatigueClass[idx] + "_";
                auto file_name = getCurrentTimeFilename(prefix, ".mp4");
                time_t rawtime;
                time(&rawtime);
                auto time = localtime(&rawtime);
                std::filesystem::path path(file_name);
                string name = path.filename();
                spdlog::info("[Thread {}] {}Detected{} {} behavior, saving file to \"{}\"...",
                             spdlog::details::os::thread_id(), green, reset, FatigueClass[idx], file_name);
                cv::VideoWriter writer(file_name, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), this->fps,
                                       cv::Size(640, 480));

                for (const auto &img: save_proposal) {
//                cv::Mat resizedFrame;
//                cv::resize(img, resizedFrame, cv::Size(640,480));
//                writer.write(resizedFrame);
                    writer.write(img);
                }
                save_proposal.clear();
                spdlog::info("[Thread {}] {}Saved{} file \"{}\"  successfully, uploading to the {}cloud{}...",
                             spdlog::details::os::thread_id(), green, reset, file_name,cyan,reset);
                writer.release();
                /* upload vide file */
                CURL *curl;
                curl = curl_easy_init();
                std::string readBuffer;
                if (curl) {
                    struct curl_httppost *formpost = nullptr;
                    struct curl_httppost *lastptr = nullptr;
                    // 添加表单数据
                    curl_formadd(&formpost,
                                 &lastptr,
                                 CURLFORM_COPYNAME, "input_video",
                                 CURLFORM_FILE, file_name.c_str(),
                                 CURLFORM_END);
                    curl_easy_setopt(curl, CURLOPT_URL,
                                     "https://71696f82bccb4c899786e3e7d3757882.apig.cn-north-4.huaweicloudapis.com/v1/infers/8f0a3dda-d67e-417a-be5e-4eaeaf2c82c8");
                    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
//                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // 开启详细输出
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // 设置写回调函数
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer); // 设置写回调函数的第四个参数
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // 跳过SSL验证
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L); // 跳过SSL验证
                    // 设置HTTP头
                    struct curl_slist *headers = nullptr;
                    headers = curl_slist_append(headers, token.c_str());
                    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

                    auto res = curl_easy_perform(curl);

                    std::unique_lock uniqueLock(socket_m);
                    if (res != CURLE_OK) {
                        spdlog::error("curl_easy_perform() failed: {}", curl_easy_strerror(res));
                        tcp_client.cloud_res = 6;
                    } else {
                        Json::CharReaderBuilder builder;
                        Json::CharReader *reader = builder.newCharReader();
                        Json::Value root;
                        std::string errors;
                        bool parsingSuccessful = reader->parse(readBuffer.c_str(),
                                                               readBuffer.c_str() + readBuffer.size(), &root, &errors);
                        delete reader;
                        if (!parsingSuccessful) {
                            spdlog::error("Error parsing the string: {}", errors);
                        }
                        if (root.isMember("error_msg")) {
                            tcp_client.cloud_res = 5;
                        } else if (root.isMember("result") && root["result"].isMember("drowsy") &&
                                   root["result"]["drowsy"].isArray() && !root["result"]["drowsy"].empty() &&
                                   root["result"]["drowsy"][0U].isMember("category")) {
                            tcp_client.cloud_res = root["result"]["drowsy"][0U]["category"].asInt();
                        } else {
                            tcp_client.cloud_res = 0;
                        }
                        spdlog::info("[Thread {}] {}Upload{} \"{}\"  successfully, response from ModelArts: {}",
                                     spdlog::details::os::thread_id(), green, reset, file_name, readBuffer);
                    }
                    curl_easy_cleanup(curl);
                    curl_formfree(formpost);
                    curl_slist_free_all(headers);
                    systime = time;
                    tcp_client.image_name_len = 0;
                    tcp_client.video_name = name;
                    auto alarm_type = "PL_00" + to_string(idx + 1);
                    tcp_client.ai_type.push_back(alarm_type);
                    tcp_client.video_name_len = name.size();/*计算名称长度*/
                    tcp_client.alert_num = 1;
                    spdlog::info("[Thread {}] {}Verified{} file \"{}\" has been uploaded to the {}server{}.",
                                 spdlog::details::os::thread_id(), green, reset, file_name,yellow,reset);
                    Send_Date(tcp_client.send_buf, SEND_HANDLE_ID);/*发送报警数据*/
//                ftp发送文件
                    bool ret = writeTxtfile(videofile_path, file_name);
                    if (ret == 0) {
                        spdlog::warn("create_video.cpp 写视频文件信息失败");
                        return false;
                    }
                } else {
                    spdlog::error("curl init failed!");
                }
            });

            save.detach();
        }
        pre_cls = idx;
        pre_idx = proposal_.back().frame_idx;
        proposal_.clear();
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

    bool writeTxtfile(string& strPath,string& TxtData)/*Vector作为函数的参数或者返回值时，需要注意它的写法：double Distance(vector<int>&a, vector<int>&b) 其中的“&”绝对不能少*/
    {
        //ofstream txtfile(strPath);   //覆盖写入从已有的文件中写
        ofstream txtfile(strPath, ofstream::app);  //不覆盖写入/*从已有的文件中写*/
        if (!txtfile.is_open())
        {
//        cout<<"写操作不能打开文件!"<<endl;
            return 0;
        }
        txtfile << TxtData;

        txtfile <<"\n";
        txtfile.close();
        return 1;
    }

    ~SeqProcesser(){
        if (workerThread.joinable()) {
            workerThread.join();
        }
        if(thread_tcp_rev.joinable()){
            thread_tcp_rev.join();
        }
        if(thread_tcp_send.joinable()){
            thread_tcp_send.join();
        }
        if(thread_ftp.joinable()){
            thread_ftp.join();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<FrameOpts>  frames_;
protected:
    std::list<FrameOpts>   proposal_;
private:
    std::unordered_map<int,std::string> FatigueClass{
            {0, "LookAround"},     //左顾右盼
            {1, "PhoneCall"},      //打电话
            {2, "Squint"},          //闭眼
            {3, "Yawn"}             //打哈欠
    };

    std::unique_ptr<Yolov6Base> ppinfer;
    std::unique_ptr<Yolov6Base> eminfer;
    std::vector<int> during_fatigue;      //保存每个类别持续时间
    std::mutex socket_m;
    long long pre_idx = -1;
    int pre_cls = -1000;

    int max_size;
    int width;
    int height;
    int fps;
    static std::string token;
    std::thread workerThread;
    std::thread thread_tcp_rev;
    std::thread thread_tcp_send;
    std::thread thread_ftp;
};



#endif //FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
