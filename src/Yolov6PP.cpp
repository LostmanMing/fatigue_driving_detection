//
// Created by 迷路的男人 on 2023/5/29.
//

#include "Yolov6PP.h"
float Yolov6PP::PP_BOX_THRESH = 0.4;
float Yolov6PP::PP_NMS_THRESH = 0.6;

inline double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }

void Yolov6PP::Preprocess(cv::Mat &img) {
    int ret;
    // init rga context
    int dst_height = 0;
    int dst_weight = 0;

    ori_width = img.cols;
    ori_height = img.rows;
    spdlog::debug("img width = {}, img height = {}", ori_width, ori_height);

    // You may not need resize when src resulotion equals to resize_dst resulotion

    if (ori_width != width || ori_height != height) {
//        auto ver = querystring(RGA_VERSION);
//        spdlog::info("RGA_VERSION :{} ",ver);
        spdlog::debug("resize and padding with OpenCV!");

        dst_height = ori_height > ori_width ? height : ((float)width / (float)ori_width) * ori_height - int(((float)width / (float)ori_width) * ori_height) % 4;
        dst_weight = ori_width > ori_height ? width  : ((float)height / (float)ori_height) * ori_width - int(((float)height / (float)ori_height) * ori_width) % 4;

        //resize
        cv::Mat resized;
        cv::resize(img,resized,cv::Size(dst_weight,dst_height));


        //padding
        int top = (height - resized.rows) / 2;
        int left = (width - resized.cols) / 2;
        int bottom = height - resized.rows - top;
        int right = width - resized.cols - left;

        cv::Mat padded;
        cv::copyMakeBorder(resized,padded,top,bottom,left,right,cv::BORDER_CONSTANT,cv::Scalar(0,0,0));
        cv::cvtColor(padded, input, cv::COLOR_BGR2RGB);
        // for debug
//        cv::Mat resize_img(cv::Size(width, height), CV_8UC3, padding_buf);
//        cv::imwrite("/home/marvsmart/workspace/sample/resize_input.jpg", resize_img);
        inputs[0].buf = (void *)input.data;
    } else {
        inputs[0].buf = (void *)img.data;
    }

}

void Yolov6PP::Postpocess(float *prob, std::vector<Object> &objects, float scale) {
    decode_outputs((float *)prob, output_size, objects, scale, ori_width, ori_height,anchor_points,stride_tensor);
}
void Yolov6PP::draw_objects(const cv::Mat& bgr, const std::vector<Object>& objects)
{

    cv::Mat image = bgr.clone();

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object& obj = objects[i];
        spdlog::debug("{} = {:03.2f}\t at {:03.2f}\t {:03.2f}\t  {:03.2f}\t x {:03.2f}\t ", obj.label, obj.prob,
                     obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);


        cv::Scalar color = cv::Scalar(color_list[obj.label][0], color_list[obj.label][1], color_list[obj.label][2]);
        float c_mean = cv::mean(color)[0];
        cv::Scalar txt_color;
        if (c_mean > 0.5){
            txt_color = cv::Scalar(0, 0, 0);
        }else{
            txt_color = cv::Scalar(255, 255, 255);
        }

        cv::rectangle(image, obj.rect, color * 255, 2);

        char text[256];
        sprintf(text, "%s %.1f%%", class_list[obj.label].c_str(), obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.4, 1, &baseLine);

        cv::Scalar txt_bk_color = color * 0.7 * 255;

        int x = obj.rect.x;
        int y = obj.rect.y + 1;
        //int y = obj.rect.y - label_size.height - baseLine;
        if (y > image.rows)
            y = image.rows;
        //if (x + label_size.width > image.cols)
        //x = image.cols - label_size.width;

        cv::rectangle(image, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      txt_bk_color, -1);

        cv::putText(image, text, cv::Point(x, y + label_size.height),
                    cv::FONT_HERSHEY_SIMPLEX, 0.4, txt_color, 1);
    }

