#include "server.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
// ANSI escape codes for text color
#define RESET   "\033[0m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define SERVER_IP "127.0.0.1"  // 服务器IP地址 
#define SERVER_PORT 3999 // 服务器端口号
#define PKT_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
#define WINDOW_SIZE 16  // 滑动窗口大小
#define _CRT_SECURE_NO_WARNINGS
#define DATA_AREA_SIZE 1024
using namespace std;
SOCKADDR_IN ServerAddr;  // 服务器地址
SOCKADDR_IN ClientAddr;  // 客户端地址
SOCKET serverSocket;  // 服务器套接字
int fileSize;  // 文件大小
unsigned int recvSize;  // 累积收到的文件位置
unsigned int recvBase;  // 期待收到但还未收到的最小分组序号
char* fileName;  // 文件名
char* fileBuffer;  // 读入文件缓冲区
char** receiveBuffer;  // 接收缓冲区
bool isCached[WINDOW_SIZE];  // 标识窗口内的分组有没有被缓存
default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // 自己设置丢包

void printTime() {
    // 获取当前系统时间点
    auto currentTime = std::chrono::system_clock::now();

    // 将时间点转换为时间结构体
    std::time_t time = std::chrono::system_clock::to_time_t(currentTime);
    std::tm tmInfo = *std::localtime(&time);

    // 格式化输出时间
    std::cout << "[ Current Time: "
        << std::put_time(&tmInfo, "%Y-%m-%d %H:%M:%S ]") << std::endl;
}


// Function to print packet information with color
void printPacketInfo(Packet* pkt, const char* packetType) {
    cout << "[" << packetType << "'s info]: ";
    cout << "Size: " << pkt->len << " Bytes, ";
    cout << "FLAG: ";

    if (pkt->FLAG & 0x1) cout << BLUE << "FIN" << RESET << ", ";
    if (pkt->FLAG & 0x2) cout << GREEN << "SYN" << RESET << ", ";
    if (pkt->FLAG & 0x4) cout << "RST, ";
    if (pkt->FLAG & 0x8) cout << BLUE << "HEAD" << RESET << ", ";

    cout << GREEN << "Seq: " << pkt->seq << RESET << ", ";
    cout << BLUE << "Ack: " << pkt->ack << RESET << ", ";
    cout << "Checksum: " << pkt->checksum << ", ";
    cout << "Window Length: " << pkt->window << " ";
    cout << "--------------------------------------" << endl;
}


void printRcvPktMsg(Packet* pkt) {
    printPacketInfo(pkt, "Receive Packet");
}

void printSndPktMsg(Packet* pkt) {
    printPacketInfo(pkt, "Send Packet");
}
void printWindow() {
    cout << "当前接收窗口: [" << recvBase << ", " << recvBase + WINDOW_SIZE - 1 << "]";
}


