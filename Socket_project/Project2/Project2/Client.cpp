#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include<iostream>
#include <Winsock2.h>
#include <WS2tcpip.h> // 使用Winsock2库
#include<cstring>

using namespace std;

#pragma comment(lib,"ws2_32.lib")   //socket库
const int Buffer_Len = 1024;//缓冲区大小
const int Max_Username_Length = 20;
DWORD WINAPI Recv_Thread_Init(void* lp)//接收消息的线程
{
	SOCKET ServerSock = *(SOCKET*)lp;//获取SOCKET参数

	while (1)
	{
		char buffer[Buffer_Len] = { 0 };//字符缓冲区，用于接收和发送消息
		int recv_msg= recv(ServerSock, buffer, sizeof(buffer), 0);
		if (recv_msg > 0)//如果接收到的字符数大于0
		{
			cout << buffer << endl;
		}
		else if (recv_msg < 0)//链接异常
		{
			cout << "与服务器断开连接" << endl;
			break;
		}
	}
	return 0;
}

int main() {
	//1. 初始化
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//2. 绑定ip和端口
	SOCKET  ClientSock = socket(AF_INET, SOCK_STREAM, 0);
	SOCKADDR_IN ClientAddr = { 0 };
	char ip[24] = { 0 };
	ClientAddr.sin_family = AF_INET;
	ClientAddr.sin_addr.s_addr = inet_addr("127.0.0.1"); //IP地址
	ClientAddr.sin_port = htons(10000);//端口号

	SOCKADDR_IN ServerAddr = { 0 };
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ServerAddr.sin_port = htons(10000);
	//3. connect
	if (connect(ClientSock, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		cout << "Error occurrs when connecting to server..." << WSAGetLastError() << endl;
	}

	//创建接受消息线程
	CloseHandle(CreateThread(NULL, 0, Recv_Thread_Init, (void*)&ClientSock, 0, 0));
	//主线程用于输入要发送的消息
	
	// 提示用户输入用户名
	cout << "Please enter your username(up to 20 words)："<<endl;
	char username[20] = { 0 };
	cin.getline(username, sizeof(username));

	// 删除用户名中多余的空格
	char trimmedUsername[20] = { 0 };
	int i, j = 0;
	for (i = 0; i < 20; i++) {
		if (username[i] != ' ' || (i > 0 && username[i - 1] != ' ')) {
			trimmedUsername[j] = username[i];
			j++;
		}
	}

	// 向服务器发送用户名
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
				break; // 用户输入 'y'，退出循环
			}
			else if (strcmp(temp, "n") == 0)
			{
				continue; // 用户输入 'n'，继续循环
			}
			else
			{
				cout << "Invalid input. Please enter 'y' to leave or 'n' to continue." << endl;
				continue; // 用户输入无效，继续循环
			}
		}
		else if (strcmp(buf, "rename") == 0) 
		{
			cout << "-----Your new username: ";
			char username[20] = { 0 };
			cin.getline(username, sizeof(username));

			// 删除用户名中多余的空格
			char trimmedUsername[20] = { 0 };
			int i, j = 0;
			for (i = 0; i < 20; i++) {
				if (username[i] != ' ' || (i > 0 && username[i - 1] != ' ')) {
					trimmedUsername[j] = username[i];
					j++;
				}
			}

			// 向服务器发送重命名请求和新用户名
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

