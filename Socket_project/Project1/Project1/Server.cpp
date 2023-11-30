#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include <cstring>
#include <cstdlib>
#include <Winsock2.h>
#include <WS2tcpip.h> // ʹ��Winsock2��
#include <thread>
#include <vector>
using namespace std;
//ʹ�õ�ȫ�ֱ���
#pragma comment(lib,"ws2_32.lib")   //socket��

const int Buffer_Len = 1024;//��������С
const int Wait_Time = 10;//ÿ���ͻ��˵ȴ��¼���ʱ�䣬��λ����
const int Max_Clients = 128;//��������������
int total = 0;//��ǰ�Ѿ����ӵĿͷ��˷�����
const int Max_Username_Length = 20;//�û�����󳤶�Ϊ20

SOCKET ClientSock[Max_Clients];//�ͻ����׽���
SOCKADDR_IN ClientAddr[Max_Clients];//�ͻ��˵�ַ
WSAEVENT ClientEvent[Max_Clients];//�ͻ����¼�

string ClientUsername[Max_Clients];//�ͻ����û���
int ClientEnterIndex[Max_Clients];//����˳���൱��ID

DWORD WINAPI Server_Thread_Init(void*lp) //���������߳�
{
	//���̳߳�ʼ��
	SOCKET servSock = *(SOCKET*)lp;
	while (true) 
	{   //���̱߳��ֳ�פ����״̬
		for (int i = 0; i < total + 1; i++)
		{
			//��ÿһ���նˣ��ͻ��˺ͷ���ˣ����鿴�Ƿ����¼����ȴ�Wait_Time����
			int index = WSAWaitForMultipleEvents(1, &ClientEvent[i], false, Wait_Time, 0);

			index -= WSA_WAIT_EVENT_0;//indexΪ�����¼�ʵ������

			if (index == WSA_WAIT_TIMEOUT || index == WSA_WAIT_FAILED)
			{
				continue;//���������߳�ʱ�����������ն�
			}

			else if (index == 0)//�׽���ע�������¼��ɹ�����������0
			{
				WSANETWORKEVENTS NetworkEvents;//�����¼�����
				WSAEnumNetworkEvents(ClientSock[i], ClientEvent[i], &NetworkEvents);//��⵱ǰ�ͻ����׽��ֵ������¼�
				//�����¼����ͻ������ӣ��˳����յ���Ϣ
				if (NetworkEvents.lNetworkEvents & FD_ACCEPT)//�пͻ�������
				{
					if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
					{
						cout << "Accept Error: " << NetworkEvents.iErrorCode[FD_ACCEPT_BIT] << endl;
						continue;
					}

					if (total + 1 < Max_Clients)//��һ���ͻ��˿��Լ���
					{
						int New_Index = total + 1;
						int len = sizeof(SOCKADDR);
						//�������Կͻ��˵��������󣬲�����һ���µ��׽���
						SOCKET NewSock = accept(servSock, (SOCKADDR*)&ClientAddr[New_Index], &len);

						if (NewSock != INVALID_SOCKET)
						{
							ClientSock[New_Index] = NewSock;
							WSAEVENT newEvent = WSACreateEvent();//�����¿ͻ����׽��֣����¼�����
							WSAEventSelect(ClientSock[New_Index], newEvent, FD_CLOSE | FD_READ | FD_WRITE);//�����׽�����Ҫ���ӵ��¼�
							ClientEvent[New_Index] = newEvent;
							ClientEnterIndex[New_Index] = New_Index+1000;
							total++;
							//���������½���ͻ������ѣ�
							char ip[24] = { 0 };
							cout << "*" << " Customer " << ClientEnterIndex[New_Index] << " (IP: " 
								<< inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[New_Index].sin_addr, ip, sizeof(ip)) << ") enters room, there are " << total << " customers." << endl;

							// ���ÿͻ��˷��ͻ�ӭ����
							char welcomeMessage[] = "#################################################\n"
								"#                                               #\n"
								"#         Welcome to Chatland!                  #\n"
								"#   Enter 'exit' or 'quit' to leave the room.   #\n"
								"#   Enter 'rename' to change your current name. #\n"
								"#                                               #\n"
								"#################################################\n";
							send(ClientSock[New_Index], welcomeMessage, sizeof(welcomeMessage), 0);

							// �㲥
							char buf[Buffer_Len] = "Chatland:Welcome customer: (IP: ";
							strcat_s(buf, inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[New_Index].sin_addr, ip, sizeof(ip)));
							strcat_s(buf, ") to join us.");

							for (int j = 1; j <= total; j++)
							{
								if (j == New_Index)
									continue; // �����¼��������
								send(ClientSock[j], buf, sizeof(buf), 0);
							}
						}
					}
					else
					{
						// ����ʱ�Ĵ�����Ϣ
						perror("accept");
						exit(0);
					}
				}
				else if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				//�ͻ��˶Ͽ�����
				{
					
					char ip[24] = { 0 };
					//�������˿ͻ����뿪����
					cout << "*" <<  "Customer"<<i <<"��IP��" << inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[i].sin_addr, ip, sizeof(ip))
						<< ")leaves the room,there exits��" << --total << endl;
					//�ͷ���Դ
					closesocket(ClientSock[i]);
					WSACloseEvent(ClientEvent[i]);
					
					//�����пͻ��˷����˳������ҵ���Ϣ
					char leaveMessage[Buffer_Len];
					sprintf_s(leaveMessage, "Chatland: Customer %d (%s)��IP��%s��leaves.", i, ClientUsername[i].c_str(), ip);
					memset(ip, 0, sizeof(ip));
					//�ȵ�������
					for (int j = i; j < total+1; j++)
					{
						ClientSock[j] = ClientSock[j + 1];
						ClientEvent[j] = ClientEvent[j + 1];
						ClientAddr[j] = ClientAddr[j + 1];
						ClientUsername[j] = ClientUsername[j + 1];
						ClientEnterIndex[j] = ClientEnterIndex[j + 1];
					}
					for (int j = 1; j <= total; j++)
						send(ClientSock[j], leaveMessage, sizeof(leaveMessage), 0);//�㲥
					
				}
				else if (NetworkEvents.lNetworkEvents & FD_READ)//�пͻ�����Ϣ
				{
					char RecvBuffer[Buffer_Len] = { 0 }; // ��ʼ���ַ�������
					char SendBuffer[Buffer_Len] = { 0 };

					for (int j = 1; j <= total; j++)//��ÿһ���ͻ���
					{
						int nrecv = recv(ClientSock[j], RecvBuffer, sizeof(RecvBuffer), 0);
						if (nrecv > 0) // ������յ����ַ�������0
						{
							// ��ȡ�ͻ��˵�IP��ַ����������Ĳ���
							char clientIP[INET_ADDRSTRLEN];
							inet_ntop(AF_INET, &(ClientAddr[j].sin_addr), clientIP, INET_ADDRSTRLEN);
							if (ClientUsername[j].empty()) {
								
								// ����ͻ��˷��͵��û���
								ClientUsername[j] = RecvBuffer;

								// ��������չʾ������Ϣ
								cout << "*" << " New customer "<< ClientEnterIndex[j] << "(IP: "<< clientIP << ") named " << ClientUsername[j] << " is in the room.Total clients : " << total << endl;

								// �㲥��ӭ��Ϣ
								char welcomeMessage[Buffer_Len];
								sprintf_s(welcomeMessage, "Chatland: Welcome %s (IP: %s) to join us.", ClientUsername[j].c_str(), clientIP);
								for (int j = 1; j <= total; j++)
									send(ClientSock[j], welcomeMessage, sizeof(welcomeMessage), 0);
							}
							else {
								if (strncmp(RecvBuffer, "rename ",7) == 0) {
								
									// ����ͻ��˷��͵��û���
									string newUsername = &RecvBuffer[7];
									ClientUsername[j] = newUsername;

									// չʾ������Ϣ
									cout << "*" << "Customer" << ClientEnterIndex[j] <<"(IP: " << clientIP << ") is renamed as " << ClientUsername[j]<<"." << endl;

								}
								else {
									sprintf_s(SendBuffer, "[Client %d (%s) at %s]: %s", ClientEnterIndex[j], ClientUsername[j].c_str(), clientIP, RecvBuffer);
									cout << SendBuffer << endl;

									// ת���ͻ��˷�����Ϣ
									for (int k = 1; k <= total; k++) {
										send(ClientSock[k], SendBuffer, sizeof(SendBuffer), 0);
									}
								
								}
							}
						}

					
				}
			}
			}
		}
	}
	return 0;
}


