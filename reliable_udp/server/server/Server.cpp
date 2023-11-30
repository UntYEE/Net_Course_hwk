#include "server.h"
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <winsock2.h>
#include <ws2tcpip.h>

#define SERVER_IP "127.0.0.1"  // ������IP��ַ 
#define SERVER_PORT 3999 // �������˿ں�
#define PKT_LENGTH 1024
#define BUFFER_SIZE sizeof(Packet)  // ��������С
#define _CRT_SECURE_NO_WARNINGS
using namespace std;
SOCKADDR_IN ServerAddr;  // ��������ַ
SOCKADDR_IN ClientAddr;  // �ͻ��˵�ַ
SOCKET serverSocket;  // �������׽���
char* fileName;
char* fileBuffer;
int fileSize;  // �ļ���С
unsigned int recvSize;  // �ۻ��յ����ļ�λ��
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
            sendPkt->setACK();
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
    string filePath = "C:\\Users\\lenovo\\Desktop\\rcv\\";  // ����·��

    
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

void receiveFile() {
    int state = 0;
    bool flag = 1;
    int waitSeq = 0;//�������к�

    Packet* recvPkt = new Packet;
    Packet* sendPkt = new Packet;
    int err = 0;
    int len = sizeof(SOCKADDR);
   
    int packetNum = 0;
    float total = 0;//�ܽ��ܵİ�
    float loss = 0;//����
    // ѭ�������ļ����ݰ�
    while (flag) {
        switch (state) {
        case 0:  // �ȴ�����HEAD״̬
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr),&len);
            if (err > 0) {
                printRcvPktMsg(recvPkt);
                if (err > 0) {
                    if (isCorrupt(recvPkt)) {  // �������ݰ���
                        printTime();
                        cout << "�յ��𻵵����ݰ������Ͷ˷��� ack=" << waitSeq << " ���ݰ�" << endl;

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
                        cout << "�յ����Է��Ͷ˵��ļ�ͷ���ݰ����ļ���Ϊ: " << fileName;
                        cout << "���ļ���СΪ: " << fileSize << " Bytes���ܹ���Ҫ���� " << packetNum << " �����ݰ�";
                        cout << "���ȴ������ļ����ݰ�..." << endl << endl;

                        waitSeq++;
                        state = 2;
                    }
                    else {
                        printTime();
                        cout << "�յ������ݰ������ļ�ͷ���ȴ����Ͷ��ش�..." << endl;
                    }
                }
            }
            break;

        case 1:  // �ȴ��ļ����ݰ�״̬
            err = recvfrom(serverSocket, (char*)recvPkt, BUFFER_SIZE, 0, (SOCKADDR*)&(ServerAddr), &len);
            if (err > 0) {
                printRcvPktMsg(recvPkt);
                if (isCorrupt(recvPkt)) {  // �������ݰ���
                    printTime();
                    cout << "�յ�һ���𻵵����ݰ������Ͷ˷��� ack=" << waitSeq << " ���ݰ�" << endl << endl;
                    state = 2;//�յ�����֮ǰ��seq
                    break;
                }
                if (recvPkt->seq == waitSeq) {  // �յ���Ŀǰ�ȴ��İ�
                    if (randomNumber(randomEngine) >= 0.05) //���ö�������0.1
                    {
                        memcpy(fileBuffer + recvSize, recvPkt->data, recvPkt->len);
                        recvSize += recvPkt->len;

                        printTime();
                        cout << "�յ��� " << recvPkt->seq << " �����ݰ������Ͷ˷��� ack=" << waitSeq << endl << endl;
                        total++;
                        waitSeq++;//�յ����ǵ�ǰ��seq
                        state = 2;
                    }
                    else {
                        loss++;
                        total++;
                        cout << "��������" << endl;
                       // state = 2;
                        //break;
                    }
                }
                else {  // ����Ŀǰ�ȴ������ݰ�
                    printTime();
                    cout << "�յ��� " << recvPkt->seq << " �����ݰ�������������Ҫ�����ݰ������Ͷ˷��� ack=" << waitSeq << endl << endl;
                }
            }

        case 2:  // ����ACK
            sendPkt->ack = waitSeq - 1;
            sendPkt->setACK();
            sendto(serverSocket, (char*)sendPkt, BUFFER_SIZE, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
            state = 1;
            if (recvSize == fileSize) {  // �ļ��������
                printTime();
                cout << "�ļ��������..." << endl;
                cout << "�����ʣ�  " << float(loss / total)*100<<"%" << endl;
                cout << endl << "**************************************************************************************************" << endl << endl;

                // �����ļ�
                saveFile();

                state = 3;
            }
            break;

        case 3:  // �ļ��ѽ�����ϣ��ȴ����Ͷ˷��Ͷ�������
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



