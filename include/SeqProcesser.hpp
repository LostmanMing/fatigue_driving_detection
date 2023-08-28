//
// Created by 迷路的男人 on 2023/8/24.
//

#ifndef FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
#define FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
#include "common.h"
#include "Yolov6PP.h"
#include "Yolov6EM.h"
#include "numeric"

class SeqProcesser {
public:
    SeqProcesser(std::unique_ptr<Yolov6Base> inferpp, std::unique_ptr<Yolov6Base> inferem,PipelineOptions &opts)
    {
        ppinfer = std::move(inferpp);
        eminfer = std::move(inferem);
        width = opts.width;
        height = opts.height;
        max_size = opts.frameRate * 3;
        during_fatigue = std::vector<int>(4,0);
        workerThread = std::thread([&](){
            while(true){
                std::unique_lock<std::mutex> uniqueLock(mutex_);
                cv_.wait(uniqueLock,[this](){
                    return !frames_.empty();
                });
                uniqueLock.unlock();

                if(proposal_.size() <= max_size){
                    std::lock_guard<std::mutex> guard(mutex_);
                    proposal_.push_back(frames_.front());
                    frames_.pop();
                } else{
                    if(major_judge()){
                        for(auto& p :proposal_){
                            if(!p.is_cal) {
                                for(int i = 0;i < p.det_res.size();i++){
                                    if(p.det_res[i] == 1) during_fatigue[i] += 1; //跳帧需要修改
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
                                SaveAndUpload(1, opts.frameRate);
                                continue;
                            }
                            if(get_status_res(0,needs)){
                                SaveAndUpload(0, opts.frameRate);
                                continue;
                            }
                            if(get_status_res(3,needs)){
                                SaveAndUpload(3, opts.frameRate);
                                continue;
                            }
                            if(get_status_res(2,needs)){
                                SaveAndUpload(2, opts.frameRate);
                                continue;
                            }
                        } else{
                            auto max_during = *std::max_element(during_fatigue.begin(),during_fatigue.end());
                            int gap = proposal_.size() - max_during;
                            auto it = proposal_.begin();
                            std::advance(it,gap);
                            proposal_.erase(proposal_.begin(), it);
                        }
                    }
                }
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
        int len = proposal_.size();
        for (auto it = proposal_.rbegin(); it != proposal_.rend(); ++it) {
            if(!it->is_det){
                infer(*it);
                it->is_det = true;
                if(std::accumulate(it->det_res.begin(),it->det_res.end(),0) == 0){
                    proposal_.erase(proposal_.begin(), it.base());
                    return false;
                }
            } else{
                return true;
            }
        }
        return true;
    }

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
            if(it->det_res[idx] == 0) return ;
        }
        while(true){
            std::unique_lock<std::mutex> uniqueLock(mutex_);
            cv_.wait(uniqueLock,[this](){
                return !frames_.empty();
            });

            auto frame = frames_.front();
            frames_.pop();

            uniqueLock.unlock();

            infer(frame);
            spdlog::info("frame.det_res[idx] : {}",frame.det_res[idx]);
            if(frame.det_res[idx] != 0){
                proposal_.push_back(frame);
            } else break;
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
                        infer(*it);
                        if(it->det_res[idx] == 0) return false;
                    }
                }
            }
        }
    }


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
                    int argmin = 0;
                    for (int i = 1; i < head.size(); ++i) {
                        if(cmp(head[argmin],head[i],height,width)) argmin = i;
                    }
                    driver_obj = head[argmin];
                }
                if(!phone.empty())  frame.det_res[1] = 1;
                int x1 = clamp((driver_obj.rect.x - driver_obj.rect.width * 0.05), static_cast<int>(2*width/3));
                int y1 = clamp(driver_obj.rect.y , height);
                int x2 = clamp((driver_obj.rect.x + driver_obj.rect.width + driver_obj.rect.width * 0.05), static_cast<int>(2*width/3));
                int y2 = clamp((driver_obj.rect.y + driver_obj.rect.height + driver_obj.rect.height * 0.05), height);
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
                } else frame.det_res[2] = 1;
                if(!mouth.empty()) {
                    for(auto& m : mouth){
                        if(m.label == 3){
                            frame.det_res[3] = 1;
                            break;
                        }
                    }
                } else frame.det_res[3] = 1;
            } else frame.det_res[0] = 1;
        } else frame.det_res[0] = 1;

    }

    void SaveAndUpload(int idx, int fps){
        get_end(idx);
//        spdlog::info("proposal_.size() :{}",proposal_.size());

        std::thread save([&](){
            /* save vide file */
            std::unique_lock<std::mutex> uniqueLock(mutex_);
            std::vector<cv::Mat> save_proposal(proposal_.size());
            for(auto&& p :proposal_){
                save_proposal.emplace_back(p.img.clone());
            }
            uniqueLock.unlock();
            auto file_name = getCurrentTimeFilename("/home/marvsmart/workspace/data/" + FatigueClass[idx],".avi");
            cv::VideoWriter writer(file_name,cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),fps,cv::Size(width, height));

            for(const auto& img : save_proposal){
                writer.write(img);
            }
            writer.release();
            /* upload vide file */
        });
        save.detach();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::unique_lock<std::mutex> uniqueLock(mutex_);
        proposal_.clear();
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
    std::vector<int> during_fatigue;

    int max_size;
    int width;
    int height;

    std::thread workerThread;
};


#endif //FATIGUE_DRIVING_DETECTION_SEQPROCESSER_HPP
