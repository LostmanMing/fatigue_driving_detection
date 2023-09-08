//
// Created by root on 2022/10/15.
//

#include "tcp_clinent.h"
#include "spdlog/spdlog.h"
#include "common.h"
#include <bitset>
#include <execution>

struct Config_struct config_struct;

struct tm *systime;   //tm结构指针
time_t rawtime;

using namespace std;

FILE *witer_fd;

struct  tcp_client  tcp_client;

void systime_init_funtion(void)
{
    time (&rawtime);
    systime= localtime (&rawtime);
}

union DATA{ uint32_t l32[1]; uint8_t C[4];}u32_to_u8;

uint16_t send_heat_time ;
uint16_t send_stauts_time ;
uint16_t send_head_num;
uint16_t net_stauts_time ;
void tcp_client_init(void)
{
    send_heat_time = 0;
    send_stauts_time = 0;
    net_stauts_time = 0;
    send_head_num = 0;
    tcp_client.channel = config_struct.config_channel_num;
}


int create_tcp_client_ipv4(struct tcp_client *client)
{
    string s = "58.218.237.198";

    strcpy((char*)(client->ip),s.c_str());
    if(nullptr == client->ip)
    {
        spdlog::info( " tcp_client ip 为空 " );
        return -2;
    }

    client->fd = socket(AF_INET,SOCK_STREAM,0);
    if(client->fd<= -1)
    {
        spdlog::info( " tcp_client fd 错误 " );
        return -1;
    }
    client->port = 8813;
    client->serv_addr.sin_port = htons(client->port);
    client->serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, client->ip, &client->serv_addr.sin_addr);

//    spdlog::info( "tcp_client.cpp client->ip ={}  ",client->ip );
//    spdlog::info( "tcp_client.cpp client->port ={}  ",client->port );
    int ret = connect(client->fd, (struct sockaddr *)&client->serv_addr, sizeof(client->serv_addr));
    if(0 != ret)
    {
        spdlog::info( "tcp_client 数据服务器连接失败 " );
        return -3;
    }
    //spdlog::info("tcp_client 数据服务器连接成功！");
    return client->fd;

}
int connect_date_client_recv_data(int socket_fd,void* recvbuf,int size)
{
    if(nullptr == recvbuf || size < 0)
    {
        spdlog::info("error!!! clinet recv buf is null or empty,can't to recv data!!!");
        return -1;
    }
    int recvlen = recv(socket_fd, recvbuf, size, 0);
    return recvlen;
}

int connect_date_client_send_data(int socket_fd,void* sendbuf, int size)
{
    if(size < 0 || nullptr == sendbuf )
    {
        spdlog::info("error!!! client  data is empty ,no need to send!!");
        return -1;
    }
    int sendsize = send(socket_fd,sendbuf,size,0);
    return sendsize;
}




/*******************************************************************
** 函数名:     Data_packet_Start
** 函数描述: 数据包头
** 参数:    sb发送数组 id消息 发送长度len
** 返回:
********************************************************************/
void Data_packet_Start (unsigned char* sb,unsigned int id,uint16_t len)
{

    sb[0]=0x7e;	    //包头
    sb[1]=id>>8; 	//消息ID
    sb[2]=id&0xff;  //消息ID
    sb[3]=len>>8;  	//消息体长度
    sb[4]=len&0xff; //消息体长度
    sb[5]=DEC2BCD(tcp_client.bcd_id[0]) ;
    sb[6]=DEC2BCD(tcp_client.bcd_id[1]) ;
    sb[7]=DEC2BCD(tcp_client.bcd_id[2]);
    sb[8]=DEC2BCD(tcp_client.bcd_id[3]);
    sb[9]=DEC2BCD(tcp_client.bcd_id[4]);
    sb[10]=DEC2BCD(tcp_client.bcd_id[5]) ;
    sb[11]=tcp_client.dev_send_number>>8; 	//消息流水号
    sb[12]=tcp_client.dev_send_number&0xff; //消息流水号

}
uint16_t search(uint8_t *a,uint16_t length,uint8_t *b)
{
    uint16_t len = -1;
    int i;
    vector<uint8_t> sub;
    sub.clear();
    for (i = 1; i < (length-1); i++)
    {
        if (a[i] == 0x7e)
        {
            sub.push_back(0x7d);
            sub.push_back(0x02);
        }
        else if (a[i] == 0x7d)
        {
            sub.push_back(0x7d);
            sub.push_back(0x01);
        }
        else
        {
            sub.push_back(a[i]);
        }
    }
    sub.insert(sub.begin(),0x7e);
    sub.push_back(0x7e);
    len = sub.size();
    for ( i = 0; i < len; i++)
    {
        b[i] = sub[i];
    }
    return len;
}
/*******************************************************************
** 函数名:     Data_packet_Over
** 函数描述: 数据包结尾
** 参数:    sb发送数组 发送长度len
** 返回:
********************************************************************/
void Data_packet_Over(unsigned char* sb,uint16_t len)
{
    uint8_t  crc;
    uint16_t sub_len = 0;

    crc = wubJTT808CalculateChecksum(sb,len+12);/*效验长度消息头12字节+消息体*/
    sb[len+13] = crc;//效验码
    sb[len+14] = 0x7e;

    sub_len = search(sb,len+15,tcp_client.sub_send_buf);

//    int fdlen=0;
//    while(fdlen < sub_len)
//    {
//        fprintf(witer_fd,"%02x ",tcp_client.sub_send_buf[fdlen]);
//        fdlen++;
//    }
//    fprintf(witer_fd,"\n");

    int sendsize =send(tcp_client.fd, tcp_client.sub_send_buf, sub_len, 0);

//    memset(sb, 0, sizeof(sb));
//    memset(tcp_client.sub_send_buf, 0, sizeof(tcp_client.sub_send_buf));
}

