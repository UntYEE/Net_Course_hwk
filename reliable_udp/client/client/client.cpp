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
#define SERVER_IP "127.0.0.1"  // ��������IP��ַ
#define SERVER_PORT 3999
#define PACKET_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // ��������С
#define TIME_OUT 0.2 * CLOCKS_PER_SEC  // ��ʱ�ش�
#define WINDOW_SIZE 16  // �������ڴ�С
using namespace std;
SOCKADDR_IN socketAddr;  // ��������ַ
SOCKET socketClient;  // �ͻ����׽���
default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // �Լ����ö���
stringstream ss;
SYSTEMTIME sysTime = { 0 };
int len = sizeof(SOCKADDR);
int packetNum;  // �������ݰ�������
int fileSize;  // �ļ���С
int sendSeq;  // �������ݰ����к�
int err;  // socket������ʾ
unsigned int sendBase;  // ���ڻ���ţ�ָ���ѷ��ͻ�δ��ȷ�ϵ���С�������
unsigned int nextSeqNum;  // ָ����һ�����õ���δ���͵ķ������
int timerID[WINDOW_SIZE];//
char* filePath;  // �ļ�(����)·��
char* fileName;  // �ļ���
char* fileBuffer;  
char** selectiveRepeatBuffer;  // ѡ���ش�������

void printTime() {
	// ��ȡ��ǰϵͳʱ���
	auto currentTime = std::chrono::system_clock::now();

	// ��ʱ���ת��Ϊʱ��ṹ��
	std::time_t time = std::chrono::system_clock::to_time_t(currentTime);
	std::tm tmInfo = *std::localtime(&time);

	// ��ʽ�����ʱ��
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
	cout << "  ��ǰ���ʹ���: [" << sendBase << ", " << sendBase + WINDOW_SIZE - 1 << "]";
	cout << endl;
}

void initSocket() {
	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);  // �汾 2.2 
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
	cout << "�ͻ��˳�ʼ���ɹ���׼�����������������" << endl;
}

void connect() {
	int state = 0;  // ��ʶĿǰ���ֵ�״̬
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode); // ���ó�����ģʽ�ȴ�ACK��Ӧ

	while (flag) {
		switch (state) {
		case 0:  // ����SYN=1���ݰ�״̬
			printTime();
			cout << "��ʼ��һ�����֣������������SYN=1�����ݰ�..." << endl;

			// ��һ�����֣������������SYN=1�����ݰ����������յ�����ӦSYN, ACK=1�����ݰ�
			sendPkt->setSYN();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
			state = 1;  // ת״̬1
			break;

		case 1:  // �ȴ��������ظ�
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if ((recvPkt->FLAG & 0x2) && (recvPkt->FLAG & 0x4)) {  // SYN=1, ACK=1
					printTime();
					cout << "��ʼ���������֣������������ACK=1�����ݰ�..." << endl;

					// ���������֣������������ACK=1�����ݰ�����֪�������Լ�������������
					sendPkt->setACK();
					sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
					state = 2;  // ת״̬2
				}
				else {
					printTime();
					cout << "�ڶ������ֽ׶��յ������ݰ��������¿�ʼ��һ������..." << endl;
					state = 0;  // ת״̬0
				}
			}
			break;

		case 2: // �������ֽ���״̬
			printTime();
			cout << "�������ֽ�����ȷ���ѽ������ӣ���ʼ�ļ�����..." << endl;
			cout << endl << "**************************************************************************************************" << endl << endl;
			flag = 0;
			break;
		}
	}
}

void disconnect() {  
	int state = 0;  // ��ʶĿǰ���ֵ�״̬	
	bool flag = 1;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;

	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode); // ���ó�����ģʽ�ȴ�ACK��Ӧ

	while (flag) {
		switch (state) {
		case 0:  // ����FIN=1���ݰ�״̬
			printTime();
			cout << "��ʼ��һ�λ��֣������������FIN=1�����ݰ�..." << endl;

			// ��һ�λ��֣������������FIN=1�����ݰ�
			sendPkt->setFIN();
			sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));

			state = 1;  // ת״̬1
			break;

		case 1:  // FIN_WAIT_1
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if (recvPkt->FLAG & 0x4) {  // ACK=1
					printTime();
					cout << "�յ������Է������ڶ��λ���ACK���ݰ�..." << endl;

					state = 2;  // ת״̬2���ȴ������������λ���
				}
				else {
					printTime();
					cout << "�ڶ��λ��ֽ׶��յ������ݰ��������¿�ʼ�ڶ��λ���..." << endl;
				}
			}
			break;

		case 2:  // FIN_WAIT_2
			err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
			if (err >= 0) {
				if ((recvPkt->FLAG & 0x1) && (recvPkt->FLAG & 0x4)) {  // ACK=1, FIN=1
					printTime();
					cout << "�յ������Է����������λ���FIN&ACK���ݰ�����ʼ���Ĵλ��֣������������ACK=1�����ݰ�..." << endl;

					// ���Ĵλ��֣������������ACK=1�����ݰ���֪ͨ������ȷ�϶Ͽ�����
					sendPkt->setFINACK();
					sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
					state = 3;  // ת״̬3
				}
				else {
					printTime();
					cout << "�����λ��ֽ׶��յ������ݰ��������¿�ʼ�����λ���..." << endl;
				}
			}
			break;

		case 3:  // TIME_WAIT
			printTime();
			Sleep(2000);
			cout << "ģ��TIME_WAIT" << endl;
			state = 4;
			break;

		case 4:  // �Ĵλ��ֽ���״̬
			printTime();
			cout << "�Ĵλ��ֽ�����ȷ���ѶϿ�����..." << endl;
			flag = 0;
			break;
		}
	}
}

