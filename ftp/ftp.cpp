//
// Created by root on 2022/10/19.
//

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include "ftp.h"
#include "spdlog/spdlog.h"

using namespace std;
std::mutex video_file_mutex;

std::string  videofile_path= "../config/video.txt";

struct ftp_struct  ftp_struct;

bool ftp_thread_create_status ;
void ftp_init(void)
{
    ftp_thread_create_status = false;
}
/*字符串分割SplitString*/
static int SplitString( std::string strSrc, std::list<std::string> &strArray , std::string strFlag)
{
    int pos = 1;

    while((pos = (int)strSrc.find_first_of(strFlag.c_str())) > 0)
    {
        strArray.insert(strArray.end(), strSrc.substr(0 , pos));
        strSrc = strSrc.substr(pos + 1, strSrc.length() - pos - 1);
    }

    strArray.insert(strArray.end(), strSrc.substr(0, strSrc.length()));

    return 0;
}

CFTPManager::CFTPManager(void): m_bLogin(false)
{
    m_cmdSocket = socket(AF_INET, SOCK_STREAM, 0);
}

CFTPManager::~CFTPManager(void)
{
    std::string strCmdLine = parseCommand(FTP_COMMAND_QUIT, "");

    Send(m_cmdSocket, strCmdLine.c_str());
    close(m_cmdSocket);
    m_bLogin = false;
}
// ! 登陆服务器
FTP_API CFTPManager::login2Server(const std::string &serverIP)
{
    std::string strPort;
    int pos = serverIP.find_first_of(":");/*查找IP里有没有端口*/

    if (pos > 0)
    {
        strPort = serverIP.substr(pos + 1, serverIP.length() - pos);
    }
    else
    {
        pos = serverIP.length();
        strPort = FTP_DEFAULT_PORT;
        //strPort = "FTP_DEFAULT_PORT";
    }

    m_strServerIP = serverIP.substr(0, pos);
    m_nServerPort = atol(strPort.c_str());

    //spdlog::info("登陆服务器 IP: {} port: {}", m_strServerIP.c_str(), m_nServerPort);

    if (Connect(m_cmdSocket, m_strServerIP, m_nServerPort) < 0)
    {

        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);/*// ! 返回服务器信息*/
    //spdlog::info("登陆服务器 @@@@Response: {}", m_strResponse.c_str());
    if(m_strResponse.empty())
    {
        //spdlog::info("登陆服务器 m_strResponse 为空");
        return -1;
    }
    return	parseResponse(m_strResponse);
}
// !输入用户名
FTP_API CFTPManager::inputUserName(const std::string &userName)
{
    std::string strCommandLine = parseCommand(FTP_COMMAND_USERNAME, userName);// !合成发送到服务器的命令

    m_strUserName = userName;
    //spdlog::info("输入用户名 m_strUserName: {}", m_strUserName.c_str());
    if (Send(m_cmdSocket, strCommandLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);
    if(m_strResponse.empty())
    {
        //spdlog::info("登陆服务器 m_strResponse 为空");
        return -1;
    }
    //spdlog::info("输入用户名 Response: {}", m_strResponse.c_str());

    return parseResponse(m_strResponse);
}
// !输入密码
FTP_API CFTPManager::inputPassWord(const std::string &password)
{
    std::string strCmdLine = parseCommand(FTP_COMMAND_PASSWORD, password);// !合成发送到服务器的命令

    m_strPassWord = password;
    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }
    else
    {
        m_bLogin = true;
        m_strResponse = serverResponse(m_cmdSocket);
        //spdlog::info("输入密码 Response: {}", m_strResponse.c_str());
        if(m_strResponse.empty())
        {
            //spdlog::info("登陆服务器 m_strResponse 为空");
            return -1;
        }
        return parseResponse(m_strResponse);
    }
}
// !退出FTP
FTP_API CFTPManager::quitServer(void)
{
    std::string strCmdLine = parseCommand(FTP_COMMAND_QUIT, "");
    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }
    else
    {
        m_strResponse = serverResponse(m_cmdSocket);
       // printf("退出FTP Response: %s\n", m_strResponse.c_str());

        return parseResponse(m_strResponse);
    }

}
// !命令： PWD
const std::string CFTPManager::PWD()
{
    std::string strCmdLine = parseCommand(FTP_COMMAND_CURRENT_PATH, "");

    if (Send(m_cmdSocket, strCmdLine.c_str()) < 0)
    {
        return "";
    }
    else
    {
        return serverResponse(m_cmdSocket);
    }
}