/*******************************************************************
** 函数名:     Device_com_reply
** 函数描述: 终端应答
** 参数:    sb发送数组
** 返回:
********************************************************************/
void Device_com_reply(unsigned char* sb)
{
    uint8_t temp =systime->tm_year+1900-2000;
    sb[13] = DEC2BCD(temp);
    temp =systime->tm_mon+1;
    sb[14] = DEC2BCD(temp);
    sb[15] = DEC2BCD(systime->tm_mday);
    sb[16] = DEC2BCD(systime->tm_hour);
    sb[17] = DEC2BCD(systime->tm_min);
    sb[18] = DEC2BCD(systime->tm_sec);

    sb[19] = tcp_client.dev_ip[0];
    sb[20] = tcp_client.dev_ip[1];
    sb[21] = tcp_client.dev_ip[2];
    sb[22] = tcp_client.dev_ip[3];
    sb[23] = tcp_client.dev_ip[4];
    sb[24] = tcp_client.dev_ip[5];
    sb[25] = tcp_client.dev_ip[6];

    sb[26]=tcp_client.server_reply_id>>8; 	//对应平台消息ID
    sb[27]=tcp_client.server_reply_id&0xff;	//对应平台消息ID

    sb[28]=tcp_client.server_reply_num>>8; 	//对应平台消息流水号
    sb[29]=tcp_client.server_reply_num&0xff;	//对应平台消息流水号

    sb[30]=tcp_client.reply_fruit;
    sb[40]=0;
}
/*******************************************************************
** 函数名:     Device_heartbeat
** 函数描述: 发送心跳
** 参数:
** 返回:
********************************************************************/
void Device_heartbeat(unsigned char* sb)
{
    uint8_t temp =systime->tm_year+1900-2000;
    sb[13] = DEC2BCD(temp);
    temp =systime->tm_mon+1;
    sb[14] = DEC2BCD(temp);
    sb[15] = DEC2BCD(systime->tm_mday);
    sb[16] = DEC2BCD(systime->tm_hour);
    sb[17] = DEC2BCD(systime->tm_min);
    sb[18] = DEC2BCD(systime->tm_sec);

    sb[19] = tcp_client.dev_ip[0];
    sb[20] = tcp_client.dev_ip[1];
    sb[21] = tcp_client.dev_ip[2];
    sb[22] = tcp_client.dev_ip[3];
    sb[23] = tcp_client.dev_ip[4];
    sb[24] = tcp_client.dev_ip[5];
    sb[25] = tcp_client.dev_ip[6];
}
/*******************************************************************
** 函数名:     Device_stauts
** 函数描述: 发送设备状态信息
** 参数:    sb发送数组
** 返回:
********************************************************************/
void Device_stauts(unsigned char* sb)
{
    uint16_t  sum;
    std::bitset<32> bs;
    uint8_t temp =systime->tm_year+1900-2000;
    sb[13] = DEC2BCD(temp);
    temp =systime->tm_mon+1;
    sb[14] = DEC2BCD(temp);
    sb[15] = DEC2BCD(systime->tm_mday);
    sb[16] = DEC2BCD(systime->tm_hour);
    sb[17] = DEC2BCD(systime->tm_min);
    sb[18] = DEC2BCD(systime->tm_sec);

    sb[19] = tcp_client.dev_ip[0];
    sb[20] = tcp_client.dev_ip[1];
    sb[21] = tcp_client.dev_ip[2];
    sb[22] = tcp_client.dev_ip[3];
    sb[23] = tcp_client.dev_ip[4];
    sb[24] = tcp_client.dev_ip[5];
    sb[25] = tcp_client.dev_ip[6];
    sb[26] = 0;
    sb[39] = 0;
    sb[41] = tcp_client.channel;
    for(uint8_t i=0;i<32;i++)
    {
        if(tcp_client.channel_status[i] == 1)
        {
            bs[i]=1;
        }
        else  bs[i]=0;
    }
    u32_to_u8.l32[0] = bs.to_ulong();
    sb[42] = u32_to_u8.C[3];
    sb[43] = u32_to_u8.C[2];
    sb[44] = u32_to_u8.C[1];
    sb[45] = u32_to_u8.C[0];
    sb[60] = 0;

    tcp_client.stauts_info_len = 48;
}
/*******************************************************************
** 函数名:     Device_handle_information
** 函数描述: 发送设备识别报警结果信息
** 参数:    sb发送数组
** 返回:
********************************************************************/
void Device_handle_information(unsigned char* sb)
{
    uint16_t  sum;
    uint8_t temp =systime->tm_year+1900-2000;
    sb[13] = DEC2BCD(temp);
    temp =systime->tm_mon+1;
    sb[14] = DEC2BCD(temp);
    sb[15] = DEC2BCD(systime->tm_mday);
    sb[16] = DEC2BCD(systime->tm_hour);
    sb[17] = DEC2BCD(systime->tm_min);
    sb[18] = DEC2BCD(systime->tm_sec);

    sb[19] = 58;
    sb[20] = 46;
    sb[21] = 218;
    sb[22] = 46;
    sb[23] = 237;
    sb[24] = 46;
    sb[25] = 198;
    sb[26] = 0;
    sb[39] = 0;
    sb[40] = 0;

    sb[41] =192;
    sb[42] =46;
    sb[43] =168;
    sb[44] =46;
    sb[45] =6;
    sb[46] =46;
    sb[47] =111;
    //printf("tcp_client file tcp_client.dev_ip = %d %d %d %d %d %d %d\n",tcp_client.dev_ip[0],tcp_client.dev_ip[1],tcp_client.dev_ip[2], \
                    tcp_client.dev_ip[3],tcp_client.dev_ip[4],tcp_client.dev_ip[5],tcp_client.dev_ip[6]);
    //printf("tcp_client file tcp_client.camera_ip = %d %d %d %d %d %d %d\n",tcp_client.camera_ip[0],tcp_client.camera_ip[1],tcp_client.camera_ip[2], \
                    tcp_client.camera_ip[3],tcp_client.camera_ip[4],tcp_client.camera_ip[5],tcp_client.camera_ip[6]);
    sb[48] = 1;/*报警个数*/

