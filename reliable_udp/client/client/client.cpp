#include "client.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <random>
#define RESET   "\033[0m"
#define GREEN   "\033[1;32m"
#define BLUE    "\033[1;34m"
#define SERVER_IP "127.0.0.1"  // 服务器的IP地址
#define SERVER_PORT 3999
#define PACKET_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // 缓冲区大小
#define TIME_OUT 0.2 * CLOCKS_PER_SEC  // 超时重传
#define WINDOW_SIZE 16  // 滑动窗口大小
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
unsigned int sendBase;  // 窗口基序号，指向已发送还未被确认的最小分组序号
unsigned int nextSeqNum;  // 指向下一个可用但还未发送的分组序号
int timerID[WINDOW_SIZE];//
char* filePath;  // 文件(绝对)路径
char* fileName;  // 文件名
char* fileBuffer;  
char** selectiveRepeatBuffer;  // 选择重传缓冲区

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
	cout << "  当前发送窗口: [" << sendBase << ", " << sendBase + WINDOW_SIZE - 1 << "]";
	cout << endl;
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

void sendPacket(Packet* sendPkt) 
{  
	cout << "发送第 " << sendPkt->seq << " 号数据包";
	printWindow();
	printSndPktMsg(sendPkt);  
	cout << endl;
	err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {
		cout << "Send Packet failed with ERROR: " << WSAGetLastError() << endl;
	}
}

void resendPacket(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) 
{   // 重传函数
	// cout << endl << "resend" << " Timer ID " << idEvent << endl << endl;
	unsigned int seq = 0;
	for (int i = 0; i < WINDOW_SIZE; i++) 
	{  // 找到是哪个Timer超时了
		if (timerID[i] == idEvent && timerID[i] != 0) 
		{
			seq = i + sendBase;
			break;
		}
	}
	cout << "No." << seq << " 数据包对应的计时器超时，重新发送" << endl;

	Packet* resendPkt = new Packet;
	memcpy(resendPkt, selectiveRepeatBuffer[seq - sendBase], sizeof(Packet));  // 从缓冲区直接取出来
	sendPacket(resendPkt);
	printSndPktMsg(resendPkt);
	cout << endl;
	
}

void ackHandler(unsigned int ack) 
{   
	// cout << endl << "ack " << ack << endl << endl;
	if (ack >= sendBase && ack < sendBase + WINDOW_SIZE) // 如果ack在窗口内
	{  
		KillTimer(NULL, timerID[ack - sendBase]);//取消对应的定时器
		timerID[ack - sendBase] = 0;  // timerID置零

		if (ack == sendBase) 
		{   // 如果 ack = sendBase，那么sendBase移动到具有最小序号的未确认分组处
			for (int i = 0; i < WINDOW_SIZE; i++) 
			{
				if (timerID[i]) break;  // 遇到有计时器的停下来
				sendBase++;  // sendBase后移
			}
			int offset = sendBase - ack;
			for (int i = 0; i < WINDOW_SIZE - offset; i++) 
			{
				//统一进行平移操作
				timerID[i] = timerID[i + offset];  
				timerID[i + offset] = 0;
				memcpy(selectiveRepeatBuffer[i], selectiveRepeatBuffer[i + offset], sizeof(Packet));  
			}
			for (int i = WINDOW_SIZE - offset; i < WINDOW_SIZE; i++) {
				timerID[i] = 0;  // 清除多余的计时器
			}
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

void sendFile() 
{
	sendBase = 0;//窗口基序号为0
	nextSeqNum = 0;//下一个可用但还未发送的分组序号
	selectiveRepeatBuffer = new char* [WINDOW_SIZE];
	for (int i = 0; i < WINDOW_SIZE; i++)selectiveRepeatBuffer[i] = new char[sizeof(Packet)];  // 选择重传缓冲区初始化
	clock_t start = clock();

	// 发送记录文件信息的HEAD数据包
	Packet* headPkt = new Packet;
	printTime();
	cout << "发送文件头数据包..." << endl;
	headPkt->setHEAD(0, fileSize, fileName);
	headPkt->checksum = checksum((uint32_t*)headPkt);
	sendPacket(headPkt); 

	// 开始发送装载文件的数据包
	printTime();
	cout << "开始发送文件数据包..." << endl;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	while (sendBase < packetNum) 
	{
		while (nextSeqNum < sendBase + WINDOW_SIZE && nextSeqNum < packetNum) 
		{  //还在窗口内持续发送数据包
			if (nextSeqNum == packetNum - 1) 
			{  // 如果是最后一个包，处理包的大小
				sendPkt->fillData(nextSeqNum, fileSize - nextSeqNum * PACKET_LENGTH, fileBuffer + nextSeqNum * PACKET_LENGTH);
				sendPkt->checksum = checksum((uint32_t*)sendPkt);
			}
			else 
			{  
				sendPkt->fillData(nextSeqNum, PACKET_LENGTH, fileBuffer + nextSeqNum * PACKET_LENGTH);
				sendPkt->checksum = checksum((uint32_t*)sendPkt);
			}
			memcpy(selectiveRepeatBuffer[nextSeqNum - sendBase], sendPkt, sizeof(Packet));  // 已经发送的包存入缓冲区
			sendPacket(sendPkt);
			timerID[nextSeqNum - sendBase] = SetTimer(NULL, 0, TIME_OUT, (TIMERPROC)resendPacket);  
			// 用于监控当前发送的数据包是否在规定时间内得到了确认，没有则触发resendPacket
			nextSeqNum++;
			Sleep(10);
		}

		// 当前发送窗口已经用完则进入接收ACK阶段
		err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
		if (err > 0) 
		{
			printRcvPktMsg(recvPkt);
			ackHandler(recvPkt->ack);  // 处理ack
		}
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{  // 以查看的方式从系统中获取消息，可以不将消息从系统中移除，是非阻塞函数；当系统无消息时，返回FALSE，继续执行后续代码。
			if (msg.message == WM_TIMER) {  // 定时器消息
				DispatchMessage(&msg);
			}
		}
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