//    cv::imwrite("/mnt/mmc/zgm/rknn/rknn_yolov5_demo/model/det_res.jpg", image);
    //fprintf(stderr, "save vis file\n");
//    cv::imshow("image", image);
//    cv::waitKey(0);
}
std::vector<Object> Yolov6PP::infer(cv::Mat &mat) {
//    cv::Mat img;
//    cv::cvtColor(mat, img, cv::COLOR_BGR2RGB);
    Preprocess(mat);
//    struct timeval start_time, stop_time;
//    gettimeofday(&start_time, NULL);
    rknn_inputs_set(ctx, rknn_io_num.n_input, inputs);
    rknn_output outputs[rknn_io_num.n_output];
    memset(outputs, 0, sizeof(outputs));
    for (int i = 0; i < rknn_io_num.n_output; i++) {
        outputs[i].want_float = 1;
    }
    rknn_run(ctx, nullptr);
    rknn_outputs_get(ctx, rknn_io_num.n_output, outputs, nullptr);
//    gettimeofday(&stop_time, NULL);
//    spdlog::info("yolopp once run use {} ms", (__get_us(stop_time) - __get_us(start_time)) / 1000);
    std::vector<Object> objects;  //det results
    //sacle
    float size_scale = std::min(width / (ori_width*1.0), height / (ori_height*1.0));
    decode_outputs((float *)outputs[0].buf,output_size,objects,size_scale,ori_width,ori_height,anchor_points,stride_tensor);
//    draw_objects(mat,objects);
    rknn_outputs_release(ctx, rknn_io_num.n_output, outputs);
    return objects;
}

Yolov6PP::~Yolov6PP() {

//    spdlog::info("Yolov6PP free");
}

static inline float intersection_area(const Object& a, const Object& b)
{
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void qsort_descent_inplace(std::vector<Object>& faceobjects, int left, int right)
{
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }
#pragma omp parallel sections
    {
#pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
#pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}
static void qsort_descent_inplace(std::vector<Object>& objects){
    if (objects.empty())
        return;

    qsort_descent_inplace(objects, 0, objects.size() - 1);
}
static void nms_sorted_bboxes(const std::vector<Object>& faceobjects, std::vector<int>& picked, float nms_threshold)
{
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.area();
    }

    for (int i = 0; i < n; i++)
    {
        const Object& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const Object& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            // float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}


void Yolov6PP::decode_outputs(float * prob, int output_size, std::vector<Object>& objects, float scale, const int img_w, const int img_h, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor){
    std::vector<Object> proposals;
    generate_yolo_proposals(prob, output_size, PP_BOX_THRESH, proposals,anchor_points,stride_tensor);
//    spdlog::info("  num of boxes before nms: {}",proposals.size());
    qsort_descent_inplace(proposals);

    std::vector<int> picked;
    nms_sorted_bboxes(proposals, picked, PP_NMS_THRESH);


    int count = picked.size();
    spdlog::debug("----yolov6PP   num of boxes: {}",count);

    objects.resize(count);
    for (int i = 0; i < count; i++)
    {
        objects[i] = proposals[picked[i]];

        // adjust offset to original unpadded
        float x0 = (objects[i].rect.x) / scale;
        float y0 = (objects[i].rect.y) / scale;
        float x1 = (objects[i].rect.x + objects[i].rect.width) / scale;
        float y1 = (objects[i].rect.y + objects[i].rect.height) / scale;

        // clip
        x0 = std::max(std::min(x0, (float)(img_w - 1)), 0.f);
        y0 = std::max(std::min(y0, (float)(img_h - 1)), 0.f);
        x1 = std::max(std::min(x1, (float)(img_w - 1)), 0.f);
        y1 = std::max(std::min(y1, (float)(img_h - 1)), 0.f);

        objects[i].rect.x = x0;
        objects[i].rect.y = y0;
        objects[i].rect.width = x1 - x0;
        objects[i].rect.height = y1 - y0;
    }
}
