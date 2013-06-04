// Controler.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#ifdef WIN32
DWORD recognition_id;
DWORD server_id;
HANDLE wait;
#endif
unsigned int WINAPI ProcessMessage(VOID* pVOID)
{
	MSG msg;
	BOOL bRet;
	while((bRet = GetMessage(&msg,NULL,NULL,NULL)>0))
	{
		switch(msg.message)
		{
		case WM_EXIT:
			PostThreadMessage(server_id,WM_EXIT,NULL,NULL);
			Sleep(1000);
			SetEvent(wait);
			break;
		case WM_SAVE:

			break;
		case WM_SERVER_ID:
			server_id = msg.wParam;
			recognition_id = msg.lParam;
			SetEvent(wait);
			break;
		}


	}
	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	SetConsoleTitle("Controler");

	DWORD process_message_thread_id = 0;
	HANDLE process_message_thread;
	wait = CreateEvent(NULL,FALSE,FALSE,NULL);
	process_message_thread = (HANDLE) _beginthreadex(NULL,NULL,ProcessMessage,NULL,NULL,(unsigned int *)&process_message_thread_id);
	
	
	char command[50];
	sprintf_s(command,"my_server %ld",process_message_thread_id);
	STARTUPINFO si;
	PROCESS_INFORMATION server_process_info;
	ZeroMemory(&si,sizeof(si));
	GetStartupInfo(&si);
	ZeroMemory(&server_process_info,sizeof(PROCESS_INFORMATION));
	if(!CreateProcess(NULL,command,NULL,NULL,NULL,CREATE_NEW_CONSOLE,NULL,NULL,&si,&server_process_info))
	{
		printf("failed to start FaceRecognition Process\n");
		CloseHandle(process_message_thread);
		return 0;
	}
	WaitForSingleObject(wait,INFINITE);

	

	while(1)
	{
		printf("输入操作参数\n");
		scanf_s("%s",command,50);
		if(strcmp(command,"EXIT") == 0)
		{
			PostThreadMessage(process_message_thread_id,WM_EXIT,NULL,NULL);
			WaitForSingleObject(wait,INFINITE);
			break;
		}
		else if(strcmp(command,"SAVE") == 0)
		{
			PostThreadMessage(recognition_id,WM_SAVE,NULL,NULL);
		}
		else
		{
			printf("command not correct\n");
		}
	}

	

	return 0;
}