//    spdlog::info(tcp_client.ai_type[0]);
    strcpy((char*)(sb+49),tcp_client.ai_type[0].c_str());
    sb[55] = 0;
    sb[56] = 0;
    sb[57] = 0;
    sb[58] = 0;
//        cout << "tcp_client.ai_type =  " << tcp_client.ai_type[i].c_str() <<endl;
//        printf("tcp_client 随机数 :%u \n", tcp_client.image_random[i]);

    sum = 49 +30;

    sb[sum] = 0;//图片名称长度为0
    strcpy((char*)(sb+sum+1),tcp_client.image_name.c_str());
   // tcp_client.image_name_len = 0;
   // tcp_client.image_name.clear();
//    printf(" tcp_client.image_name_len = :%d \n", tcp_client.image_name_len );
//    cout << "tcp_client.image_name =  " << tcp_client.image_name.c_str() <<endl;

    sum +=  1;

    sb[sum] = tcp_client.video_name_len;
    strcpy((char*)(sb+sum+1),tcp_client.video_name.c_str());
    sum = sum + 1 + tcp_client.video_name_len;
    sb[sum] = tcp_client.cloud_res;
    //tcp_client.video_name_len = 0;
    //tcp_client.video_name.clear();


//    printf(" tcp_client.video_len = :%d \n", tcp_client.video_name_len );
//    cout << "tcp_client.video_name =  " << tcp_client.video_name.c_str() <<endl;