void InitializeWinsock() 
{   //服务器端socket初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return;
    }
    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket: " << WSAGetLastError() << std::endl;
        return;
    }
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode); // 设置非阻塞模式，等待来自客户端的数据包
    ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//本机任意地址
    ServerAddr.sin_family = AF_INET;//ipv4
    ServerAddr.sin_port = htons(SERVER_PORT);//服务器端口号
    PCTSTR ipAddress = TEXT("127.0.0.1");
    if (InetPton(AF_INET, ipAddress, &(ServerAddr.sin_addr)) != 1) {
        std::cerr << "Invalid IP address." << std::endl;
        closesocket(serverSocket);
        return ;
    }
    if(bind(serverSocket, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        return ;
    }
    else {
        printTime();
        cout << "[Server]:服务器启动成功，等待客户端建立连接" << endl;
    }

}
void connect() 
{
    //模拟TCP三次握手
    int state = 0;  // 握手的状态
    int err = 0; //函数返回信息变量
    int flag = 1;
    int len = sizeof(SOCKADDR);
    Packet* sendPkt = new Packet;
    Packet* recvPkt = new Packet;

    while (flag) {
        switch (state) {
        case 0:  // 等待客户端发起连接
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr),&len);
            if (err > 0) 
            {
                if (recvPkt->FLAG & 0x2) {  // SYN=1
                    printTime();
                    cout << "[Server]:收到来自客户端的连接请求，开始第二次握手：向客户端发送ACK,SYN=1的数据包..." << endl;

                    // 服务器发起第二次握手，向客户端发送ACK,SYN=1的数据包
                    sendPkt->setSYNACK();
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    state = 1;  // 转状态1
                }
                else {
                    printTime();
                    cout << "[Server]:第一次握手阶段收到的数据包有误，重新开始第一次握手..." << endl;
                }
            }
            break;

        case 1:  // 接收客户端第三次握手的ACK=1数据包
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                if (recvPkt->FLAG & 0x4) {  // ACK=1
                    printTime();
                    cout << "[Server]:收到来自客户端第三次握手ACK数据包..." << endl;

                    state = 3;  // 转状态2
                }
                else {
                    printTime();
                    cout << "[Server]:第三次握手阶段收到的数据包有误，重新发送第二次握手的数据包..." << endl;
                    state = 2;  // 转状态重新发送第二次握手的数据包
                }
            }
            break;

        case 2:  // 重新发送第二次握手的数据包状态
            printTime();
            cout << "[Server]:重新发送第二次握手的数据包..." << endl;

            // 重新发送第二次握手，向客户端发送ACK,SYN=1的数据包
            sendPkt->setSYNACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 1;  // 转状态1，等待客户端的ACK=1数据包
            break;

        case 3: // 三次握手结束状态
            printTime();
            cout << "[Server]:三次握手结束，确认已建立连接，开始文件传输..." << endl;
            cout << endl << "**************************************************************************************************" << endl << endl;
            flag = 0;
            break;
        }
    }
}
void disconnect() 
{  
    int state = 0;  // 挥手的状态
    bool flag = 1;
    int err = 0;
    Packet* sendPkt = new Packet;
    Packet* recvPkt = new Packet;
    int len = sizeof(SOCKADDR);
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode); // 设置成非阻塞模式

    while (flag) {
        switch (state) {
        case 0:  // CLOSE_WAIT_1
            printTime();
            cout << "接收到客户端的断开连接请求，开始第二次挥手，向客户端发送ACK=1的数据包..." << endl;

            // 第二次挥手，向客户端发送ACK=1的数据包
            sendPkt->setACK(1);
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 1;  // 转状态1
            break;

        case 1:  // CLOSE_WAIT_2
            printTime();
            cout << "开始第三次挥手，向客户端发送FIN,ACK=1的数据包..." << endl;

            // 第三次挥手，向客户端发送FIN,ACK=1的数据包
            sendPkt->setFINACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 2;  // 转状态2，等待客户端第四次挥手
            break;

        case 2:  // LAST_ACK
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                if (recvPkt->FLAG & 0x4) {  // ACK=1
                    printTime();
                    cout << "收到了来自客户端第四次挥手的ACK数据包..." << endl;

                    state = 3;  // 转状态3
                }
                else {
                    printTime();
                    cout << "第四次挥手阶段收到的数据包有误，重新开始第四次挥手..." << endl;
                }
            }
            break;

        case 3:  // CLOSE
            printTime();
            cout << "四次挥手结束，确认已断开连接..." << endl;
            flag = 0;
            break;
        }
    }
}
void saveFile() {
    string filePath = "C://Users//Betty//Desktop//rcv//";  // 保存路径

    
    //保存路径添加文件名
    for (int i = 0; fileName[i]; i++) {
        filePath += fileName[i];
    }
    

    // 调试信息：输出文件名和文件大小
    cout << "Saving file: " << filePath << " Size: " << fileSize << " Bytes" << endl;

    ofstream fout(filePath, ios::binary | ios::out);

    if (!fout.is_open()) {
        cout << "Error: Failed to open the file for writing." << endl;
        return;
    }

    fout.write(fileBuffer, fileSize);

    if (fout.good()) {
        cout << "File has been saved successfully." << endl;
    }
    else {
        cout << "Error: Failed to write to the file." << endl;
    }

    fout.close();
}
void receiveFile() 
{
    //int nextSeqNum = 0; // 下一个要发送的序号
    int expectedSeqNum = 0; // 期望收到的下一个序号
    int lastAckedSeqNum = -1; // 最后一个确认的序号
    Packet* recvPkt = new Packet;
    Packet* sendPkt = new Packet;
    int err = 0;
    int flag = 1;
    int len = sizeof(SOCKADDR);
    int packetNum = 0;
    float total = 0;  // 总接受的包
    float loss = 0;   // 丢包
    recvSize = 0;
    recvBase = 0;//期待收到但还未收到的最小分组序号
    receiveBuffer = new char* [WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++)receiveBuffer[i] = new char[PKT_LENGTH];
    //接受head包
    while (flag) 
    {
        // 等待接受HEAD状态
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {

                if (isCorrupt(recvPkt)) {  // 检测出数据包损坏
                    printTime();
                    cout << "收到损坏数据包" << endl;
                    //state = 2;
                    //break;
                }
                printRcvPktMsg(recvPkt);
                printTime();
                if (recvPkt->FLAG & 0x8) 
                {  // HEAD=1
                    fileSize = recvPkt->len;
                    fileBuffer = new char[fileSize];
                    fileName = new char[128];
                    memcpy(fileName, recvPkt->data, strlen(recvPkt->data) + 1);
                    packetNum = fileSize % PKT_LENGTH ? fileSize / PKT_LENGTH + 1 : fileSize / PKT_LENGTH;

                    printTime();
                    cout << "收到文件头数据包，文件名: " << fileName;
                    cout << ", 文件大小: " << fileSize << " Bytes，总共需要接收 " << packetNum << " 个数据包";
                    cout << ", 等待发送文件数据包..." << endl << endl;

                    expectedSeqNum++;
                    flag = 0;
                    //state = 2;
                }
                else {
                    printTime();
                    cout << "非文件头数据包，等待发送端重传..." << endl;
                }
            }
        
    }      
    // 循环接收文件数据包
    while (recvBase < packetNum)
    {
        err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
        if (err > 0) 
        {
            if (isCorrupt(recvPkt)) 
            {  // 检测出数据包损坏
                printTime();
                cout << "收到一个损坏的数据包，不做任何处理" << endl << endl;
            }
            printRcvPktMsg(recvPkt);
            printTime();
            cout << "收到第 " << recvPkt->seq << " 号数据包" << endl;
            if (randomNumber(randomEngine) >= 0.05)
            {  // 自主进行丢包测试
                unsigned int seq = recvPkt->seq;
                if (seq >= recvBase && seq < recvBase + WINDOW_SIZE)
                {   // 窗口内的分组被接收
                    //发送ack
                    Packet* sendPkt = new Packet;
                    unsigned int ack = recvPkt->seq;
                    sendPkt->setACK(ack);
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    if (!isCached[seq - recvBase])
                    {  
                        cout << "首次收到该数据包，将其缓存  ";
                        memcpy(receiveBuffer[seq - recvBase], recvPkt->data, PKT_LENGTH);
                        isCached[seq - recvBase] = 1;
                    }
                    else {
                        cout << "已经收到过该数据包...";
                    }

                    if (seq == recvBase) 
                    {   // 该分组序号等于基序号
                        for (int i = 0; i < WINDOW_SIZE; i++) 
                        {  // 窗口移动
                            if (!isCached[i]) break;  // 遇到没有缓存的，停下来
                            recvBase++;
                        }
                        int offset = recvBase - seq;
                        cout << "该数据包序号等于目前接收基序号，窗口移动 " << offset << " 个单位  ";
                        for (int i = 0; i < offset; i++) 
                        {  // 分组交付上层
                            memcpy(fileBuffer + (seq + i) * PKT_LENGTH, receiveBuffer[i], recvPkt->len);
                        }
                        for (int i = 0; i < WINDOW_SIZE - offset; i++) 
                        {
                            //同步平移
                            isCached[i] = isCached[i + offset];  
                            isCached[i + offset] = 0;
                            memcpy(receiveBuffer[i], receiveBuffer[i + offset], PKT_LENGTH);
                        }
                        for (int i = WINDOW_SIZE - offset; i < WINDOW_SIZE; i++) 
                        {
                            isCached[i] = 0; //清零已处理的数据包标记
                        }
                    }
                    printWindow();
                    cout << endl;
                }
                else if (seq >= recvBase - WINDOW_SIZE && seq < recvBase)
                {
                    printWindow();
                    cout << "  该数据包分组序列号在["<< recvBase - WINDOW_SIZE <<","<< recvBase - 1<<"]，发送ACK且不进行其他操作..." << endl;
                    unsigned int ack = recvPkt->seq;
                    sendPkt->setACK(ack);
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    // 接受到在这个范围内的分组则发送ACK，不做其他操作
                }
                else {
                    printWindow();
                    cout << "  分组序号 " << seq << " 不在正确范围，不做任何操作" << endl;
                }
            }
            else {
                loss++;
                cout << "主动丢包" << endl;
            }
        }
    }

    // 跳出循环说明文件接收完毕
    printTime();
    cout << "文件接收完毕..." << endl;
    float lossRate = (loss / packetNum) * 100;
    cout << "丢包率: " << lossRate << "%" << endl;
    cout << endl << "**************************************************************************************************" << endl << endl;

    // 保存文件
    saveFile();

    // 文件保存完毕，等待发送端发送断连请求
    flag = 1;
    while (flag) {
        err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
        if (err > 0) {
            if (recvPkt->FLAG & 0x1) {  // FIN=1
                flag = 0;
                disconnect();
            }
        }
    }
}



int main() {
    // 初始化服务器socket
    InitializeWinsock();

    // 建立连接
    connect();

    // 开始接收文件
    receiveFile();

    // 断连完成后，关闭socket
    int err = closesocket(serverSocket);
    if (err == SOCKET_ERROR) {
        cout << "Close socket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    // 清理退出
    printTime();
    cout << "程序退出..." << endl;
    WSACleanup();
    return 0;
}



