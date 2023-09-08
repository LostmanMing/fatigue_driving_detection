//
// Created by root on 2022/10/15.
//

#ifndef COAL_AI_TCP_CLINENT_H
#define COAL_AI_TCP_CLINENT_H


#include "string"
#include "vector"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#define BUF_SIZE 2048

#define SEND_HANDLE_ID 0x0200
#define SEND_IMAGE_ID  0x0300
#define SEND_ACK_ID    0x0400
#define REV_ACK_ID     0x0800

#define SEND_STAUTS_ID  0x0101
#define SEND_HEART_ID  0x0102
using namespace std;

struct tcp_client {
    uint8_t channel;/*摄像头通道个数*/
    char channel_status[32];/*摄像头通道状态*/
    uint8_t bcd_id[6];/**/
    uint8_t dev_ip[7];/*设备IP*/
    uint8_t camera_ip[7];/*摄像头*/
    uint8_t  alert_num;/*报警个数*/
    uint8_t cloud_res;/*云端返回结果*/
    vector<std::string >ai_type;/*算法类型*/
    uint32_t image_random[50];/*图片随机码*/
    uint8_t image_name_len;/*图片名称长度*/
    std::string  image_name;/*图片名称*/
    std::string  image_path;/*图片路径*/
    uint32_t image_date_len;/*图片数据长度*/
    uint16_t image_bag_sum;/*图片包总数*/
    uint16_t image_bag_num;/*图片第几个包*/
    uint16_t image_bag_len;/*图片包长度*/
    uint16_t image_last_bag_len;// 图片最后一个包字节数、
    uint8_t  image_bag_date[1800];//图片数据
    uint16_t image_read_num ;/*图片包读取个数*/
    uint32_t image_total_offset;/*图片数据偏移量*/

    uint8_t video_name_len;
    std::string video_name;

    uint32_t dev_send_number;/*设备发送数据流水号*/
    uint16_t handle_info_len;/*报警信息消息体长度*/
    uint16_t stauts_info_len;/*状态消息体长度*/
    uint16_t image_info_len;/*图片信息消息体长度*/

    uint16_t server_reply_num;/*平台流水号*/
    uint16_t server_reply_id;/*平台ID*/
    uint8_t  reply_fruit;/*应答结果*/
    int  net_status;

    char link_status;
    char ip[20];
    int port;
    int fd;
    bool  seng_flag ;
    bool  rev_flag ;
    struct sockaddr_in serv_addr;
    unsigned int rev_len;
    unsigned int send_len;
    unsigned char recv_buf[BUF_SIZE+BUF_SIZE];//数据接收缓冲区
    unsigned char send_buf[BUF_SIZE];//数据传输缓冲区
    unsigned char sub_send_buf[BUF_SIZE];
    tcp_client()
    {
        link_status = 0;
        fd = -1;
        seng_flag = true;
        rev_flag = true;

         image_date_len = 0;/*图片数据长度*/
         image_bag_sum = 0;/*图片包总数*/
         image_bag_num = 0;/*图片第几个包*/
         image_bag_len = 0;/*图片包长度*/
         image_last_bag_len = 0;// 图片最后一个包字节数、
         image_read_num = 0 ;/*图片包读取个数*/
         image_total_offset = 0;/*图片数据偏移量*/
        bcd_id[0] = 01 ;
        bcd_id[1] = 36 ;
        bcd_id[2] = 85 ;
        bcd_id[3] = 15 ;
        bcd_id[4] = 27 ;
        bcd_id[5] = 43 ;
        memset((void*)&serv_addr,0,sizeof(struct sockaddr_in));
        memset((void*)&recv_buf, 0, sizeof(recv_buf));
        memset((void*)&send_buf, 0, sizeof(send_buf));
    }
};

extern struct tcp_client  tcp_client;

extern void tcp_client_init(void);
void systime_init_funtion(void);
void  tcp_client_rev_date_thread_funtion(void);
void  tcp_client_send_date_thread_funtion(void);
int create_tcp_client_ipv4(struct dateclient *client);
int connect_date_client_recv_data(int socket_fd,void* recvbuf,int size);
int connect_date_client_send_data(int socket_fd,void* sendbuf, int size);
void Data_packet_Start (unsigned char* sb,unsigned int id,uint16_t len);
void Data_packet_Over(unsigned char* sb,uint16_t len);
unsigned char wubJTT808CalculateChecksum(unsigned char *aubData_p, unsigned int auwDataLength);
void Send_Date(unsigned char* sb,unsigned int id);
void Device_com_reply(unsigned char* sb);
void Device_heartbeat(unsigned char* sb);
void Device_stauts(unsigned char* sb);
void Device_stauts(unsigned char* sb);
void Device_handle_information(unsigned char* sb);

unsigned char NumToBCD(unsigned char num);
unsigned char BCDToNum(unsigned char bcd);
extern uint8_t BCD2DEC(uint8_t bcd);
extern uint8_t DEC2BCD(uint8_t dec);












#endif //COAL_AI_TCP_CLINENT_H