// !设置传输格式 2进制  还是ascii方式传输*/
FTP_API CFTPManager::setTransferMode(type mode)
{
    std::string strCmdLine;

    switch (mode)
    {
        case binary:
            strCmdLine = parseCommand(FTP_COMMAND_TYPE_MODE, "I");
            break;
        case ascii:
            strCmdLine = parseCommand(FTP_COMMAND_TYPE_MODE, "A");
            break;
        default:
            break;
    }

    if (Send(m_cmdSocket, strCmdLine.c_str()) < 0)
    {
        assert(false);
    }
    else
    {
        m_strResponse  = serverResponse(m_cmdSocket);
        //printf("设置传输格式 Response: %s", m_strResponse.c_str());

        return parseResponse(m_strResponse);
    }
}

// !设置为被动模式
const std::string CFTPManager::Pasv()
{
    std::string strCmdLine = parseCommand(FTP_COMMAND_PSAV_MODE, "");

    if (Send(m_cmdSocket, strCmdLine.c_str()) < 0)
    {
        return "";
    }
    else
    {
        m_strResponse = serverResponse(m_cmdSocket);

        return m_strResponse;
    }
}

// ! 命令： DIR
const std::string CFTPManager::Dir(const std::string &path)
{
    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (createDataLink(dataSocket) < 0)
    {
        return "";
    }
    // 数据连接成功
    std::string strCmdLine = parseCommand(FTP_COMMAND_DIR, path);/*合成发送到服务器的命令*/

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());
        close(dataSocket);
        return "";
    }
    else
    {
        trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());
        m_strResponse = serverResponse(dataSocket);

        trace("@@@@Response: \n%s\n", m_strResponse.c_str());
        close(dataSocket);

        return m_strResponse;
    }

}

// !命令 ： CD
FTP_API CFTPManager::CD(const std::string &path)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_CHANGE_DIRECTORY, path);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);

    trace("@@@@Response: %s\n", m_strResponse.c_str());
    return parseResponse(m_strResponse);
}
// ！删除文件
FTP_API CFTPManager::DeleteFile(const std::string &strRemoteFile)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_DELETE_FILE, strRemoteFile);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);
    printf("@@@@Response: %s\n", m_strResponse.c_str());
    return parseResponse(m_strResponse);
}
// ! 删除文件夹/目录
FTP_API CFTPManager::DeleteDirectory(const std::string &strRemoteDir)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_DELETE_DIRECTORY, strRemoteDir);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);

    trace("@@@@Response: %s\n", m_strResponse.c_str());
    return parseResponse(m_strResponse);
}
// ! 创建目录/文件夹
FTP_API CFTPManager::CreateDirectory(const std::string &strRemoteDir)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_CREATE_DIRECTORY, strRemoteDir);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);

    trace("@@@@Response: %s\n", m_strResponse.c_str());
    return parseResponse(m_strResponse);
}
// !重命名
FTP_API CFTPManager::Rename(const std::string &strRemoteFile, const std::string &strNewFile)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_RENAME_BEGIN, strRemoteFile);
    Send(m_cmdSocket, strCmdLine);
    trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());

    Send(m_cmdSocket, parseCommand(FTP_COMMAND_RENAME_END, strNewFile));

    m_strResponse = serverResponse(m_cmdSocket);
    trace("@@@@Response: %s\n", m_strResponse.c_str());
    return parseResponse(m_strResponse);
}
// !获取文件大小
long CFTPManager::getFileLength(const std::string &strRemoteFile)
{
    assert(m_cmdSocket != INVALID_SOCKET);

    std::string strCmdLine = parseCommand(FTP_COMMAND_FILE_SIZE, strRemoteFile);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }

    m_strResponse = serverResponse(m_cmdSocket);

    //trace("获取服务器文件大小 Response: %s\n", m_strResponse.c_str());

    std::string strData = m_strResponse.substr(0, 3);
    unsigned long val = atol(strData.c_str());

    if (val == 213)
    {
        strData = m_strResponse.substr(4);
        //trace("获取服务器文件大小 strData: %s\n", strData.c_str());
        val = atol(strData.c_str());

        return val;
    }

    return -1;
}