//    sum = sum + 1 + tcp_client.video_name_len;
    sum += 1;
    tcp_client.handle_info_len = sum - 13;
    //printf(" tcp_client.handle_info_len = :%d \n", tcp_client.handle_info_len );
}


/*******************************************************************
** 函数名:     Send_Date
** 函数描述: 根据id发送对应数据
** 参数:    sb发送数组
** 返回:
********************************************************************/
void Send_Date(unsigned char* sb,unsigned int id)
{
    /********************发送应答数据*********************/
    if(id == SEND_ACK_ID)
    {
        Device_com_reply(sb);/*终端应答*/
        Data_packet_Start(sb,id,28);/*包头*/
        Data_packet_Over(sb,28);		/*包尾*/
#ifdef DEBUG
        spdlog::info( "tcp_client 发送应答数据!");
#endif
    }
    /********************发送心跳数据*********************/
    else if(id == SEND_HEART_ID)
    {
        Device_heartbeat(sb);/*心跳*/
        Data_packet_Start(sb,id,13);/*包头*/
        Data_packet_Over(sb,13);		/*包尾*/
#ifdef DEBUG
        spdlog::info( "tcp_client 发送心跳数据!");
#endif
    }
        /********************发送设备状态数据*********************/
    else if(id == SEND_STAUTS_ID)
    {
        Device_stauts(sb);/*状态*/
        Data_packet_Start(sb,id,tcp_client.stauts_info_len);/*包头*/
        Data_packet_Over(sb,tcp_client.stauts_info_len);		/*包尾*/
#ifdef DEBUG
        spdlog::info( "tcp_client 发送设备状态数据!");
#endif
    }
        /********************发送报警数据*********************/
    else if(id == SEND_HANDLE_ID)
    {
        Device_handle_information(sb);/*报警图片报警信息*/
        Data_packet_Start(sb,id,tcp_client.handle_info_len);/*包头  ***要放在信息后面.消息体长度在前面计算*/
        Data_packet_Over(sb,tcp_client.handle_info_len);		/*包尾*/
#ifdef DEBUG
        spdlog::info( "tcp_client 发送报警数据!");
#endif

    }
}
/*******************************************************************
** 函数名:     getNetworkStatus
** 函数描述: 获取当前的网络连接情况
** 参数:          无
** 返回:          返回0为断开连接，其他正常连接
********************************************************************/
int getNetworkStatus()
{
    FILE *fp;
    if ((fp = fopen(NETWORK_PATH, "r")) != NULL)
    {
        char ch = fgetc(fp);
        fclose(fp);
        return ch - '0';
    }
    return 0;
}
/*******************************************************************
** 函数名:     tcp_client_send_date_thread_funtion
** 函数描述: 数据发送线程
** 参数:    心跳 设备状态 读取网络状态
** 返回:
********************************************************************/
void  tcp_client_send_date_thread_funtion(void)
{

    while (1)
    {
         sleep(1);

//         if((witer_fd=fopen("../etc/img.txt","wb")) == NULL)
//         {
//             printf("file open failed!");
//         }

        tcp_client.seng_flag =true;
        while (tcp_client.seng_flag)
        {

            if (tcp_client.link_status == 1) /*链接正常*/
            {
                /***函数描述: 100s发送一次心跳状态****************/
                if (++send_heat_time >= 101)/*100s发送一次心跳*/
                {
                    tcp_client.dev_send_number++;/*消息流水号发一条自加一次*/
                    Send_Date(tcp_client.send_buf, SEND_HEART_ID);/*发送数据*/
                    send_heat_time = 0;
                    if (++send_head_num >= 10000) {/*平台心跳应答,暂时未用到*/
                        send_head_num = 0;
                    }

                }
                /***函数描述: 60s发送一次设备状态****************/
                if (++send_stauts_time >= 61)/*60s发送一次*/
                {
                    send_stauts_time = 0;
                    tcp_client.dev_send_number++;/*消息流水号发一条自加一次*/
                    Send_Date(tcp_client.send_buf, SEND_STAUTS_ID);/*发送数据*/
                }

            }
            /***函数描述: 获取当前的网络连接情况,判断当前网络情况****************/
            if (++net_stauts_time > 5)
            {
                net_stauts_time = 0;
                tcp_client.net_status = getNetworkStatus();
            }
            sleep(1);/*10ms*/

        }
       // fclose(witer_fd);
    }

}

