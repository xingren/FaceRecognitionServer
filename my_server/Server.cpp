#include"stdafx.h"
#include"Server.h"

Server::Server(io_service &io,unsigned int port,std::string ip):io_service_(io),acceptor(io),is_start(false),sql_con(nullptr)
{
	set_server_addr(port,ip);
	CoreStartEvent = CreateEvent(NULL,FALSE,FALSE,"Core-Start-Event");


	sql_con = mysql_init((MYSQL*)NULL);

	if(sql_con == NULL)
	{
		printf("can't init MYSQL* \n");
		return ;
	}
	if(!mysql_real_connect(sql_con,"localhost","root","123","rui",0,NULL,0))
	{
		printf("failed to connect mariadb\n");
		return;
	}
	//mysql_query(&sql_con,"SET NAMES UTF8");


	//����ͨ�ţ��߳�ͬ��������ʼ��
	//share_mem_mutex = CreateMutex(NULL,FALSE,"share-mem-mutex");
	/*share_mem_full = CreateSemaphore(NULL,0,async_run_num,NULL);
	share_mem_empty = CreateSemaphore(NULL,async_run_num,async_run_num,NULL);*/
	hFileMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"file-mapping");
	hResultMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"result-mapping");
	file_mapping_buf = (char *)MapViewOfFile(hFileMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_buf = MapViewOfFile(hResultMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_mutex = CreateMutex(NULL,FALSE,"rects_mapping_mutex");//���߳�ӵ��mutex�������߳��޷�ӵ��
	file_mapping_mutex = CreateMutex(NULL,FALSE,"file_mapping_mutex");
//	file_mapping_op_finish = CreateEvent(NULL,FALSE,TRUE,"file_mapping_op_finish");

	if(nullptr == file_mapping_buf )
	{
		printf("error in MapViewOffFile with file_mapping_buf: %ld\n",GetLastError());
		
	}
	if(nullptr == rects_mapping_buf)
	{
		printf("error in MapViewOffFile with rects_mapping_buf: %ld\n",GetLastError());
	}



	/*
	���ڴ�����Ա�����̣߳������ַ���
		����һ��
		static DWORD WINAPI func(void *p)
		{
			Server *s = (Server*)p;
			s->Core();
			return 0;
		}
	*/
	//�����������߾ȹ���ͨ��һЩ�Ƚ��ػصķ�ʽ�����Ǿ�̬��Ա�����߳�
	//����void Class::func()���͵ĺ������ڱ����ʱ���ת������ͨ���� void func(Class *this),
	//Ҳ����˵����ÿ����Ա����������ת������ͨ��������ԭ���ĳ�Ա������һ������ǰ����һ�����thisָ�����
	//_beginthread�����ĵ�һ����������ʽҪ��Ϊvoid (*func)(void *)��ǡ�úͱ���õĳ�Ա��������ͬ���ͣ�
	//ͨ��union��void (*func)(void *)����ĳ�Ա����������һ��
	//ע�⣬�˷�������void Class::func()���͵ĳ�Ա����
	//����һ�ַ�ʽ�����þ�̬��Ա�������÷Ǿ�̬��Ա����
	union{
		unsigned int (WINAPI *ThreadProc)(void *param);
		unsigned int (WINAPI Server::*MemberProc)();
	}proc;
	proc.MemberProc =  &Server::Core;
	core_thread_handle = (HANDLE)_beginthreadex(NULL,NULL,proc.ThreadProc,this,NULL,&core_thread_id);

	WaitForSingleObject(CoreStartEvent,INFINITE);
	printf("already run the Core thread\n");
	char command[50];
	sprintf(command,"FaceRecognition %ld",core_thread_id);
	STARTUPINFO si;
	ZeroMemory(&si,sizeof(si));
	GetStartupInfo(&si);
	ZeroMemory(&recognition_process_info,sizeof(PROCESS_INFORMATION));
	
	

	if(NULL == hFileMapping)
	{
		printf("failed CreateFileMapping with hFileMapping: %ld \n",GetLastError());
		return ;
		
	}
	if(NULL == hResultMapping)
	{
		printf("failed CreateFileMapping with hFileMapping: %ld \n",GetLastError());
		return;
	}

	if(!CreateProcess(NULL,command,NULL,NULL,NULL,NULL,NULL,NULL,&si,&recognition_process_info))
	{
		printf("failed to start FaceRecognition Process\n");
		return;
	}
	Sleep(1000);
	CloseHandle(CoreStartEvent);
	
	
	if(!init())
	{
		std::cout << "can not start the server" <<  std::endl;
		
	}
	
	is_start = true;
}

Server::~Server()
{
	PostThreadMessage(recognition_main_thread_id,WM_EXIT,NULL,NULL);
	CloseHandle(core_thread_handle);
	CloseHandle(CoreStartEvent);
	

	mysql_close(sql_con);
	UnmapViewOfFile(file_mapping_buf);
	UnmapViewOfFile(rects_mapping_buf);

	CloseHandle(hFileMapping);
	CloseHandle(hResultMapping);

//	CloseHandle(file_mapping_op_finish);
	CloseHandle(rects_mapping_mutex);
}
bool Server::init()
{
	
	//�󶨶˿ڣ������˿�
	boost::system::error_code ec;
	acceptor.open(server_addr.protocol(),ec);

	//��Ӵ�����
	assert(!ec);

	acceptor.set_option(ip::tcp::acceptor::reuse_address(true),ec);

	//��Ӵ�����
	assert(!ec);

	acceptor.bind(server_addr,ec);

	//��Ӵ�����
	assert(!ec);
	if(ec)
	{
		io_service_.stop();
		///printf("logger error: bind failed\n");
		std::cout << "logger error: bind failed\n" << std::endl;
		return false;

	}
	
	acceptor.listen(socket_base::max_connections,ec);
	assert(!ec);

	if(ec)
	{
		io_service_.stop();
		std::cout <<"logger error:listen failed" << std::endl;

		
	}
	for(int i = 0;i < 1;i++)
	{
		start_next_accept();	
	}
	return true;
}
void Server::serialize_int(int val,char* out)
{
	out[0] = out[1] = out[2] = out[3] = 0;
	out[0] = (val & 255);
	out[1] = (val >> 8) & 255;
	out[2] = (val >> 16)&255;
	out[3] = (val >> 24)&255;
}

void Server::deserialize_int(uchar* in,int& val)
{
	val = 0;
	val |= in[0];
	val |= in[1] << 8;
	val |= in[2] << 16;
	val |= in[3] << 24;
}



unsigned int WINAPI Server::Core()
{
	MSG msg;
	PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
	//wrEvent = CreateEvent(NULL,FALSE,FALSE,FALSE);
	union{
		unsigned int  (WINAPI *func)(VOID* p);
		unsigned int  (WINAPI Server::*fun)();
	}proc;
	
	//proc.fun = &Server::write_share_file;

	//write_share_mem_thread = (HANDLE)_beginthreadex(NULL,NULL,proc.func,this,NULL,&write_share_mem_thread_id);
	//WaitForSingleObject(wrEvent,INFINITE);
	//ResetEvent(wrEvent);
	//proc.fun = &Server::read_result;
	//read_share_mem_thread = (HANDLE)_beginthreadex(NULL,NULL,proc.func,this,NULL,&read_share_mem_thread_id);
	//WaitForSingleObject(wrEvent,INFINITE);
	//CloseHandle(wrEvent);
	SetEvent(CoreStartEvent);
	BOOL bRet;
	
	is_start = false;
	while((bRet = GetMessage(&msg,NULL,0,0)) > 0)
	{
		TranslateMessage(&msg);
		switch(msg.message)
		{
		case WM_IMAGE:
			{
				uchar *recv_buf = (uchar*)msg.lParam;
				uchar* recv_file = recv_buf + 4;
				int clientId = msg.wParam;
				int total_bytes;
				deserialize_int(recv_buf,total_bytes);
				cout << "total bytes in Server::Core: " << total_bytes << endl;
//				WaitForSingleObject(file_mapping_op_finish,INFINITE);
//				ResetEvent(file_mapping_op_finish);
				WaitForSingleObject(file_mapping_mutex,INFINITE);
				memcpy(file_mapping_buf,recv_buf,total_bytes+4);
				ReleaseMutex(file_mapping_mutex);
				int count = 0;
				while(count < 10 && PostThreadMessage(recognition_thread_id,WM_IMAGE,clientId,NULL) == 0)
				{
					printf("error in post thread,tell recognition recieve image:%ld\n",GetLastError());
					count ++;
				}


				/*uchar* recv_file = recv_buf + 4;
				int total_bytes;
				deserialize_int((char*)recv_buf,total_bytes);
				vector<uchar> vec;
				vec.reserve(total_bytes);
				for(int i = 0;i<total_bytes;i++)
				{
					vec.push_back(recv_file[i]);
				}

				Mat img_mat = imdecode(vec,CV_LOAD_IMAGE_COLOR);
				imshow("recv_image",img_mat);
				cvWaitKey();*/
			}
			break;
		case WM_ADD_FACE://come from client
			{
				
			}
			break;
		case WM_ADD_PERSON:
			{}
			break;
		case WM_GET_RECOGNITION_ID:
			{
				recognition_thread_id = msg.wParam;
				printf("Get Recognition ID\n");
			}
			break;
		case WM_RESULT_DECTIVE://��FaceRecognition���͹����ļ����Ϣ
			{
				
				int size = msg.lParam;
				char *result = nullptr; //������ڶ��������ڴ棬���ջ���Client::send_dective_result���ͷ�
				int len = 0;
				if(size != 0)
				{
					result = new char[(sizeof(CvRect)+4)*size];
					WaitForSingleObject(rects_mapping_mutex,INFINITE);
					memcpy(result,rects_mapping_buf,(sizeof(CvRect)+4)*size);
					ReleaseMutex(rects_mapping_mutex);

					//�����
					vector<PersonData> persons;
					vector<CvRect> rects;
					int id;
					CvRect rect;
					PersonData p;
					for(int i = 0;i < size ;i++)
					{
						memcpy(&id,result + i*(sizeof(CvRect) + 4),4);
						memcpy(&rect,result + i*(sizeof(CvRect)+4)+4,sizeof(CvRect));
						p = getPersonInDatabase(id);
						persons.push_back(p);
						rects.push_back(rect);
						len += 2 + p.name.length() + 1 + sizeof(CvRect);
					}
					
					delete[] result;

					result = nullptr;
					result = new char[len+4];

					memcpy(result,&len,4);
					char* ptr = result + 4;
					for(int i = 0;i < size;i++)
					{
						//�����id��name�ĳ���
						id = persons.at(i).name.length();
						memcpy(ptr,&id,2);
						memcpy(ptr+2,persons.at(i).name.c_str(),id);
						memcpy(ptr+2+id,&rects.at(i),sizeof(CvRect));
						ptr += 2 + id + sizeof(CvRect);
					}

				}
				int clientId = msg.wParam;
				cli_map_mutex.lock();
				map<int,client_ptr>::iterator iter = client_map.find(clientId);
				if(iter != client_map.end())
				{
					iter->second->send_dective_result(result,len+4);
				}
				cli_map_mutex.unlock();
			}
			break;
		case WM_RESULT_FIND://come from recognition thread
			{
				
			}
			break;
		case WM_RECOGNITION_ERROR:
			{
				printf("recognition error\n");
				abort();
			}
			break;
		case WM_CLIENT_DISCONNECT:
			{
				int id = msg.wParam;
				cli_map_mutex.lock();
				
				map<int,client_ptr>::iterator iter;
				iter = client_map.find(id);
				if(iter == client_map.end())
				{
					printf("client already be deleted\n");
				}
				else
				{
					printf("client didn't be deleted\n");
				}
				cli_map_mutex.unlock();
				PostThreadMessage(recognition_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
			}
			break;
		default:
			{
				printf("server:undefine message\n");
			}
			break;
		}
	}

	return 0;
}

boost::shared_ptr<Client> Server::create_client()
{
	client_ptr tmp = boost::make_shared<Client>(io_service_,core_thread_id);
	client_map.insert(pair<int,client_ptr>(tmp->id,tmp));
	return tmp;
}
void Server::start_next_accept()
{
	client_ptr new_client = create_client();
	acceptor.async_accept(new_client->get_socket(),boost::bind(&Server::accept_handler,this,boost::asio::placeholders::error,new_client));
}

void Server::run()
{
	boost::thread_group tg;
	for(int i = 0;i < async_run_num;i++)
		tg.create_thread(boost::bind(&io_service::run,&io_service_));
	tg.join_all();
}

void Server::start()
{
	std::cout << "server start" << std::endl;
	boost::thread t(boost::bind(&Server::run,this));
	t.join();
	
	
}

void Server::accept_handler(const boost::system::error_code& ec,client_ptr& client)
{
	if(!ec)
	{
		
		std::cout << "logger: client connect" << std::endl;
		std::cout << client->cli_socket.remote_endpoint().address() << std::endl;
		client->start();
		start_next_accept();
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << endl;
	}
}

void Server::set_server_addr(unsigned int port,std::string ip)
{
	if(ip.empty())
		server_addr = ip::tcp::endpoint(DEFAULT_IP_VERSION,port);
	else
	{
		boost::system::error_code ec;
		server_addr = ip::tcp::endpoint(ip::address::from_string(ip,ec),port);assert(!ec);
	}
	
}


PersonData Server::getPersonInDatabase(int client_id)
{
	char query[50];
	sprintf(query,"select * from person where Id=%d",client_id);
	MYSQL_RES* result = nullptr;
	int resCount;
	resCount = mysql_query(sql_con,query);
	PersonData p;
	if(0 != resCount)
	{
		result = mysql_store_result(sql_con);
		if(nullptr != result)
		{
			MYSQL_ROW row = nullptr;
			row = mysql_fetch_row(result);
			p.name = row[1];
			p.sex = atoi(row[2]);
		}
	}
	return p;
}


//unsigned int WINAPI Server::write_share_file()
//{
//	MSG msg;
//	PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
//	SetEvent(wrEvent);
//	BOOL bRet;
//	while((bRet = GetMessage(&msg,NULL,0,0)) > 0)
//	{
//		TranslateMessage(&msg);
//		switch(msg.message                                                                                                                                                                                                                             )
//		{
//		case WM_IMAGE:
//			{
//				char *p = (char*)msg.wParam;
//				size_t size = (size_t)msg.lParam;
//				char *pbuf = (char*)MapViewOfFile(hFileMapping,FILE_MAP_WRITE,0,0,IPC_BUFF_SIZE);
//				WaitForSingleObject(share_mem_empty,INFINITE);
//				WaitForSingleObject(share_mem_mutex,INFINITE);
//
//				memcpy(pbuf,p,size);
//
//				ReleaseMutex(share_mem_mutex);
//				ReleaseSemaphore(share_mem_mutex,1,NULL);
//			}
//			break;
//		}
//	}
//
//	return 0;
//}
//
//unsigned int Server::read_result()
//{
//	MSG msg;
//	PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
//	SetEvent(wrEvent);
//	BOOL bRet;
//	while((bRet = GetMessage(&msg,NULL,0,0)) > 0)
//	{
//		TranslateMessage(&msg);
//		switch(msg.message)
//		{
//		case WM_RESULT:
//			{
//				char *p = (char*)msg.wParam;
//				char *pbuf = (char*)MapViewOfFile(hFileMapping,FILE_MAP_WRITE,0,0,IPC_BUFF_SIZE);
//				WaitForSingleObject(share_mem_empty,INFINITE);
//				WaitForSingleObject(share_mem_mutex,INFINITE);
//				ReleaseMutex(share_mem_mutex);
//				ReleaseSemaphore(share_mem_mutex,1,NULL);
//			}
//			break;
//		}
//	}
//
//	return 0;
//}