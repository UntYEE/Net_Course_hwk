#include "server.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SERVER_IP "127.0.0.1"  // 服务器IP地址 
#define SERVER_PORT 3999 // 服务器端口号
#define PKT_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
#define _CRT_SECURE_NO_WARNINGS
using namespace std;
SOCKADDR_IN ServerAddr;  // 服务器地址
SOCKADDR_IN ClientAddr;  // 客户端地址
SOCKET serverSocket;  // 服务器套接字
char* fileName;
char* fileBuffer;
int fileSize;  // 文件大小
unsigned int recvSize;  // 累积收到的文件位置
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
void printRcvPktMsg(Packet* pkt) {
    cout << "[Receive Packet's information]:" << endl;
    cout << "Packet Information:" << endl;
    cout << "  Size: " << pkt->len << " Bytes" << endl;
    cout << "  FLAG: " << pkt->FLAG << endl;
    cout << "  Sequence Number: " << pkt->seq << endl;
    cout << "  Acknowledgment Number: " << pkt->ack << endl;
    cout << "  Checksum: " << pkt->checksum << endl;
    cout << "  Window Length: " << pkt->window << endl;
    cout << "--------------------------------------" << endl;
}
void printSndPktMsg(Packet* pkt) {
    cout << "[Send Packet's information]:" << endl;
    cout << "Packet Information:" << endl;
    cout << "  Size: " << pkt->len << " Bytes" << endl;
    cout << "  FLAG: " << pkt->FLAG << endl;
    cout << "  Sequence Number: " << pkt->seq << endl;
    cout << "  Acknowledgment Number: " << pkt->ack << endl;
    cout << "  Checksum: " << pkt->checksum << endl;
    cout << "  Window Length: " << pkt->window << endl;
    cout << "--------------------------------------" << endl;
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
            sendPkt->setACK();
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
    string filePath = "C:\\Users\\lenovo\\Desktop\\rcv\\";  // 保存路径

    
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

void receiveFile() {
    int state = 0;
    bool flag = 1;
    int waitSeq = 0;//期望序列号

    Packet* recvPkt = new Packet;
    Packet* sendPkt = new Packet;
    int err = 0;
    int len = sizeof(SOCKADDR);
   
    int packetNum = 0;
    float total = 0;//总接受的包
    float loss = 0;//丢包
    // 循环接收文件数据包
    while (flag) {
        switch (state) {
        case 0:  // 等待接受HEAD状态
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr),&len);
            if (err > 0) {
                printRcvPktMsg(recvPkt);
                if (err > 0) {
                    if (isCorrupt(recvPkt)) {  // 检测出数据包损坏
                        printTime();
                        cout << "收到损坏的数据包，向发送端发送 ack=" << waitSeq << " 数据包" << endl;

                        state = 2;
                        break;
                    }
                    if (recvPkt->FLAG & 0x8) {  // HEAD=1
                        fileSize = recvPkt->len;
                        fileBuffer = new char[fileSize];
                        fileName = new char[128];
                        memcpy(fileName, recvPkt->data, strlen(recvPkt->data) + 1);
                        packetNum = fileSize % PKT_LENGTH ? fileSize / PKT_LENGTH + 1 : fileSize / PKT_LENGTH;

                        printTime();
                        cout << "收到来自发送端的文件头数据包，文件名为: " << fileName;
                        cout << "。文件大小为: " << fileSize << " Bytes，总共需要接收 " << packetNum << " 个数据包";
                        cout << "，等待发送文件数据包..." << endl << endl;

                        waitSeq++;
                        state = 2;
                    }
                    else {
                        printTime();
                        cout << "收到的数据包不是文件头，等待发送端重传..." << endl;
                    }
                }
            }
            break;

        case 1:  // 等待文件数据包状态
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                printRcvPktMsg(recvPkt);
                if (isCorrupt(recvPkt)) {  // 检测出数据包损坏
                    printTime();
                    cout << "收到一个损坏的数据包，向发送端发送 ack=" << waitSeq << " 数据包" << endl << endl;
                    state = 2;//收到的是之前的seq
                    break;
                }
                if (recvPkt->seq == waitSeq) {  // 收到了目前等待的包
                    if (randomNumber(randomEngine) >= 0.05) //设置丢包率是0.1
                    {
                        memcpy(fileBuffer + recvSize, recvPkt->data, recvPkt->len);
                        recvSize += recvPkt->len;

                        printTime();
                        cout << "收到第 " << recvPkt->seq << " 号数据包，向发送端发送 ack=" << waitSeq << endl << endl;
                        total++;
                        waitSeq++;//收到的是当前的seq
                        state = 2;
                    }
                    else {
                        loss++;
                        total++;
                        cout << "主动丢包" << endl;
                       // state = 2;
                        //break;
                    }
                }
                else {  // 不是目前等待的数据包
                    printTime();
                    cout << "收到第 " << recvPkt->seq << " 号数据包，但并不是需要的数据包，向发送端发送 ack=" << waitSeq << endl << endl;
                }
            }

        case 2:  // 发送ACK
            sendPkt->ack = waitSeq - 1;
            sendPkt->setACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
            state = 1;
            if (recvSize == fileSize) {  // 文件接收完毕
                printTime();
                cout << "文件接收完毕..." << endl;
                cout << "丢包率：  " << float(loss / total)*100<<"%" << endl;
                cout << endl << "**************************************************************************************************" << endl << endl;

                // 保存文件
                saveFile();

                state = 3;
            }
            break;

        case 3:  // 文件已接收完毕，等待发送端发送断连请求
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                if (recvPkt->FLAG & 0x1) {  // FIN=1
                    flag = 0;
                    disconnect();
                }
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