// !关闭连接
void CFTPManager::Close(int sock)
{
    shutdown(sock, SHUT_RDWR);
    close(sock);
    sock = INVALID_SOCKET;
}
// 下载文件
FTP_API CFTPManager::Get(const std::string &strRemoteFile, const std::string &strLocalFile)
{
    return downLoad(strRemoteFile, strLocalFile);
}

// 上载文件  支持断电传送方式
//先获取本地文件大小，与服务器文件比较，如果小于服务器文件大小，则开始断点续传，本地文件用append模式打开，从文件末尾写入。
//设置偏移量REST 本地文件大小，之前还要切换成Binary模式，一般FTP服务器默认的是Ascii模式，Ascii模式是不能进行断点续传的。
FTP_API CFTPManager::Put(const std::string &strRemoteFile, const std::string &strLocalFile)
{
    std::string strCmdLine;
    const unsigned long dataLen = FTP_DEFAULT_BUFFER;
    char strBuf[dataLen] = {0};
    unsigned long nSize = getFileLength(strRemoteFile);
    unsigned long nLen = 0;

    FILE *pFile = NULL; /* 需要注意 */
    pFile = fopen(strLocalFile.c_str(), "rb");  // 以只读方式打开  且文件必须存在
    assert(pFile != NULL);

    int data_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(data_fd != -1);

    if (createDataLink(data_fd) < 0)
    {
        return -1;
    }

    if (nSize == -1)
    {
        strCmdLine = parseCommand(FTP_COMMAND_UPLOAD_FILE, strRemoteFile);
    }
    else
    {
        strCmdLine = parseCommand(FTP_COMMAND_APPEND_FILE, strRemoteFile);
    }

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        Close(data_fd);
        return -1;
    }

    //trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());

    fseek(pFile, nSize, SEEK_SET);
    while (!feof(pFile))
    {
        nLen = fread(strBuf, 1, dataLen, pFile);
        if (nLen < 0)
        {
            break;
        }
        if (Send(data_fd, strBuf,nLen) < 0)
        {
            Close(data_fd);
            return -1;
        }
    }

    //trace("@@@@Response: %s\n", serverResponse(data_fd).c_str());
    Close(data_fd);
    //trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());
    fclose(pFile);
    pFile = NULL; /* 需要指向空，否则会指向原打开文件地址 */
    return 0;

}
// !合成发送到服务器的命令
const std::string CFTPManager::parseCommand(const unsigned int command, const std::string &strParam)
{
    if (command < FTP_COMMAND_BASE || command > FTP_COMMAND_END)
    {
        return "";
    }

    std::string strCommandLine;

    m_nCurrentCommand = command;
    m_commandStr.clear();

    switch (command)
    {
        case FTP_COMMAND_USERNAME:
            strCommandLine = "USER ";
            break;
        case FTP_COMMAND_PASSWORD:
            strCommandLine = "PASS ";
            break;
        case FTP_COMMAND_QUIT:
            strCommandLine = "QUIT ";
            break;
        case FTP_COMMAND_CURRENT_PATH:
            strCommandLine = "PWD ";
            break;
        case FTP_COMMAND_TYPE_MODE:
            strCommandLine = "TYPE ";
            break;
        case FTP_COMMAND_PSAV_MODE:
            strCommandLine = "PASV ";
            break;
        case FTP_COMMAND_DIR:
            strCommandLine = "LIST ";
            break;
        case FTP_COMMAND_CHANGE_DIRECTORY:
            strCommandLine = "CWD ";
            break;
        case FTP_COMMAND_DELETE_FILE:
            strCommandLine = "DELE ";
            break;
        case FTP_COMMAND_DELETE_DIRECTORY:
            strCommandLine = "RMD ";
            break;
        case FTP_COMMAND_CREATE_DIRECTORY:
            strCommandLine = "MKD ";
            break;
        case FTP_COMMAND_RENAME_BEGIN:
            strCommandLine = "RNFR ";
            break;
        case FTP_COMMAND_RENAME_END:
            strCommandLine = "RNTO ";
            break;
        case FTP_COMMAND_FILE_SIZE:
            strCommandLine = "SIZE ";
            break;
        case FTP_COMMAND_DOWNLOAD_FILE:
            strCommandLine = "RETR ";
            break;
        case FTP_COMMAND_DOWNLOAD_POS:
            strCommandLine = "REST ";
            break;
        case FTP_COMMAND_UPLOAD_FILE:
            strCommandLine = "STOR ";
            break;
        case FTP_COMMAND_APPEND_FILE:
            strCommandLine = "APPE ";
            break;
        default :
            break;
    }

    strCommandLine += strParam;
    strCommandLine += "\r\n";

    m_commandStr = strCommandLine;
   // spdlog::info("合成发送到服务器的命令 parseCommand: {}", m_commandStr.c_str());

    return m_commandStr;
}
// ! 建立连接
FTP_API CFTPManager::Connect(int socketfd, const std::string &serverIP, unsigned int nPort)
{
    if (socketfd == INVALID_SOCKET)
    {
        return -1;
    }

    unsigned int argp = 1;
    int error = -1;
    int len = sizeof(int);
    struct sockaddr_in  addr;
    bool ret = false;
    timeval stime;
    fd_set  set;

    ioctl(socketfd, FIONBIO, &argp);  //设置为非阻塞模式

    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port	= htons(nPort);
    addr.sin_addr.s_addr = inet_addr(serverIP.c_str());
    bzero(&(addr.sin_zero), 8);

    //trace("Address: %s %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

    if (connect(socketfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) == -1)   //若直接返回 则说明正在进行TCP三次握手
    {
        stime.tv_sec = 20;  //设置为1秒超时
        stime.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(socketfd, &set);

        if (select(socketfd + 1, NULL, &set, NULL, &stime) > 0)   ///在这边等待 阻塞 返回可以读的描述符 或者超时返回0  或者出错返回-1
        {
            getsockopt(socketfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t*)&len);
            if (error == 0)
            {
                ret = true;
            }
            else
            {
                ret = false;
            }
        }
    }
    else
    {	trace("Connect Immediately!!!\n");
        ret = true;
    }

    argp = 0;
    ioctl(socketfd, FIONBIO, &argp);

    if (!ret)
    {
        close(socketfd);
        fprintf(stderr, "cannot connect server!!\n");
        return -1;
    }

    //fprintf(stdout, "Connect!!!\n");

    return 0;
}