void sendPacket(Packet* sendPkt) 
{  
	cout << "���͵� " << sendPkt->seq << " �����ݰ�";
	printWindow();
	printSndPktMsg(sendPkt);  
	cout << endl;
	err = sendto(socketClient, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&socketAddr, sizeof(SOCKADDR));
	if (err == SOCKET_ERROR) {
		cout << "Send Packet failed with ERROR: " << WSAGetLastError() << endl;
	}
}

void resendPacket(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) 
{   // �ش�����
	// cout << endl << "resend" << " Timer ID " << idEvent << endl << endl;
	unsigned int seq = 0;
	for (int i = 0; i < WINDOW_SIZE; i++) 
	{  // �ҵ����ĸ�Timer��ʱ��
		if (timerID[i] == idEvent && timerID[i] != 0) 
		{
			seq = i + sendBase;
			break;
		}
	}
	cout << "No." << seq << " ���ݰ���Ӧ�ļ�ʱ����ʱ�����·���" << endl;

	Packet* resendPkt = new Packet;
	memcpy(resendPkt, selectiveRepeatBuffer[seq - sendBase], sizeof(Packet));  // �ӻ�����ֱ��ȡ����
	sendPacket(resendPkt);
	printSndPktMsg(resendPkt);
	cout << endl;
	
}

void ackHandler(unsigned int ack) 
{   
	// cout << endl << "ack " << ack << endl << endl;
	if (ack >= sendBase && ack < sendBase + WINDOW_SIZE) // ���ack�ڴ�����
	{  
		KillTimer(NULL, timerID[ack - sendBase]);//ȡ����Ӧ�Ķ�ʱ��
		timerID[ack - sendBase] = 0;  // timerID����

		if (ack == sendBase) 
		{   // ��� ack = sendBase����ôsendBase�ƶ���������С��ŵ�δȷ�Ϸ��鴦
			for (int i = 0; i < WINDOW_SIZE; i++) 
			{
				if (timerID[i]) break;  // �����м�ʱ����ͣ����
				sendBase++;  // sendBase����
			}
			int offset = sendBase - ack;
			for (int i = 0; i < WINDOW_SIZE - offset; i++) 
			{
				//ͳһ����ƽ�Ʋ���
				timerID[i] = timerID[i + offset];  
				timerID[i + offset] = 0;
				memcpy(selectiveRepeatBuffer[i], selectiveRepeatBuffer[i + offset], sizeof(Packet));  
			}
			for (int i = WINDOW_SIZE - offset; i < WINDOW_SIZE; i++) {
				timerID[i] = 0;  // �������ļ�ʱ��
			}
		}
	}
}

void readFile() {
	ifstream f(filePath, ifstream::in | ios::binary);  // �Զ����Ʒ�ʽ���ļ�

	if (!f.is_open()) {
		cout << "�ļ��޷��򿪣�" << endl;
		return;
	}

	f.seekg(0, std::ios_base::end);  // ���ļ���ָ�붨λ������ĩβ�Ի�ȡ�ļ��Ĵ�С
	fileSize = f.tellg();  // ��ȡ�ļ���С

	// �������ݰ���������PACKET_LENGTH Ϊÿ�����ݰ��ĳ���
	packetNum = fileSize % PACKET_LENGTH ? fileSize / PACKET_LENGTH + 1 : fileSize / PACKET_LENGTH;

	cout << "�ļ���СΪ " << fileSize << " Bytes���ܹ�Ҫ���� " << packetNum + 1 << " �����ݰ�" << endl << endl;
	//���ȡģ�Ľ�����㣨������ʣ���ֽڣ�����ȡ�������ټ� 1������ȡ������

	f.seekg(0, std::ios_base::beg);  // ���ļ���ָ�����¶�λ�����Ŀ�ʼ

	fileBuffer = new char[fileSize];  // �����㹻��Ļ��������洢�����ļ�

	f.read(fileBuffer, fileSize);  // ��ȡ�ļ����ݵ�������
	f.close();  // �ر��ļ�
}