/*******************************************************************
** 函数名:   tcp_client_rev_date_thread_funtion
** 函数描述: 数据接收线程
** 参数:    接收资产下发 设备重启命令
** 返回:
********************************************************************/

void  tcp_client_rev_date_thread_funtion(void)
{
    while (1)
    {
        //spdlog::info("tcp_client 数据接收线程");
        tcp_client.fd = create_tcp_client_ipv4(&tcp_client);/*链接IP*/
        if (tcp_client.fd < 0)
        {
            tcp_client.link_status = 0; /*链接错误*/
            close(tcp_client.fd);
            spdlog::info("tcp_client 链接错误");
            sleep(5);
        }
        else
        {
            tcp_client.rev_flag= true;
            tcp_client.link_status = 1; /*链接正常*/
            spdlog::info("tcp_client 链接正常");
            while (tcp_client.rev_flag)
            {
                int recv_ret = connect_date_client_recv_data(tcp_client.fd, tcp_client.recv_buf,
                                                             sizeof(tcp_client.recv_buf));
                if ((recv_ret > 0) && (recv_ret < BUF_SIZE))
                {
                    tcp_client.recv_buf[recv_ret] = '\0';
                    // cout << "DateClient Received：" << tcp_client.recv_buf << " ，Info Length："<< recv_ret << endl;

                    if ((tcp_client.recv_buf[0] == 0x7e) && (tcp_client.recv_buf[recv_ret - 1] == 0x7e))
                    {
                        if((tcp_client.recv_buf[1] == 0xa5)&&(tcp_client.recv_buf[2] == 0x07))
                        {
                            spdlog::info(  " 接收重启指令数据 ");
                            usleep(100);
                            FILE * fp;
                            int maxline = 512;
                            char buffer[maxline];
                            string name = "coal_ai";
                            snprintf(buffer, maxline, "sh -x ../etc/restart.sh %s", name.c_str());
                            fp = popen(buffer, "r");
                            pclose(fp);

                        }

                    }
                }
                else //阻塞进入数据接收阶段,<0或==0情况均需要重新连接
                {
                    tcp_client.rev_flag= false;
                    spdlog::info("tcp_client 接收数据错误，请查看服务器是否断开");
                    tcp_client.link_status = 0; /*链接错误*/
                    close(tcp_client.fd);
                    tcp_client.fd  = -1;
                    break;
                }
            }
        }
    }

}



/*******************************************************************
** 函数名:   wubJTT808CalculateChecksum
** 函数描述: 校验码指从消息头之后开始，同后一字节异或，直到校验码前一个字节，占用一个字节
 * Type_uWord auwCnt = 1;是用来跳过标识位用的
** 参数:    aubData_p数组 auwDataLength长度
** 返回:
********************************************************************/
unsigned char wubJTT808CalculateChecksum(unsigned char *aubData_p, unsigned int auwDataLength)
{
    unsigned char aubChecksum = 0;
    unsigned int auwCnt = 1;
    while(auwCnt <= auwDataLength)
    {
        aubChecksum ^= aubData_p[auwCnt];
        auwCnt++;
    }
    return aubChecksum;
}

uint8_t BCD2DEC(uint8_t bcd)
{
    return (bcd-(bcd>>4)*6);
}

uint8_t DEC2BCD(uint8_t dec)
{
    return (dec+(dec/10)*6);
}


/**十进制转BCD*/
unsigned char NumToBCD(unsigned char num)
{
    unsigned char a, b, bcd;
    a = (num % 10) & 0x0f;
    b = ((num / 10) << 4) & 0xf0;
    bcd = a | b;
    return bcd;
}
/**BCD转十进制**/
unsigned char BCDToNum(unsigned char bcd)
{
    unsigned char a, b;
    a = (bcd >> 4);
    b = bcd & 0x0f;
    return (a * 10 + b);
}