// ! 返回服务器信息
const std::string CFTPManager::serverResponse(int sockfd)
{
    if (sockfd == INVALID_SOCKET)
    {
        return "";
    }

    int nRet = -1;
    char buf[MAX_PATH] = {0};

    m_strResponse.clear();

    while ((nRet = getData(sockfd, buf, MAX_PATH)) > 0)
    {
        buf[MAX_PATH - 1] = '\0';
        m_strResponse += buf;
    }

    return m_strResponse;
}
// !获取服务器数据
FTP_API CFTPManager::getData(int fd, char *strBuf, unsigned long length)
{
    assert(strBuf != NULL);

    if (fd == INVALID_SOCKET)
    {
        return -1;
    }

    memset(strBuf, 0, length);
    timeval stime;
    int nLen;

    stime.tv_sec = 1;
    stime.tv_usec = 0;

    fd_set	readfd;
    FD_ZERO( &readfd );
    FD_SET(fd, &readfd );

    if (select(fd + 1, &readfd, 0, 0, &stime) > 0)
    {
        if ((nLen = recv(fd, strBuf, length, 0)) > 0)
        {
            return nLen;
        }
        else
        {
            return -2;
        }
    }
    return 0;
}
// !发送命令
FTP_API CFTPManager::Send(int fd, const std::string &cmd)
{
    if (fd == INVALID_SOCKET)
    {
        return -1;
    }

    return Send(fd, cmd.c_str(), cmd.length());
}
// !发送命令
FTP_API CFTPManager::Send(int fd, const char *cmd, const size_t len)
{
    if((FTP_COMMAND_USERNAME != m_nCurrentCommand)
       &&(FTP_COMMAND_PASSWORD != m_nCurrentCommand)
       &&(!m_bLogin))
    {
        return -1;
    }

    timeval timeout;
    timeout.tv_sec  = 1;
    timeout.tv_usec = 0;

    fd_set  writefd;
    FD_ZERO(&writefd);
    FD_SET(fd, &writefd);

    if(select(fd + 1, 0, &writefd , 0 , &timeout) > 0)
    {
        size_t nlen  = len;
        int nSendLen = 0;
        while (nlen >0)
        {
            nSendLen = send(fd, cmd , (int)nlen , 0);

            if(nSendLen == -1)
                return -2;

            nlen = nlen - nSendLen;
            cmd +=  nSendLen;
        }
        return 0;
    }
    return -1;
}