void sendFile() 
{
	sendBase = 0;//���ڻ����Ϊ0
	nextSeqNum = 0;//��һ�����õ���δ���͵ķ������
	selectiveRepeatBuffer = new char* [WINDOW_SIZE];
	for (int i = 0; i < WINDOW_SIZE; i++)selectiveRepeatBuffer[i] = new char[sizeof(Packet)];  // ѡ���ش���������ʼ��
	clock_t start = clock();

	// ���ͼ�¼�ļ���Ϣ��HEAD���ݰ�
	Packet* headPkt = new Packet;
	printTime();
	cout << "�����ļ�ͷ���ݰ�..." << endl;
	headPkt->setHEAD(0, fileSize, fileName);
	headPkt->checksum = checksum((uint32_t*)headPkt);
	sendPacket(headPkt); 

	// ��ʼ����װ���ļ������ݰ�
	printTime();
	cout << "��ʼ�����ļ����ݰ�..." << endl;
	Packet* sendPkt = new Packet;
	Packet* recvPkt = new Packet;
	while (sendBase < packetNum) 
	{
		while (nextSeqNum < sendBase + WINDOW_SIZE && nextSeqNum < packetNum) 
		{  //���ڴ����ڳ����������ݰ�
			if (nextSeqNum == packetNum - 1) 
			{  // ��������һ������������Ĵ�С
				sendPkt->fillData(nextSeqNum, fileSize - nextSeqNum * PACKET_LENGTH, fileBuffer + nextSeqNum * PACKET_LENGTH);
				sendPkt->checksum = checksum((uint32_t*)sendPkt);
			}
			else 
			{  
				sendPkt->fillData(nextSeqNum, PACKET_LENGTH, fileBuffer + nextSeqNum * PACKET_LENGTH);
				sendPkt->checksum = checksum((uint32_t*)sendPkt);
			}
			memcpy(selectiveRepeatBuffer[nextSeqNum - sendBase], sendPkt, sizeof(Packet));  // �Ѿ����͵İ����뻺����
			sendPacket(sendPkt);
			timerID[nextSeqNum - sendBase] = SetTimer(NULL, 0, TIME_OUT, (TIMERPROC)resendPacket);  
			// ���ڼ�ص�ǰ���͵����ݰ��Ƿ��ڹ涨ʱ���ڵõ���ȷ�ϣ�û���򴥷�resendPacket
			nextSeqNum++;
			Sleep(10);
		}

		// ��ǰ���ʹ����Ѿ�������������ACK�׶�
		err = recvfrom(socketClient, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(socketAddr), &len);
		if (err > 0) 
		{
			printRcvPktMsg(recvPkt);
			ackHandler(recvPkt->ack);  // ����ack
		}
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{  // �Բ鿴�ķ�ʽ��ϵͳ�л�ȡ��Ϣ�����Բ�����Ϣ��ϵͳ���Ƴ����Ƿ�������������ϵͳ����Ϣʱ������FALSE������ִ�к������롣
			if (msg.message == WM_TIMER) {  // ��ʱ����Ϣ
				DispatchMessage(&msg);
			}
		}
	}

	clock_t end = clock();
	printTime();
	cout << "�ļ�������ϣ�����ʱ��Ϊ��" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
	cout << "������Ϊ��" << ((float)fileSize) / ((end - start) / CLOCKS_PER_SEC) << " Bytes/s " << endl << endl;
	cout << endl << "**************************************************************************************************" << endl << endl;
}


int main() {
	// ��ʼ���ͻ���
	initSocket();

	// ��������˽�������
	connect();

	// ��ȡ�ļ�
	cout << "������Ҫ������ļ���(�����ļ����ͺ�׺)��";
	fileName = new char[128];
	cin >> fileName;
	cout << "������Ҫ������ļ�����·��(����·��)��";
	filePath = new char[128];
	cin >> filePath;
	readFile();

	// ��ʼ�����ļ�
	sendFile();

	// �����Ͽ�����
	disconnect();

	// ������ɺ󣬹ر�socket
	err = closesocket(socketClient);
	if (err == SOCKET_ERROR) {
		cout << "Close socket failed with error: " << WSAGetLastError() << endl;
		return 1;
	}

	// �����˳�
	printTime();
	cout << "�����˳�..." << endl;
	WSACleanup();
	return 0;
}