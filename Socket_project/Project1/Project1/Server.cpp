#include <stdio.h>
#include <stdlib.h>
#include<iostream>
#include <cstring>
#include <cstdlib>
#include <Winsock2.h>
#include <WS2tcpip.h> // 使用Winsock2库
#include <thread>
#include <vector>
using namespace std;
//使用的全局变量
#pragma comment(lib,"ws2_32.lib")   //socket库

const int Buffer_Len = 1024;//缓冲区大小
const int Wait_Time = 10;//每个客户端等待事件的时间，单位毫秒
const int Max_Clients = 128;//服务端最大链接数
int total = 0;//当前已经链接的客服端服务数
const int Max_Username_Length = 20;//用户名最大长度为20

SOCKET ClientSock[Max_Clients];//客户端套接字
SOCKADDR_IN ClientAddr[Max_Clients];//客户端地址
WSAEVENT ClientEvent[Max_Clients];//客户端事件

string ClientUsername[Max_Clients];//客户端用户名
int ClientEnterIndex[Max_Clients];//进入顺序，相当于ID

DWORD WINAPI Server_Thread_Init(void*lp) //服务器端线程
{
	//主线程初始化
	SOCKET servSock = *(SOCKET*)lp;
	while (true) 
	{   //主线程保持常驻开启状态
		for (int i = 0; i < total + 1; i++)
		{
			//对每一个终端（客户端和服务端），查看是否发生事件，等待Wait_Time毫秒
			int index = WSAWaitForMultipleEvents(1, &ClientEvent[i], false, Wait_Time, 0);

			index -= WSA_WAIT_EVENT_0;//index为发生事件实际索引

			if (index == WSA_WAIT_TIMEOUT || index == WSA_WAIT_FAILED)
			{
				continue;//如果出错或者超时，即跳过此终端
			}

			else if (index == 0)//套接字注册网络事件成功，函数返回0
			{
				WSANETWORKEVENTS NetworkEvents;//创建事件对象
				WSAEnumNetworkEvents(ClientSock[i], ClientEvent[i], &NetworkEvents);//检测当前客户端套接字的网络事件
				//三种事件：客户端连接，退出，收到消息
				if (NetworkEvents.lNetworkEvents & FD_ACCEPT)//有客户端链接
				{
					if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
					{
						cout << "Accept Error: " << NetworkEvents.iErrorCode[FD_ACCEPT_BIT] << endl;
						continue;
					}

					if (total + 1 < Max_Clients)//下一个客户端可以加入
					{
						int New_Index = total + 1;
						int len = sizeof(SOCKADDR);
						//接受来自客户端的连接请求，并创建一个新的套接字
						SOCKET NewSock = accept(servSock, (SOCKADDR*)&ClientAddr[New_Index], &len);

						if (NewSock != INVALID_SOCKET)
						{
							ClientSock[New_Index] = NewSock;
							WSAEVENT newEvent = WSACreateEvent();//创建新客户端套接字，绑定事件对象
							WSAEventSelect(ClientSock[New_Index], newEvent, FD_CLOSE | FD_READ | FD_WRITE);//设置套接字上要监视的事件
							ClientEvent[New_Index] = newEvent;
							ClientEnterIndex[New_Index] = New_Index+1000;
							total++;
							//服务器端新进入客户端提醒：
							char ip[24] = { 0 };
							cout << "*" << " Customer " << ClientEnterIndex[New_Index] << " (IP: " 
								<< inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[New_Index].sin_addr, ip, sizeof(ip)) << ") enters room, there are " << total << " customers." << endl;

							// 给该客户端发送欢迎提醒
							char welcomeMessage[] = "#################################################\n"
								"#                                               #\n"
								"#         Welcome to Chatland!                  #\n"
								"#   Enter 'exit' or 'quit' to leave the room.   #\n"
								"#   Enter 'rename' to change your current name. #\n"
								"#                                               #\n"
								"#################################################\n";
							send(ClientSock[New_Index], welcomeMessage, sizeof(welcomeMessage), 0);

							// 广播
							char buf[Buffer_Len] = "Chatland:Welcome customer: (IP: ";
							strcat_s(buf, inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[New_Index].sin_addr, ip, sizeof(ip)));
							strcat_s(buf, ") to join us.");

							for (int j = 1; j <= total; j++)
							{
								if (j == New_Index)
									continue; // 除了新加入的自身
								send(ClientSock[j], buf, sizeof(buf), 0);
							}
						}
					}
					else
					{
						// 人满时的错误消息
						perror("accept");
						exit(0);
					}
				}
				else if (NetworkEvents.lNetworkEvents & FD_CLOSE)
				//客户端断开连接
				{
					
					char ip[24] = { 0 };
					//服务器端客户端离开提醒
					cout << "*" <<  "Customer"<<i <<"（IP：" << inet_ntop(AF_INET, (SOCKADDR*)&ClientAddr[i].sin_addr, ip, sizeof(ip))
						<< ")leaves the room,there exits：" << --total << endl;
					//释放资源
					closesocket(ClientSock[i]);
					WSACloseEvent(ClientEvent[i]);
					
					//给所有客户端发送退出聊天室的消息
					char leaveMessage[Buffer_Len];
					sprintf_s(leaveMessage, "Chatland: Customer %d (%s)（IP：%s）leaves.", i, ClientUsername[i].c_str(), ip);
					memset(ip, 0, sizeof(ip));
					//先调整数组
					for (int j = i; j < total+1; j++)
					{
						ClientSock[j] = ClientSock[j + 1];
						ClientEvent[j] = ClientEvent[j + 1];
						ClientAddr[j] = ClientAddr[j + 1];
						ClientUsername[j] = ClientUsername[j + 1];
						ClientEnterIndex[j] = ClientEnterIndex[j + 1];
					}
					for (int j = 1; j <= total; j++)
						send(ClientSock[j], leaveMessage, sizeof(leaveMessage), 0);//广播
					
				}
				else if (NetworkEvents.lNetworkEvents & FD_READ)//有客户端消息
				{
					char RecvBuffer[Buffer_Len] = { 0 }; // 初始化字符缓冲区
					char SendBuffer[Buffer_Len] = { 0 };

					for (int j = 1; j <= total; j++)//对每一个客户端
					{
						int nrecv = recv(ClientSock[j], RecvBuffer, sizeof(RecvBuffer), 0);
						if (nrecv > 0) // 如果接收到的字符数大于0
						{
							// 获取客户端的IP地址，用于下面的操作
							char clientIP[INET_ADDRSTRLEN];
							inet_ntop(AF_INET, &(ClientAddr[j].sin_addr), clientIP, INET_ADDRSTRLEN);
							if (ClientUsername[j].empty()) {
								
								// 保存客户端发送的用户名
								ClientUsername[j] = RecvBuffer;

								// 服务器端展示更新信息
								cout << "*" << " New customer "<< ClientEnterIndex[j] << "(IP: "<< clientIP << ") named " << ClientUsername[j] << " is in the room.Total clients : " << total << endl;

								// 广播欢迎信息
								char welcomeMessage[Buffer_Len];
								sprintf_s(welcomeMessage, "Chatland: Welcome %s (IP: %s) to join us.", ClientUsername[j].c_str(), clientIP);
								for (int j = 1; j <= total; j++)
									send(ClientSock[j], welcomeMessage, sizeof(welcomeMessage), 0);
							}
							else {
								if (strncmp(RecvBuffer, "rename ",7) == 0) {
								
									// 保存客户端发送的用户名
									string newUsername = &RecvBuffer[7];
									ClientUsername[j] = newUsername;

									// 展示更新信息
									cout << "*" << "Customer" << ClientEnterIndex[j] <<"(IP: " << clientIP << ") is renamed as " << ClientUsername[j]<<"." << endl;

								}
								else {
									sprintf_s(SendBuffer, "[Client %d (%s) at %s]: %s", ClientEnterIndex[j], ClientUsername[j].c_str(), clientIP, RecvBuffer);
									cout << SendBuffer << endl;

									// 转发客户端发送信息
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
	//创建监听用的套接字
	//1、socket创建套接字，返回句柄
	SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);

	//第三个参数一般设置0，当确定套接字使用的协议簇和类型时，这个参数的值就为0	(flag==0)
	if (listener == -1) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		perror("Server listener does not work……");
		exit(0);//建立监听失败
	}
	else printf("服务器启动成功...\n");


	//2、bind/listen 绑定、监听端口
	//端口号用于区别应用
	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;//协议簇
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;
	server_addr.sin_port = htons(10000);//设置端口号为10000
	int ret = bind(listener, (struct sockaddr*)&server_addr, sizeof(sockaddr_in));
	if (ret == -1)
	{
		perror("bind does not work...");
		exit(0);
	}

	WSAEVENT ServerEvent = WSACreateEvent();//使用WSACreateEvent创建事件对象ServerEvent
	WSAEventSelect(listener, ServerEvent, FD_ALL_EVENTS);//使用WSAEventSelect将其与监听套接字相关联。绑定事件对象，并且监听所有事件

	ClientSock[0] = listener;
	ClientEvent[0] = ServerEvent;


	// 3. 设置监听
	ret = listen(listener, 128);//设置最大连接数与监听
	if (ret == -1)
	{
		perror("listen does not work...");
		exit(0);
	}
	//4. 创建主线程（服务器线程
	CloseHandle(CreateThread(NULL, 0, &Server_Thread_Init, (void*)&listener, 0, 0));

	cout << "------Chatland is open!------" << endl;

	//5. 服务器自身不断地读取输入
	while (true)
	{
		char ServerInput[Buffer_Len] = { 0 };
		char Send_Buf[Buffer_Len] = { 0 };

		// 从服务器控制台读取消息
		cin.getline(ServerInput, sizeof(ServerInput));

		// 格式化服务器消息
		sprintf_s(Send_Buf, "[Chatland]: %s", ServerInput);

		// 广播服务器消息给所有客户端
		for (int j = 1; j <= total; j++)
		{
			send(ClientSock[j], Send_Buf, sizeof(Send_Buf), 0);
		}
	}

	WSACleanup();
}


