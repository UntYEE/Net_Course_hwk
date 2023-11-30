#include "client.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>
#define SERVER_IP "127.0.0.1"  // 服务器的IP地址
#define SERVER_PORT 3999
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
#define WINDOW_SIZE 20
#define PACKET_LENGTH 1024
#define MAX_TIME 2 * CLOCKS_PER_SEC  // 超时重传
using namespace std;
SOCKADDR_IN socketAddr;  // 服务器地址
SOCKET socketClient;  // 客户端套接字
default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // 自己设置丢包
stringstream ss;
SYSTEMTIME sysTime = { 0 };
int len = sizeof(SOCKADDR);
int packetNum;  // 发送数据包的数量
int fileSize;  // 文件大小
int sendSeq;  // 发送数据包序列号
int err;  // socket错误提示
char* filePath;
char* fileName;
char* fileBuffer;

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

void initSocket() {
	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // 版本 2.2 
	if (err != NO_ERROR)
	{
		cout << "WSAStartup failed with error: " << err << endl;
		return;
	}
	socketClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socketAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP);
	socketAddr.sin_family = AF_INET;
	socketAddr.sin_port = htons(SERVER_PORT);

	printTime();
	cout << "客户端初始化成功，准备与服务器建立连接" << endl;
}

void connect() {
	int state = 0;  // 标识目前握手的状态
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode); // 设置成阻塞模式等待ACK响应

	while (flag) {
		switch (state) {
		case 0:  // 发送SYN=1数据包状态
			printTime();
			cout << "开始第一次握手，向服务器发送SYN=1的数据包..." << endl;

			// 第一次握手，向服务器发送SYN=1的数据包，服务器收到后会回应SYN, ACK=1的数据包
			sendPkt->setSYN();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
			state = 1;  // 转状态1
			break;

		case 1:  // 等待服务器回复
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if ((recvPkt->FLAG & 0x2) && (recvPkt->FLAG & 0x4)) {  // SYN=1, ACK=1
					printTime();
					cout << "开始第三次握手，向服务器发送ACK=1的数据包..." << endl;

					// 第三次握手，向服务器发送ACK=1的数据包，告知服务器自己接收能力正常
					sendPkt->setACK();
					sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
					state = 2;  // 转状态2
				}
				else {
					printTime();
					cout << "第二次握手阶段收到的数据包有误，重新开始第一次握手..." << endl;
					state = 0;  // 转状态0
				}
			}
			break;

		case 2: // 三次握手结束状态
			printTime();
			cout << "三次握手结束，确认已建立连接，开始文件传输..." << endl;
			cout << endl << "**************************************************************************************************" << endl << endl;
			flag = 0;
			break;
		}
	}
}

void disconnect() {  
	int state = 0;  // 标识目前挥手的状态	
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode); // 设置成阻塞模式等待ACK响应

	while (flag) {
		switch (state) {
		case 0:  // 发送FIN=1数据包状态
			printTime();
			cout << "开始第一次挥手，向服务器发送FIN=1的数据包..." << endl;

			// 第一次挥手，向服务器发送FIN=1的数据包
			sendPkt->setFIN();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

			state = 1;  // 转状态1
			break;

		case 1:  // FIN_WAIT_1
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if (recvPkt->FLAG & 0x4) {  // ACK=1
					printTime();
					cout << "收到了来自服务器第二次挥手ACK数据包..." << endl;

					state = 2;  // 转状态2，等待服务器第三次挥手
				}
				else {
					printTime();
					cout << "第二次挥手阶段收到的数据包有误，重新开始第二次挥手..." << endl;
				}
			}
			break;

		case 2:  // FIN_WAIT_2
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if ((recvPkt->FLAG & 0x1) && (recvPkt->FLAG & 0x4)) {  // ACK=1, FIN=1
					printTime();
					cout << "收到了来自服务器第三次挥手FIN&ACK数据包，开始第四次挥手，向服务器发送ACK=1的数据包..." << endl;

					// 第四次挥手，向服务器发送ACK=1的数据包，通知服务器确认断开连接
					sendPkt->setFINACK();
					sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
					state = 3;  // 转状态3
				}
				else {
					printTime();
					cout << "第三次挥手阶段收到的数据包有误，重新开始第三次挥手..." << endl;
				}
			}
			break;

		case 3:  // TIME_WAIT
			printTime();
			Sleep(2000);
			cout << "模拟TIME_WAIT" << endl;
			state = 4;
			break;

		case 4:  // 四次挥手结束状态
			printTime();
			cout << "四次挥手结束，确认已断开连接..." << endl;
			flag = 0;
			break;
		}
	}
}
//发送单个包的函数
void sendPacket(Packet* sendPkt) {
	Packet* recvPkt = new Packet;

	// 检查一下文件的各个内容
	printSndPktMsg(sendPkt);
	err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {
		cout << "sendto failed with error: " << WSAGetLastError() << endl;
	}
	clock_t start = clock();  // 记录发送时间，超时重传

	// 等待接收ACK信息，验证acknowledge number
	while (true) {
		while (recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len) <= 0)//接收出错 
		{
			if (clock() - start > MAX_TIME) {
				printTime();
				cout << "TIME OUT! Resend this data packet" << endl;
				//超时重传
				err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
				if (err == SOCKET_ERROR) {
					cout << "sendto failed with error: " << WSAGetLastError() << endl;
				}
				start = clock(); // 重设开始时间
			}
		}

		// 满足flag中ack=1且ack=seq则视为发送成功
		if ((recvPkt->FLAG & 0x4) && (recvPkt->ack == sendPkt->seq)) {
			sendSeq++;
			break;
		}
	}

}

