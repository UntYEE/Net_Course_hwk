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
#define SERVER_IP "127.0.0.1"  // ������IP��ַ 
#define SERVER_PORT 3999 // �������˿ں�
#define PKT_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // ��������С
#define WINDOW_SIZE 16  // �������ڴ�С
#define _CRT_SECURE_NO_WARNINGS
#define DATA_AREA_SIZE 1024
using namespace std;
SOCKADDR_IN ServerAddr;  // ��������ַ
SOCKADDR_IN ClientAddr;  // �ͻ��˵�ַ
SOCKET serverSocket;  // �������׽���
int fileSize;  // �ļ���С
unsigned int recvSize;  // �ۻ��յ����ļ�λ��
unsigned int recvBase;  // �ڴ��յ�����δ�յ�����С�������
char* fileName;  // �ļ���
char* fileBuffer;  // �����ļ�������
char** receiveBuffer;  // ���ջ�����
bool isCached[WINDOW_SIZE];  // ��ʶ�����ڵķ�����û�б�����
default_random_engine randomEngine;
uniform_real_distribution<float> randomNumber(0.0, 1.0);  // �Լ����ö���

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
    cout << "��ǰ���մ���: [" << recvBase << ", " << recvBase + WINDOW_SIZE - 1 << "]";
}


