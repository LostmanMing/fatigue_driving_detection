//
// Created by 迷路的男人 on 2023/5/29.
//

#include "Yolov6Base.h"
Yolov6Base::Yolov6Base(char *rknn_path, int class_num, int imgsz):rknn_path(rknn_path), CLASS_NUM(class_num), imgsz(imgsz){
    const float NMS_THRES = 0.45;
    const float BOX_THRES = 0.4;
    int ret;
    /* Create the neural network */
    spdlog::info("Loading mode...");
    int model_data_size = 0;
    model_data = load_model(rknn_path, &model_data_size);
    ret = rknn_init(&ctx, model_data, model_data_size, 0, NULL);
    if (ret < 0) {
        spdlog::error("rknn_init error ret={}",ret);
        return ;
    }
    /* querry sdk version*/
    rknn_sdk_version version;
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0) {
        spdlog::error("rknn_init error ret={}",ret);
        return ;
    }
    spdlog::info("sdk version: {} driver version: {}", version.api_version, version.drv_version);
    /*query rknn_input_output_num */
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        spdlog::error("rknn_init error ret={}", ret);
        return ;
    }
    spdlog::info("model input num: {}, output num: {}\n", io_num.n_input, io_num.n_output);
    this->rknn_io_num = io_num;
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            spdlog::error("rknn_init error ret={}", ret);
            return ;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        dump_tensor_attr(&(output_attrs[i]));
    }
    output_size = output_attrs[0].dims[1] * output_attrs[0].dims[2];
    //获取输入参数
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        spdlog::info("model is NCHW input fmt");
        channel = input_attrs[0].dims[1];
        height = input_attrs[0].dims[2];
        width = input_attrs[0].dims[3];
    } else {
        spdlog::info("model is NHWC input fmt");
        height = input_attrs[0].dims[1];
        width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }
    spdlog::info("model input height={}, width={}, channel={}", height, width, channel);
    //设置输入格式
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].pass_through = 0;

    //生成网格
    generateAnchors(fpn_strides,anchor_points,stride_tensor,imgsz,imgsz);
}

unsigned char *Yolov6Base::load_model(const char *filename, int *model_size) {
    FILE *fp;
    unsigned char *data;

    fp = fopen(filename, "rb");
    if (NULL == fp) {
        printf("Open file %s failed.\n", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    data = load_data(fp, 0, size);

    fclose(fp);

    *model_size = size;
    return data;
}

unsigned char * Yolov6Base::load_data(FILE *fp, size_t ofst, size_t sz) {
    unsigned char *data;
    int ret;

    data = NULL;

    if (NULL == fp) {
        return NULL;
    }

    ret = fseek(fp, ofst, SEEK_SET);
    if (ret != 0) {
        printf("blob seek failure.\n");
        return NULL;
    }

    data = (unsigned char *) malloc(sz);
    if (data == NULL) {
        printf("buffer malloc failure.\n");
        return NULL;
    }
    ret = fread(data, 1, sz, fp);
    return data;
}

void Yolov6Base::dump_tensor_attr(rknn_tensor_attr *attr) {
    printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

void Yolov6Base::generateAnchors(const std::vector<int> &fpn_strides, std::vector<AnchorPoints> &anchor_points,
                                 std::vector<float> &stride_tensor, int height, int width) {
    float grid_cell_offset = 0.5;
    std::vector<int> hs = {int(height/8), int(height/16), int(height/32)};
    std::vector<int> ws = {int(width/8), int(width/16), int(width/32)};
    for (int i = 0; i < fpn_strides.size(); i++) {
        int h = hs[i];
        int w = ws[i];
        std::vector<float> shift_x(w);
        std::vector<float> shift_y(h);
        for (int j = 0; j < w; j++) {
            shift_x[j] = j + grid_cell_offset;
        }
        for (int j = 0; j < h; j++) {
            shift_y[j] = j + grid_cell_offset;
        }

        std::vector<std::vector<AnchorPoints>> anchor_point(h, std::vector<AnchorPoints>(w));
        for (int j = 0; j < h; j++) {
            for (int k = 0; k < w; k++) {
                anchor_point[j][k].x = shift_x[k];
                anchor_point[j][k].y = shift_y[j];
            }
        }
        for (int j = 0; j < h; j++) {
            for (int k = 0; k < w; k++) {
                anchor_points.push_back(anchor_point[j][k]);
                stride_tensor.push_back(fpn_strides[i]);
            }
        }
    }
}
static inline float sigmoid(float x) { return 1.0 / (1.0 + expf(-x)); }

void Yolov6Base::generate_yolo_proposals(float * feat_blob, int output_size, float prob_threshold, std::vector<Object>& objects, const std::vector<AnchorPoints> &anchor_points, const std::vector<float> &stride_tensor )
{

    auto dets = output_size / (CLASS_NUM + 5);
    for (int boxs_idx = 0; boxs_idx < dets; boxs_idx++)
    {
        const AnchorPoints ap = anchor_points[boxs_idx];
        const float st = stride_tensor[boxs_idx];
        const int basic_pos = boxs_idx *(CLASS_NUM + 5);
        float left_offset = feat_blob[basic_pos+0];
        float top_offset = feat_blob[basic_pos+1];
        float right_offset = feat_blob[basic_pos+2];
        float bottom_offset = feat_blob[basic_pos+3];
        float x0 = (ap.x - left_offset) * st;
        float y0 = (ap.y - top_offset) * st;
        float x1 = (ap.x + right_offset) * st;
        float y1 = (ap.y + bottom_offset) * st;
        float w = x1 - x0;
        float h = y1 - y0;
        float box_objectness = 1.0;
        // std::cout<<*feat_blob<<std::endl;
        for (int class_idx = 0; class_idx < CLASS_NUM; class_idx++)
        {
            float box_cls_score = sigmoid(feat_blob[basic_pos + 5 + class_idx]);
            float box_prob = box_objectness * box_cls_score;
            if (box_prob > prob_threshold)
            {
                Object obj;
                obj.rect.x = x0;
                obj.rect.y = y0;
                obj.rect.width = w;
                obj.rect.height = h;
                obj.label = class_idx;
                obj.prob = box_prob;
                objects.push_back(obj);
            }

        } // class loop
    }

}
Yolov6Base::~Yolov6Base() {
    spdlog::info("Yolov6Base free");
    if(model_data) free(model_data);
}