void readFile() {
	ifstream f(filePath, ifstream::in | ios::binary);  // 以二进制方式打开文件

	if (!f.is_open()) {
		cout << "文件无法打开！" << endl;
		return;
	}

	f.seekg(0, std::ios_base::end);  // 将文件流指针定位到流的末尾以获取文件的大小
	fileSize = f.tellg();  // 获取文件大小

	// 计算数据包的数量，PACKET_LENGTH 为每个数据包的长度
	packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;

	cout << "文件大小为 " << fileSize << " Bytes，总共要发送 " << packetNum + 1 << " 个数据包" << endl << endl;
	//如果取模的结果非零（即存在剩余字节），则取整数商再加 1，否则取整数商

	f.seekg(0, std::ios_base::beg);  // 将文件流指针重新定位到流的开始

	fileBuffer = new char[fileSize];  // 创建足够大的缓冲区来存储整个文件

	f.read(fileBuffer, fileSize);  // 读取文件内容到缓冲区
	f.close();  // 关闭文件
}

void sendFile() {
	sendSeq = 0;
	clock_t start = clock();

	// 创建一个记录文件名的数据包（HEAD标志位为1）表示开始文件传输
	Packet* headPkt = new Packet;
	printTime();
	cout << "发送文件头数据包..." << endl;
	headPkt->setHEAD(sendSeq, fileSize, fileName);
	headPkt->checksum = checksum((uint32_t*)headPkt);
	sendPacket(headPkt);

	// 开始发送装载文件的数据包
	printTime();
	cout << "开始发送文件数据包..." << endl;
	Packet* sendPkt = new Packet;
	for (int i = 0; i < packetNum; i++) {
		
		if (i == packetNum - 1) // 最后一个包
		{  
			sendPkt->fillData(sendSeq, fileSize - i * PACKET_LENGTH, fileBuffer + i * PACKET_LENGTH);
			sendPkt->checksum = checksum((uint32_t*)sendPkt);
		}
		else {
			
			sendPkt->fillData(sendSeq, PACKET_LENGTH, fileBuffer + i * PACKET_LENGTH);
			sendPkt->checksum = checksum((uint32_t*)sendPkt);
		}
		
		sendPacket(sendPkt);
		Sleep(5);//设置延时
	}

	clock_t end = clock();
	printTime();
	cout << "文件发送完毕，传输时间为：" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
	cout << "吞吐率为：" << ((float)fileSize) / ((end - start) / CLOCKS_PER_SEC) << " Bytes/s " << endl << endl;
	cout << endl << "**************************************************************************************************" << endl << endl;
}

int main() {
	// 初始化客户端
	initSocket();

	// 与服务器端建立连接
	connect();

	// 读取文件
	cout << "请输入要传输的文件名(包括文件类型后缀)：";
	fileName = new char[128];
	cin >> fileName;
	cout << "请输入要传输的文件所在路径(绝对路径)：";
	filePath = new char[128];
	cin >> filePath;
	readFile();

	// 开始发送文件
	sendFile();

	// 主动断开连接
	disconnect();

	// 断连完成后，关闭socket
	err = closesocket(socketClient);
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