void InitializeWinsock() 
{   //��������socket��ʼ��
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
    ioctlsocket(serverSocket, FIONBIO, &mode); // ���÷�����ģʽ���ȴ����Կͻ��˵����ݰ�
    ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//���������ַ
    ServerAddr.sin_family = AF_INET;//ipv4
    ServerAddr.sin_port = htons(SERVER_PORT);//�������˿ں�
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
        cout << "[Server]:�����������ɹ����ȴ��ͻ��˽�������" << endl;
    }

}
void connect() 
{
    //ģ��TCP��������
    int state = 0;  // ���ֵ�״̬
    int err = 0; //����������Ϣ����
    int flag = 1;
    int len = sizeof(SOCKADDR);
    Packet* sendPkt = new Packet;
    Packet* recvPkt = new Packet;

    while (flag) {
        switch (state) {
        case 0:  // �ȴ��ͻ��˷�������
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr),&len);
            if (err > 0) 
            {
                if (recvPkt->FLAG & 0x2) {  // SYN=1
                    printTime();
                    cout << "[Server]:�յ����Կͻ��˵��������󣬿�ʼ�ڶ������֣���ͻ��˷���ACK,SYN=1�����ݰ�..." << endl;

                    // ����������ڶ������֣���ͻ��˷���ACK,SYN=1�����ݰ�
                    sendPkt->setSYNACK();
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    state = 1;  // ת״̬1
                }
                else {
                    printTime();
                    cout << "[Server]:��һ�����ֽ׶��յ������ݰ��������¿�ʼ��һ������..." << endl;
                }
            }
            break;

        case 1:  // ���տͻ��˵��������ֵ�ACK=1���ݰ�
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                if (recvPkt->FLAG & 0x4) {  // ACK=1
                    printTime();
                    cout << "[Server]:�յ����Կͻ��˵���������ACK���ݰ�..." << endl;

                    state = 3;  // ת״̬2
                }
                else {
                    printTime();
                    cout << "[Server]:���������ֽ׶��յ������ݰ��������·��͵ڶ������ֵ����ݰ�..." << endl;
                    state = 2;  // ת״̬���·��͵ڶ������ֵ����ݰ�
                }
            }
            break;

        case 2:  // ���·��͵ڶ������ֵ����ݰ�״̬
            printTime();
            cout << "[Server]:���·��͵ڶ������ֵ����ݰ�..." << endl;

            // ���·��͵ڶ������֣���ͻ��˷���ACK,SYN=1�����ݰ�
            sendPkt->setSYNACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 1;  // ת״̬1���ȴ��ͻ��˵�ACK=1���ݰ�
            break;

        case 3: // �������ֽ���״̬
            printTime();
            cout << "[Server]:�������ֽ�����ȷ���ѽ������ӣ���ʼ�ļ�����..." << endl;
            cout << endl << "**************************************************************************************************" << endl << endl;
            flag = 0;
            break;
        }
    }
}
void disconnect() 
{  
    int state = 0;  // ���ֵ�״̬
    bool flag = 1;
    int err = 0;
    Packet* sendPkt = new Packet;
    Packet* recvPkt = new Packet;
    int len = sizeof(SOCKADDR);
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode); // ���óɷ�����ģʽ

    while (flag) {
        switch (state) {
        case 0:  // CLOSE_WAIT_1
            printTime();
            cout << "���յ��ͻ��˵ĶϿ��������󣬿�ʼ�ڶ��λ��֣���ͻ��˷���ACK=1�����ݰ�..." << endl;

            // �ڶ��λ��֣���ͻ��˷���ACK=1�����ݰ�
            sendPkt->setACK(1);
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 1;  // ת״̬1
            break;

        case 1:  // CLOSE_WAIT_2
            printTime();
            cout << "��ʼ�����λ��֣���ͻ��˷���FIN,ACK=1�����ݰ�..." << endl;

            // �����λ��֣���ͻ��˷���FIN,ACK=1�����ݰ�
            sendPkt->setFINACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));

            state = 2;  // ת״̬2���ȴ��ͻ��˵��Ĵλ���
            break;

        case 2:  // LAST_ACK
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                if (recvPkt->FLAG & 0x4) {  // ACK=1
                    printTime();
                    cout << "�յ������Կͻ��˵��Ĵλ��ֵ�ACK���ݰ�..." << endl;

                    state = 3;  // ת״̬3
                }
                else {
                    printTime();
                    cout << "���Ĵλ��ֽ׶��յ������ݰ��������¿�ʼ���Ĵλ���..." << endl;
                }
            }
            break;

        case 3:  // CLOSE
            printTime();
            cout << "�Ĵλ��ֽ�����ȷ���ѶϿ�����..." << endl;
            flag = 0;
            break;
        }
    }
}
void saveFile() {
    string filePath = "C://Users//Betty//Desktop//rcv//";  // ����·��

    
    //����·������ļ���
    for (int i = 0; fileName[i]; i++) {
        filePath += fileName[i];
    }
    

    // ������Ϣ������ļ������ļ���С
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
    //int nextSeqNum = 0; // ��һ��Ҫ���͵����
    int expectedSeqNum = 0; // �����յ�����һ�����
    int lastAckedSeqNum = -1; // ���һ��ȷ�ϵ����
    Packet* recvPkt = new Packet;
    Packet* sendPkt = new Packet;
    int err = 0;
    int flag = 1;
    int len = sizeof(SOCKADDR);
    int packetNum = 0;
    float total = 0;  // �ܽ��ܵİ�
    float loss = 0;   // ����
    recvSize = 0;
    recvBase = 0;//�ڴ��յ�����δ�յ�����С�������
    receiveBuffer = new char* [WINDOW_SIZE];
    for (int i = 0; i < WINDOW_SIZE; i++)receiveBuffer[i] = new char[PKT_LENGTH];
    //����head��
    while (flag) 
    {
        // �ȴ�����HEAD״̬
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {

                if (isCorrupt(recvPkt)) {  // �������ݰ���
                    printTime();
                    cout << "�յ������ݰ�" << endl;
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
                    cout << "�յ��ļ�ͷ���ݰ����ļ���: " << fileName;
                    cout << ", �ļ���С: " << fileSize << " Bytes���ܹ���Ҫ���� " << packetNum << " �����ݰ�";
                    cout << ", �ȴ������ļ����ݰ�..." << endl << endl;

                    expectedSeqNum++;
                    flag = 0;
                    //state = 2;
                }
                else {
                    printTime();
                    cout << "���ļ�ͷ���ݰ����ȴ����Ͷ��ش�..." << endl;
                }
            }
        
    }      
    // ѭ�������ļ����ݰ�
    while (recvBase < packetNum)
    {
        err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
        if (err > 0) 
        {
            if (isCorrupt(recvPkt)) 
            {  // �������ݰ���
                printTime();
                cout << "�յ�һ���𻵵����ݰ��������κδ���" << endl << endl;
            }
            printRcvPktMsg(recvPkt);
            printTime();
            cout << "�յ��� " << recvPkt->seq << " �����ݰ�" << endl;
            if (randomNumber(randomEngine) >= 0.05)
            {  // �������ж�������
                unsigned int seq = recvPkt->seq;
                if (seq >= recvBase && seq < recvBase + WINDOW_SIZE)
                {   // �����ڵķ��鱻����
                    //����ack
                    Packet* sendPkt = new Packet;
                    unsigned int ack = recvPkt->seq;
                    sendPkt->setACK(ack);
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    if (!isCached[seq - recvBase])
                    {  
                        cout << "�״��յ������ݰ������仺��  ";
                        memcpy(receiveBuffer[seq - recvBase], recvPkt->data, PKT_LENGTH);
                        isCached[seq - recvBase] = 1;
                    }
                    else {
                        cout << "�Ѿ��յ��������ݰ�...";
                    }

                    if (seq == recvBase) 
                    {   // �÷�����ŵ��ڻ����
                        for (int i = 0; i < WINDOW_SIZE; i++) 
                        {  // �����ƶ�
                            if (!isCached[i]) break;  // ����û�л���ģ�ͣ����
                            recvBase++;
                        }
                        int offset = recvBase - seq;
                        cout << "�����ݰ���ŵ���Ŀǰ���ջ���ţ������ƶ� " << offset << " ����λ  ";
                        for (int i = 0; i < offset; i++) 
                        {  // ���齻���ϲ�
                            memcpy(fileBuffer + (seq + i) * PKT_LENGTH, receiveBuffer[i], recvPkt->len);
                        }
                        for (int i = 0; i < WINDOW_SIZE - offset; i++) 
                        {
                            //ͬ��ƽ��
                            isCached[i] = isCached[i + offset];  
                            isCached[i + offset] = 0;
                            memcpy(receiveBuffer[i], receiveBuffer[i + offset], PKT_LENGTH);
                        }
                        for (int i = WINDOW_SIZE - offset; i < WINDOW_SIZE; i++) 
                        {
                            isCached[i] = 0; //�����Ѵ�������ݰ����
                        }
                    }
                    printWindow();
                    cout << endl;
                }
                else if (seq >= recvBase - WINDOW_SIZE && seq < recvBase)
                {
                    printWindow();
                    cout << "  �����ݰ��������к���["<< recvBase - WINDOW_SIZE <<","<< recvBase - 1<<"]������ACK�Ҳ�������������..." << endl;
                    unsigned int ack = recvPkt->seq;
                    sendPkt->setACK(ack);
                    sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
                    // ���ܵ��������Χ�ڵķ�������ACK��������������
                }
                else {
                    printWindow();
                    cout << "  ������� " << seq << " ������ȷ��Χ�������κβ���" << endl;
                }
            }
            else {
                loss++;
                cout << "��������" << endl;
            }
        }
    }

    // ����ѭ��˵���ļ��������
    printTime();
    cout << "�ļ��������..." << endl;
    float lossRate = (loss / packetNum) * 100;
    cout << "������: " << lossRate << "%" << endl;
    cout << endl << "**************************************************************************************************" << endl << endl;

    // �����ļ�
    saveFile();

    // �ļ�������ϣ��ȴ����Ͷ˷��Ͷ�������
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
    // ��ʼ��������socket
    InitializeWinsock();

    // ��������
    connect();

    // ��ʼ�����ļ�
    receiveFile();

    // ������ɺ󣬹ر�socket
    int err = closesocket(serverSocket);
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