// !建立数据连接
FTP_API CFTPManager::createDataLink(int data_fd)
{
    assert(data_fd != INVALID_SOCKET);

    std::string strData;
    unsigned long nPort = 0 ;
    std::string strServerIp ;
    std::list<std::string> strArray ;

    std::string parseStr = Pasv();

    if (parseStr.size() <= 0)
    {
        return -1;
    }

    //trace("parseInfo: %s\n", parseStr.c_str());

    size_t nBegin = parseStr.find_first_of("(");
    size_t nEnd	  = parseStr.find_first_of(")");
    strData		  = parseStr.substr(nBegin + 1, nEnd - nBegin - 1);

    //trace("ParseAfter: %s\n", strData.c_str());
    if( SplitString( strData , strArray , "," ) <0)
        return -1;

    if( ParseString( strArray , nPort , strServerIp) < 0)
        return -1;

    //trace("nPort: %ld IP: %s\n", nPort, strServerIp.c_str());

    if (Connect(data_fd, strServerIp, nPort) < 0)
    {
        return -1;
    }

    return 0;

}
// !解析PASV模式返回的字符串获取FTP端口号和FTP服务器IP
FTP_API CFTPManager::ParseString(std::list<std::string> strArray, unsigned long & nPort ,std::string & strServerIp)
{
    if (strArray.size() < 6 )
        return -1 ;

    std::list<std::string>::iterator citor;
    citor = strArray.begin();
    strServerIp = *citor;
    strServerIp += ".";
    citor ++;
    strServerIp += *citor;
    strServerIp += ".";
    citor ++ ;
    strServerIp += *citor;
    strServerIp += ".";
    citor ++ ;
    strServerIp += *citor;
    citor = strArray.end();
    citor--;
    nPort = atol( (*citor).c_str());
    citor--;
    nPort += atol( (*(citor)).c_str()) * 256 ;
    return 0 ;
}
// 打开本地文件
FILE *CFTPManager::createLocalFile(const std::string &strLocalFile)
{
    return fopen(strLocalFile.c_str(), "w+b");
}
// 下载文件
FTP_API CFTPManager::downLoad(const std::string &strRemoteFile, const std::string &strLocalFile, const int pos, const unsigned int length)
{
    assert(length >= 0);

    FILE *file = NULL;
    unsigned long nDataLen = FTP_DEFAULT_BUFFER;
    char strPos[MAX_PATH]  = {0};
    int data_fd = socket(AF_INET, SOCK_STREAM, 0);

    assert(data_fd != -1);

    if ((length != 0) && (length < nDataLen))
    {
        nDataLen = length;
    }
    char *dataBuf = new char[nDataLen];//定义缓冲区，字符指针指向nDataLen字节的内存地址
    assert(dataBuf != NULL);

    sprintf(strPos, "%d", pos);

    if (createDataLink(data_fd) < 0)
    {
        trace("@@@@ Create Data Link error!!!\n");
        return -1;
    }

    std::string strCmdLine = parseCommand(FTP_COMMAND_DOWNLOAD_POS, strPos);
    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }
    trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());

    strCmdLine = parseCommand(FTP_COMMAND_DOWNLOAD_FILE, strRemoteFile);

    if (Send(m_cmdSocket, strCmdLine) < 0)
    {
        return -1;
    }
    trace("@@@@Response: %s\n", serverResponse(m_cmdSocket).c_str());

    file = createLocalFile(std::string(FTP_DEFAULT_PATH + strLocalFile));
    assert(file != NULL);

    int len = 0;
    int nReceiveLen = 0;
    while ((len = getData(data_fd, dataBuf, nDataLen)) > 0)
    {
        nReceiveLen += len;

        int num = fwrite(dataBuf, 1, len, file);
        memset(dataBuf, 0, sizeof(dataBuf));

        //trace("%s", dataBuf);
        trace("Num:%d\n", num);
        if (nReceiveLen == length && length != 0)
            break;

        if ((nReceiveLen + nDataLen) > length  && length != 0)
        {
            delete []dataBuf;//调用使用类对象的析构函数释放用户自己分配内存空间并且   释放了dataBuf指针指向的全部内存空间
            nDataLen = length - nReceiveLen;
            dataBuf = new char[nDataLen];
        }
    }

    Close(data_fd);
    fclose(file);
    delete []dataBuf;

    return 0;
}
/*******************************************************************
** 函数名:   parseResponse
** 函数描述: 解析返回ftp命令的值,每一个 FTP 命令发送之后，FTP 服务器都会返回一个字符串，
 * 其中包括一个响应代码和一些说明信息。
** 参数:
** 返回:其中的返回码主要是用于判断命令是否被成功执行了。
********************************************************************/
FTP_API CFTPManager::parseResponse(const std::string &str)
{
    assert(!str.empty());/*查表达式expresion的值来决定是否需要终止执行程序。*/

    std::string strData = str.substr(0, 3);
    unsigned int val = atoi(strData.c_str());
//    switch (val)
//    {
//        case 110:trace("新文件指示器上的重启标记\n");
//        break;
//        case 120:trace("在段时间内服务器准备就绪\n");
//            break;
//        case 125:trace("数据连接已打开,在短时间内开始传输\n");
//            break;
//        case 150:trace("文件OK,数据连接将在短时间内打开\n");
//            break;
//        case 200:trace("成功\n");
//            break;
//        case 202:trace("不执行的命令\n");
//            break;
//        case 211:trace("系统状态回复\n");
//            break;
//        case 220:trace("服务器准备就绪\n");
//            break;
//        case 221:trace("服务关闭\n");
//            break;
//        case 225:trace("数据连接打开\n");
//            break;
//        case 226:trace("数据连接关闭\n");
//            break;
//        case 227:trace("进入被动模式\n");
//            break;
//        case 230:trace("登录完成\n");
//            break;
//        case 250:trace("完成的文件行为\n");
//            break;
//        case 257:trace("建立的路径名\n");
//            break;
//        case 331:trace("用户名有效，需要密码\n");
//            break;
//        case 332:trace("需要账号名\n");
//            break;
//        case 350:trace("未决的文件行为\n");
//            break;
//        case 421:trace("关闭服务器\n");
//            break;
//        case 425:trace("不能打开数据连接\n");
//            break;
//        case 426:trace("结束连接\n");
//            break;
//        case 450:trace("文件不可用\n");
//            break;
//        case 502:trace("命令未执行\n");
//            break;
//        case 530:trace("登录失败\n");
//            break;
//        case 550:trace("不可用的文件\n");
//            break;
//        case 553:trace("不允许的文件名\n");
//            break;
//    }
    return val;
}


