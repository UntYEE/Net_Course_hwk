#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<iostream>
#include <Winsock2.h>
#include <WS2tcpip.h> // ʹ��Winsock2��
#include<cstring>

using namespace std;

#pragma comment(lib,"ws2_32.lib")   //socket��
const int Buffer_Len = 1024;//��������С
const int Max_Username_Length = 20;
DWORD WINAPI Recv_Thread_Init(void* lp)//������Ϣ���߳�
{
	SOCKET ServerSock = *(SOCKET*)lp;//��ȡSOCKET����

	while (1)
	{
		char buffer[Buffer_Len] = { 0 };//�ַ������������ڽ��պͷ�����Ϣ
		int recv_msg= recv(ServerSock, buffer, sizeof(buffer), 0);
		if (recv_msg > 0)//������յ����ַ�������0
		{
			cout << buffer << endl;
		}
		else if (recv_msg < 0)//�����쳣
		{
			cout << "��������Ͽ�����" << endl;
			break;
		}
	}
	return 0;
}

int main() {
	//1. ��ʼ��
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//2. ��ip�Ͷ˿�
	SOCKET  ClientSock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN ClientAddr = { 0 };
	char ip[24] = { 0 };
	ClientAddr.sin_family = AF_INET;
	ClientAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP��ַ
	ClientAddr.sin_port = htons(10000);//�˿ں�

	SOCKADDR_IN ServerAddr = { 0 };
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ServerAddr.sin_port = htons(10000);
	//3. connect
	if (connect(ClientSock, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "Error occurrs when connecting to server..." << WSAGetLastError() << endl;
	}

	//����������Ϣ�߳�
	CloseHandle(CreateThread(NULL, 0, Recv_Thread_Init, (void*)&ClientSock, 0, 0));
	//���߳���������Ҫ���͵���Ϣ
	
	// ��ʾ�û������û���
	cout << "Please enter your username(up to 20 words)��"<<endl;
	char username[20] = { 0 };
	cin.getline(username, sizeof(username));

	// ɾ���û����ж���Ŀո�
	char trimmedUsername[20] = { 0 };
	int i, j = 0;
	for (i = 0; i < 20; i++) {
		if (username[i] != ' ' || (i > 0 && username[i - 1] != ' ')) {
			trimmedUsername[j] = username[i];
			j++;
		}
	}

	// ������������û���
	send(ClientSock, trimmedUsername, sizeof(trimmedUsername), 0);



	while (true)
	{
		char buf[Buffer_Len] = { 0 };
		cin.getline(buf, sizeof(buf));
		if (strcmp(buf, "quit") == 0 || strcmp(buf, "exit") == 0)
		{
			cout << "Do you want to leave? (y/n): ";
			char temp[Buffer_Len] = { 0 };
			cin.getline(temp, sizeof(temp));
			if (strcmp(temp, "y") == 0)
			{
				break; // �û����� 'y'���˳�ѭ��
			}
			else if (strcmp(temp, "n") == 0)
			{
				continue; // �û����� 'n'������ѭ��
			}
			else
			{
				cout << "Invalid input. Please enter 'y' to leave or 'n' to continue." << endl;
				continue; // �û�������Ч������ѭ��
			}
		}
		else if (strcmp(buf, "rename") == 0) 
		{
			cout << "-----Your new username: ";
			char username[20] = { 0 };
			cin.getline(username, sizeof(username));

			// ɾ���û����ж���Ŀո�
			char trimmedUsername[20] = { 0 };
			int i, j = 0;
			for (i = 0; i < 20; i++) {
				if (username[i] != ' ' || (i > 0 && username[i - 1] != ' ')) {
					trimmedUsername[j] = username[i];
					j++;
				}
			}

			// �������������������������û���
			string renameRequest = "rename " + string(trimmedUsername);
			send(ClientSock, renameRequest.c_str(), renameRequest.length(), 0);
			cout << "-----Your username has changed successfully."<<endl;
			continue;
		}

		send(ClientSock, buf, sizeof(buf), 0);
	}

	closesocket(ClientSock);
	WSACleanup();
	return 0;
}

