// my_server.cpp : 定义控制台应用程序的入口点。
//

#include"stdafx.h"
#include "common.h"

#include "Server.h"
using namespace std;





using namespace boost::asio;

int main()
{
	SetConsoleTitle("send");


	io_service service;
	Server server(service,PORT);
	server.start();
	string str;
	if(!server.is_start)
	{
		printf("无法启动服务器\n");
		return 0;
	}
	while(server.is_start)
	{
		cin >> str;
		if(str == "exit")
			break;
	}
	

	return 0;
}