/*******************************************************************
** 函数名:     readTxtfile
** 函数描述: 读取一个TXT文件中的每一行到一个向量中
** 参数:
** 返回:      读取结果状态
********************************************************************/
bool readTxtfile(string&  strPath, vector<vector<string>>& TxtData)
{
    string str,str1;
    ifstream txtfile(strPath);/*从已有的文件中读。*/
    if (!txtfile.is_open())
    {
        cout<<"readTxtfile 不能打开文件!"<<endl;

        return false;
    }
    while (getline(txtfile, str))/*利用getline的方法一行一行的读取，每一行作为一个字符串，放到TxtData的第二维*/
    {
        istringstream input(str);  //针对每一行操作的对象，用于字符串切割、转化
        vector<string> everyRow;
        while (input >> str1)/*将读入的一行中的整数存储到p*/
        {
            everyRow.push_back(str1);
        }
        TxtData.push_back(everyRow);
    }
    txtfile.close();
    return true;
}

/*******************************************************************
** 函数名:     cleanTxtfile
** 函数描述: 清除路径文件内容
** 参数:          文件路径
** 返回:          清理结果
********************************************************************/
bool cleanTxtfile(string strPath)
{
    ofstream file_writer(strPath, ios_base::out);
    if (!file_writer.is_open())
    {
        return 1;
    }
    std::cout<<"cleanTxtfile = "<< strPath <<std::endl;
    file_writer.close();
    return 0;
}