int main() {

	WORD wVersion = MAKEWORD(2, 2);
	WSADATA wsadata;
	if (WSAStartup(wVersion, &wsadata) != 0)
	{
		perror("Wsa initation failed...");
		exit(1);
	}
	//���������õ��׽���
	//1��socket�����׽��֣����ؾ��
	SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);

	//����������һ������0����ȷ���׽���ʹ�õ�Э��غ�����ʱ�����������ֵ��Ϊ0	(flag==0)
	if (listener == -1) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		perror("Server listener does not work����");
		exit(0);//��������ʧ��
	}
	else printf("�����������ɹ�...\n");


	//2��bind/listen �󶨡������˿�
	//�˿ں���������Ӧ��
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;//Э���
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	server_addr.sin_port = htons(10000);//���ö˿ں�Ϊ10000
	int ret = bind(listener, (struct sockaddr*)&server_addr, sizeof(sockaddr_in));
	if (ret == -1)
	{
		perror("bind does not work...");
		exit(0);
	}

	WSAEVENT ServerEvent = WSACreateEvent();//ʹ��WSACreateEvent�����¼�����ServerEvent
	WSAEventSelect(listener, ServerEvent, FD_ALL_EVENTS);//ʹ��WSAEventSelect����������׽�������������¼����󣬲��Ҽ��������¼�

	ClientSock[0] = listener;
	ClientEvent[0] = ServerEvent;


	// 3. ���ü���
	ret = listen(listener, 128);//������������������
	if (ret == -1)
	{
		perror("listen does not work...");
		exit(0);
	}
	//4. �������̣߳��������߳�
	CloseHandle(CreateThread(NULL, 0, &Server_Thread_Init, (void*)&listener, 0, 0));

	cout << "------Chatland is open!------" << endl;

	//5. �����������ϵض�ȡ����
	while (true)
	{
		char ServerInput[Buffer_Len] = { 0 };
		char Send_Buf[Buffer_Len] = { 0 };

		// �ӷ���������̨��ȡ��Ϣ
		cin.getline(ServerInput, sizeof(ServerInput));

		// ��ʽ����������Ϣ
		sprintf_s(Send_Buf, "[Chatland]: %s", ServerInput);

		// �㲥��������Ϣ�����пͻ���
		for (int j = 1; j <= total; j++)
		{
			send(ClientSock[j], Send_Buf, sizeof(Send_Buf), 0);
		}
	}

	WSACleanup();
}


