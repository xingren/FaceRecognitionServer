<<<<<<< HEAD
// my_server.cpp : �������̨Ӧ�ó������ڵ㡣
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
		printf("�޷�����������\n");
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

=======
// my_server.cpp : �������̨Ӧ�ó������ڵ㡣
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
		printf("�޷�����������\n");
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

>>>>>>> ceaf66a127ebb6267244e49b2ed69def37b5572d