/*******************************************************************
** 函数名:   ftp_normal_send_funtion
** 函数描述: 上传视频文件
** 参数:
** 返回:
********************************************************************/
bool ftp_normal_send_funtion(struct ftp_struct*  ftp_struct)
{
        int return_value;
        CFTPManager *ftp = new CFTPManager();
        return_value = ftp->login2Server(ftp_struct->ServerIP);// ! 登陆服务器
        if(return_value == -1 )
        {
            spdlog::info( "登陆服务器失败 ");
            ftp->quitServer();
            delete ftp;//删除对象
            return true;
        }
        return_value = ftp->inputUserName(ftp_struct->UserName);// !输入用户名
        if(return_value == -1 )
        {
            spdlog::info( "输入用户名失败 ");
            ftp->quitServer();
            delete ftp;//删除对象
            return true;
        }
        if(ftp->inputPassWord(ftp_struct->PassWord) == 230)// !输入密码
        {
            ftp_thread_create_status = true;
            spdlog::info(" ftp 线程密码正确,进入ftp线程 ");
        }
        else if(ftp->inputPassWord(ftp_struct->PassWord) == -1)
        {
            spdlog::info( "输入密码失败 ");
            ftp->quitServer();
            delete ftp;//删除对象
            return true;
        }

        vector<vector<string>> readData;
        /***********************读取设备配置文件文件******************************/
        readData.clear();
        video_file_mutex.lock();
        bool ret =readTxtfile(videofile_path , readData);
        video_file_mutex.unlock();
        if(ret == true)
        {
            if(readData.size() == 0 )
            {
                spdlog::info( " 读取ftp视频文件 为空 ");
                ftp->quitServer();
                delete ftp;//删除对象
                return true;
            }
            ftp_struct->read_video_num = readData.size();
            //spdlog::info("读取ftp视频文件个数 ftp_struct->read_video_num ={}", ftp_struct->read_video_num );
            cleanTxtfile(videofile_path);
        }
        else
        {
            spdlog::info(  " 读取 videofile_path 失败 ");
            ftp->quitServer();
            delete ftp;//删除对象
            return false;
        }
        std::string RemoteFile;
        std::string LocalFile;
        do{
            RemoteFile.clear();
            LocalFile.clear();
            RemoteFile = readData[ftp_struct->read_video_num-1][1].c_str();
            LocalFile = readData[ftp_struct->read_video_num-1][0]+ RemoteFile;
            //spdlog::info( " ftp 线程上传视频RemoteFile = {} ",RemoteFile.c_str());
            //spdlog::info( " ftp 线程上传视频RemoteFile = {} ", LocalFile.c_str());
            ret = ftp->Put(RemoteFile.c_str(),LocalFile.c_str());
            if( ret == 0)
            {
               spdlog::info(  " ftp 线程上传视频成功 ");
                ftp_struct->read_video_num--;
            }
            else
            {
                spdlog::info(" ftp 线程上传视频失败 ");
                ftp_struct->read_video_num = 0;
                spdlog::info("直接退出do while循环" );
                ftp->quitServer();
                delete ftp;//删除对象
                return false;
            }
            usleep(1000);
        }
        while(ftp_struct->read_video_num) ;
        ftp->quitServer();
        delete ftp;//删除对象
        return true;
}


/*******************************************************************
** 函数名:   ftp_thread_funtion
** 函数描述: ftp线程
** 参数:
** 返回:
********************************************************************/
void  ftp_thread_funtion(void)
{
    while (1)
    {
        ftp_normal_send_funtion( &ftp_struct);
        //spdlog::info(" ftp 线程运行 ");
        sleep(20);
    }

}
