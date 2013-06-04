#include"stdafx.h"
#include"Server.h"

char Server::text[1000]="";

Server::Server(io_service &io,unsigned int port,std::string ip):io_service_(io),acceptor(io),is_start(false),sql_con(nullptr)
{

	controler = 0;
	char* command_line = GetCommandLine();
	if(strcmp(command_line,"") != 0)
	{
		stringstream sstr(command_line);
		string str;
		//printf("%s\n",command_line);
		sprintf_s(text,"command line:%s",command_line);
		print_line_by_lock(text);
		
		//cout << "command line:" << command_line << endl;
		sstr >> str;
		sstr >> controler;
		//cout << "controler:" << controler << endl;
		sprintf_s(text,"controler:%ld",controler);
		print_line_by_lock(text);
		
	}
	else
	{
		//printf("no command line\n");
		print_line_by_lock("no command line");
	}

	set_server_addr(port,ip);
	CoreStartEvent = CreateEvent(NULL,FALSE,FALSE,"Core-Start-Event");


	sql_con = mysql_init((MYSQL*)NULL);

	if(sql_con == NULL)
	{
		//printf("can't init MYSQL* \n");
		print_line_by_lock("can't init MYSQL*");
		logger.PrintLog("Server construct","can't init MYSQL*");
		return ;
	}
	if(!mysql_real_connect(sql_con,"localhost","root","123","rui",0,NULL,0))
	{
		//printf("failed to connect mariadb\n");
		print_line_by_lock("failed to connect mariadb");
		logger.PrintLog("Server construct","failed to connect mariadb");
		return;
	}
	//mysql_query(&sql_con,"SET NAMES UTF8");


	//进程通信，线程同步操作初始化

	hFileMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"file-mapping");
	hResultMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"result-mapping");
	hAddMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"add-mapping");
	file_mapping_buf = (char *)MapViewOfFile(hFileMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_buf = MapViewOfFile(hResultMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	add_mapping_buf = (char *)MapViewOfFile(hAddMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_mutex = CreateMutex(NULL,FALSE,"Global\\rects_mapping_mutex");
	file_mapping_mutex = CreateMutex(NULL,FALSE,"Global\\file_mapping_mutex");
	add_mapping_mutex = CreateMutex(NULL,FALSE,"Global\\add_mapping_mutex");
	wait_for_recognition_load = CreateEvent(NULL,FALSE,FALSE,"recognition_load");



	if(nullptr == rects_mapping_mutex)
	{
		sprintf_s(text,"error in CreateMutext,rects_mapping_mutex:%ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}
	if(nullptr == file_mapping_mutex)
	{
		sprintf_s(text,"error in CreateMutext,file_mapping_mutex:%ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}
	if(nullptr == file_mapping_buf )
	{
		sprintf_s(text,"error in MapViewOffFile with file_mapping_buf: %ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}
	if(nullptr == rects_mapping_buf)
	{
		sprintf_s(text,"error in MapViewOffFile with rects_mapping_buf: %ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}
	if(nullptr == add_mapping_buf)
	{
		sprintf_s(text,"error in MapViewOffFile with add_mapping_buf: %ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}
	if(nullptr == add_mapping_mutex)
	{
		sprintf_s(text,"error in CreateMutex with add_mapping_mutex: %ld\n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}



	/*
	对于创建成员函数线程，有两种方法
	方法一：
	static DWORD WINAPI func(void *p)
	{
	Server *s = (Server*)p;
	s->Core();
	return 0;
	}
	*/
	//方法二：曲线救国，通过一些比较迂回的方式创建非静态成员函数线程
	//对于void Class::func()类型的函数，在编译的时候会转换成普通函数 void func(Class *this),
	//也就是说对于每个成员函数，都会转换成普通函数，并原来的成员函数第一个参数前插入一个类的this指针参数
	//_beginthread函数的第一个参数的形式要求为void (*func)(void *)，恰好和编译好的无传入参数成员函数属于同类型，
	//通过union将void (*func)(void *)和类的成员函数关联在一起
	//注意，此方法仅限void Class::func()类型的成员函数
	//还有一种方式就是用静态成员函数调用非静态成员函数
	union{
		unsigned int (WINAPI *ThreadProc)(void *param);
		unsigned int (WINAPI Server::*MemberProc)();
	}proc;
	proc.MemberProc =  &Server::Core;
	core_thread_handle = (HANDLE)_beginthreadex(NULL,NULL,proc.ThreadProc,this,NULL,&core_thread_id);

	WaitForSingleObject(CoreStartEvent,INFINITE);
	print_by_lock("already run the Core thread\n");
	logger.PrintLog("Server construct","already run the Core thread\n");
	char command[50];
	sprintf_s(command,"FaceRecognition %ld",core_thread_id);
	STARTUPINFO si;
	ZeroMemory(&si,sizeof(si));
	GetStartupInfo(&si);
	ZeroMemory(&recognition_process_info,sizeof(PROCESS_INFORMATION));


	if(NULL == hFileMapping)
	{
		sprintf_s(text,"failed CreateFileMapping with hFileMapping: %ld \n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return ;

	}
	if(NULL == hResultMapping)
	{
		sprintf_s(text,"failed CreateFileMapping with hFileMapping: %ld \n",GetLastError());
		print_by_lock(text);
		logger.PrintLog("Server construct",text);
		return;
	}

	if(!CreateProcess(NULL,command,NULL,NULL,NULL,CREATE_NEW_CONSOLE,NULL,NULL,&si,&recognition_process_info))
	{
		print_by_lock("failed to start FaceRecognition Process\n");
		logger.PrintLog("Server construct","failed to start FaceRecognition Process");
		return;
	}
	Sleep(1000);
	CloseHandle(CoreStartEvent);

	WaitForSingleObject(wait_for_recognition_load,INFINITE);

	//std::cout << "begin init acceptor" << std::endl;
	print_by_lock("begin init acceptor\n");
	logger.PrintLog("Server construct","begin init acceptor\n");
	if(!acceptor_init())
	{
		print_by_lock("init acceptor failed\n" );
		logger.PrintLog("Server construct","init acceptor failed\n" );
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
	CloseHandle(hAddMapping);

	CloseHandle(file_mapping_mutex);
	CloseHandle(rects_mapping_mutex);
	CloseHandle(add_mapping_mutex);
	CloseHandle(wait_for_recognition_load);
}
bool Server::acceptor_init()
{

	//绑定端口，监听端口
	boost::system::error_code ec;
	acceptor.open(server_addr.protocol(),ec);

	//添加错误处理
	assert(!ec);

	acceptor.set_option(ip::tcp::acceptor::reuse_address(true),ec);

	//添加错误处理
	assert(!ec);

	acceptor.bind(server_addr,ec);

	//添加错误处理
	assert(!ec);
	if(ec)
	{
		io_service_.stop();
		///printf("logger error: bind failed\n");
		print_by_lock("bind failed\n");
		logger.PrintLog("Server construct","bind failed");
		return false;

	}

	acceptor.listen(socket_base::max_connections,ec);
	assert(!ec);

	if(ec)
	{
		io_service_.stop();
		print_by_lock("listen failed\n");
		logger.PrintLog("Server construct","listen failed");

	}
	for(int i = 0;i < 1;i++)
	{
		start_next_accept();	
	}
	return true;
}

unsigned int WINAPI Server::Core()
{
	MSG msg;
	PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
	//wrEvent = CreateEvent(NULL,FALSE,FALSE,FALSE);

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
				//deserialize_int(recv_buf,total_bytes);
				memcpy(&total_bytes,recv_buf,sizeof(int));
				//cout << "total bytes in Server::Core: " << total_bytes << endl;
				sprintf_s(text,"total bytes in Server::Core: %d\n",total_bytes);
				print_by_lock(text);
				
				DWORD dw = WaitForSingleObject(file_mapping_mutex,MAX_WAIT_TIME);
				switch(dw)
				{
				case WAIT_TIMEOUT:
					print_by_lock("my_server error becuase read result of recognition timeout\n");
					//进程FaceRecognition任务太多
					logger.PrintLog("Server Core thread","my_server error becuase read result of recognition timeout");
					break;
				}
				memcpy(file_mapping_buf,recv_buf,total_bytes+4);
				
				int count = 0;
				while(count < 10 && PostThreadMessage(recognition_thread_id,WM_IMAGE,clientId,NULL) == 0)
				{
					sprintf_s(text,"error in post thread,tell recognition recieve image:%ld\n",GetLastError());
					print_by_lock(text);
					logger.PrintLog("Server Core thread",text);
					count ++;
				}

				if(nullptr!=recv_buf)
				{
					delete[] recv_buf;
				}
			}
			break;
		case WM_MODIFY: //come from Client
			{
				//包的格式:包的长度+//paper_id的长度(2) + paper_id + name的长度(2) + name + sex(1) + CvRect 
				size_t size;

				int clientId = msg.wParam;
				char* ptr = (char*)msg.lParam;
				memcpy(&size,ptr,4);
				int nameLen = 0;
				int paper_id_len = 0;
				string name,paper_id;
				char sex;
				memcpy(&paper_id_len,ptr+4,2);
				memcpy(&nameLen,ptr+4 +2+ paper_id_len,2);

				if(nameLen != 0 && nameLen != size-16-1-2-paper_id_len-2)
				{
					//cout << "client " << clientId << " MODIFY request has wrong package" << endl;
					sprintf_s(text,"client %d MODIFY request has wrong package\n");
					print_by_lock(text);
					logger.PrintLog("Server Core",text);

					delete[] ptr;
					break;
				}
				paper_id.append(ptr+4+2,paper_id_len);
				name.append(ptr+4+2+paper_id_len+2,nameLen);
				sex = ptr[4+2+paper_id_len+2+nameLen];

				MyRect rect;
				memcpy(&rect,ptr+4+2+nameLen+2+paper_id_len+1,sizeof(CvRect));
				//cout << "CvRect: " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;
				sprintf_s(text,"CvRect: %d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);
				print_by_lock(text);
				int personId;
				client_ptr client;
				client_map_mutex.lock();

				map<int,client_ptr>::iterator client_iter = client_map.find(clientId);

				if(client_iter == client_map.end())
				{
					//cout << "client had been removed from client_map !!" << endl;
					print_by_lock("client had been removed from client_map !!\n");
					logger.PrintLog("Server Core","client had been removed from client_map !!");
					delete[] ptr;
					break;
				}
				client = client_iter->second;
				client_map_mutex.unlock();

				//找出CvRect对应的personId
				map<MyRect,int>::iterator id_iter = client->personId_map.find(rect);
				//不存在对应的personId
				if(id_iter == client->personId_map.end())
				{
					sprintf_s(text,"not exist CvRect: %d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);
					print_by_lock(text);
					logger.PrintLog("Server Core",text);
					delete[] ptr;
					break;
				}
				personId = id_iter->second;

				if(personId == -1) //未配对上人脸数据库的
				{
					//先在人信息数据中查找对应的paper_id
					personId = find_person_id_in_database(paper_id);//没有该证件号的人
					if(personId  == -1)
					{
						personId = add_person_to_database(paper_id,name,sex);
					}
					else //有该证件号的人，更新信息
					{
						update_person_in_database(personId,name,sex);
					}
					//并且通知Recognition加人脸加入数据库中
					//personId(4) + CvRect
					WaitForSingleObject(add_mapping_mutex,INFINITE);
					memcpy(add_mapping_buf,&personId,4);
					memcpy((char *)add_mapping_buf + 4,&rect,sizeof(rect));
					PostThreadMessage(recognition_thread_id,WM_ADD_FACE,clientId,NULL);

				}
				else//更新信息
				{
					int tmpId = find_person_id_in_database(paper_id);
					if(tmpId == personId)//是同一人
						update_person_in_database(personId,name,sex);
					else//不是同一人，识别错误,
					{
						if(tmpId != -1)
						{
							//更新
							update_person_in_database(tmpId,name,sex);
						}
						else //是一个新的人
						{
							tmpId = add_person_to_database(paper_id,name,sex);

						}
						//更正Client类里的personId_map
						id_iter->second = tmpId;
						//将该人脸加入人脸数据库中
						WaitForSingleObject(add_mapping_mutex,INFINITE);
						memcpy(add_mapping_buf,&tmpId,4);
						memcpy((char *)add_mapping_buf + 4,&rect,sizeof(rect));
						PostThreadMessage(recognition_thread_id,WM_ADD_FACE,clientId,NULL);
						
					}
				}
				delete[] ptr;
			}
			break;
		case WM_GET_RECOGNITION_ID:
			{
				recognition_thread_id = msg.wParam;
				cout << "controler id:" << controler << endl;
				if(controler != 0)
				{
					PostThreadMessage(controler,WM_SERVER_ID,core_thread_id,recognition_thread_id);
				}
				print_by_lock("Get Recognition ID\n");
			}
			break;
		case WM_RESULT_DECTIVE://从FaceRecognition传送过来的检测消息
			{
				int clientId = msg.wParam;
				sprintf_s(text,"WM_RESULT_DECTIVE with client_id %d\n",clientId);
				print_by_lock(text);
				int size = msg.lParam;
				char *result = nullptr; //如果有在堆上申请内存，最终会在Client::send_dective_result内释放
				int len = 0;
				if(size != 0)
				{
					result = new char[(sizeof(CvRect)+4)*size];
					//WaitForSingleObject(rects_mapping_mutex,INFINITE);
					memcpy(result,rects_mapping_buf,(sizeof(CvRect)+4)*size);
					ReleaseMutex(rects_mapping_mutex);

					//构造包
					vector<PersonData> persons;
					vector<CvRect> rects;
					int person_id;
					MyRect rect;
					PersonData p;
					client_map_mutex.lock();
					map<int,client_ptr>::iterator client_iter = client_map.find(clientId);

					if(client_iter == client_map.end())
					{
						print_by_lock("client had been removed from client_map !!\n");
						logger.PrintLog("Server Core","client had been removed from client_map !!");
						//通知FaceRecognition删除该client
						PostThreadMessage(recognition_thread_id,WM_CLIENT_DISCONNECT,clientId,NULL);
						delete[] result;
						break;;
					}
					client_ptr client = client_iter->second;
					for(int i = 0;i < size ;i++)
					{
						memcpy(&person_id,result + i*(sizeof(CvRect) + 4),4);
						memcpy(&rect,result + i*(sizeof(CvRect)+4)+4,sizeof(CvRect));

						sprintf_s(text,"CvRect: %d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);
						print_by_lock(text);

						p = get_person_from_database(person_id);
						persons.push_back(p);
						rects.push_back(rect);
						map<MyRect,int>::iterator iter = client->personId_map.find(rect);
						if(iter != client->personId_map.end())
						{
							iter->second = person_id;
						}
						else
						{
							client->personId_map.insert(pair<MyRect,int>(rect,person_id));
						}
						sprintf_s(text,"the %d rect is:%d,%d,%d,%d\n",i,rect.x,rect.y,rect.width,rect.height);
						print_by_lock(text);
						len += 2 + p.paper_id.length() +  2 + p.name.length() + 1 + sizeof(CvRect);
					}
					client_map_mutex.unlock();
					delete[] result;

					result = nullptr;
					result = new char[len+4];

					memcpy(result,&len,4);
					char* ptr = result + 4;
					int &nlen = person_id;
					for(int i = 0;i < size;i++)
					{
						nlen = persons.at(i).paper_id.length();
						memcpy(ptr,&nlen,2);
						memcpy(ptr + 2,persons.at(i).paper_id.c_str(),nlen);
						nlen = persons.at(i).name.length();
						memcpy(ptr + 2 + persons.at(i).paper_id.length(),&nlen,2);
						memcpy(ptr+2 + persons.at(i).paper_id.length() + 2,
							persons.at(i).name.c_str(),
							nlen);
						memcpy(ptr+2+nlen + 2 + persons.at(i).paper_id.length()
							,&persons.at(i).sex,1);
						memcpy(ptr +2 + persons.at(i).paper_id.length()+ 2 + nlen + 1,&rects.at(i),sizeof(CvRect));
						sprintf_s(text,"%d the sex is %d\n",i,persons.at(i).sex);
						print_by_lock(text);
						ptr += 2+ persons.at(i).paper_id.length() + 2 + nlen + 1 + sizeof(CvRect);
					}
					len += 4;
				}

				client_map_mutex.lock();
				map<int,client_ptr>::iterator iter = client_map.find(clientId);
				if(iter != client_map.end())
				{
					sprintf_s(text,"send recognition result to client: %d result length %d\n" ,clientId,len);
					print_by_lock(text);
					logger.PrintLog("Server Core",text);
					iter->second->send_dective_result(result,len);
				}
				client_map_mutex.unlock();
			}
			break;
		case WM_RECOGNITION_ERROR:
			{
				print_by_lock("recognition error\n");
				logger.PrintLog("Server Core","recognition error\n");
				abort();
			}
			break;
		case WM_CLIENT_DISCONNECT:
			{
				int id = msg.wParam;
				client_map_mutex.lock();

				map<int,client_ptr>::iterator iter;
				iter = client_map.find(id);
				if(iter == client_map.end())
				{
					print_by_lock("client already be deleted\n");
					logger.PrintLog("Server Core","client already be deleted");
				}
				else
				{
					print_by_lock("client didn't be deleted\n");
					logger.PrintLog("Server Core","client didn't be deleted");
				}
				client_map_mutex.unlock();
				PostThreadMessage(recognition_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
			}
			break;
		case WM_ADD_FACE_FAILED:
			{
				int personId = msg.lParam;
				delete_person_from_database(personId);
			}
			break;
		case WM_EXIT:
			PostThreadMessage(recognition_thread_id,WM_EXIT,NULL,NULL);
			io_service_.stop();
			is_start = false;

			break;
		default:
			{
				print_by_lock("undefine message\n");
				logger.PrintLog("Server Core","undefine message");
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
	print_by_lock("server start\n");
	boost::asio::io_service::work work(io_service_);//使得就算没有任务了，run线程也不会退出
	boost::thread t(boost::bind(&Server::run,this));
	t.join();
}

void Server::accept_handler(const boost::system::error_code& ec,client_ptr& client)
{
	if(!ec)
	{

		sprintf_s(text,"client %d connect\n",client->id);
		print_by_lock(text);
		print_line_by_lock(client->cli_socket.remote_endpoint().address().to_string().c_str());
		client->start_recv_request();
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


PersonData Server::get_person_from_database(int person_id)
{
	PersonData p;
	p.id = person_id;
	if(person_id == -1)
	{
		return p;
	}
	char query[50];
	sprintf_s(query,"select * from person where Id=%d",person_id);
	MYSQL_RES* result = nullptr;


	if(!mysql_query(sql_con,query))
	{
		result = mysql_store_result(sql_con);
		if(nullptr != result)
		{
			MYSQL_ROW row = nullptr;
			row = mysql_fetch_row(result);
			int field_num = mysql_num_fields(result);
			MYSQL_FIELD* fds=  mysql_fetch_fields(result);

			if(nullptr != row)
			{
				for(int i = 0;i<field_num;i++)
				{
					if(strcmp(fds[i].name,"paper_id") == 0)
						p.paper_id = row[i];
					else if(strcmp(fds[i].name,"name") == 0)
						p.name = row[i];
					else if(strcmp(fds[i].name,"sex")==0)
						p.sex = atoi(row[i]);
				}
			}
		}
	}
	return p;
}

int Server::add_person_to_database(string paper_id,string name,char sex)
{
	int personId = -1;

	string query = "insert into person(paper_id,name,sex) values('" + paper_id +"','" +  name + "'," + (sex == 1?"1":"0") + ")";

	//query = AnsiToUTF8(query.c_str(),query.length());

	MYSQL_RES* result = nullptr;
	mysql_query(sql_con,query.c_str());

	query = "select LAST_INSERT_ID()";
	if(!mysql_query(sql_con,query.c_str()))
	{
		result = mysql_store_result(sql_con);
		if(nullptr != result)
		{
			MYSQL_ROW row = nullptr;
			row = mysql_fetch_row(result);

			personId = atoi(row[0]);

		}
	}
	if(personId == -1)
		personId = get_next_database_insert_id() - 1;
	return personId;
}

bool Server::update_person_in_database(int personId,string name,char sex)
{
	string query;
	//sprintf_s(query,"update person set name='%s',sex=%d where id=%d",name.c_str(),sex,personId);
	char id[12];
	sprintf_s(id,"%d",personId);
	query = "update person set name='" + name + "',sex=" + (sex == 1?"1":"0") + " where id=" + id;
	//query = AnsiToUTF8(query.c_str(),query.length());
	MYSQL_RES* result = nullptr;

	mysql_query(sql_con,query.c_str());

	return true;
}

int Server::get_next_database_insert_id()
{
	int personId;

	char query[40] = "show table status like 'person'";//查看表的维护信息，可以找到Auto_increment对应的字段的下一个值
	MYSQL_RES* result = nullptr;

	if(!mysql_query(sql_con,query))
	{
		result = mysql_store_result(sql_con);
		if(nullptr != result)
		{
			MYSQL_ROW row = nullptr;
			row = mysql_fetch_row(result);
			int field_num = mysql_num_fields(result);
			MYSQL_FIELD* fds=  mysql_fetch_fields(result);
			int i;

			for(i = 0;i<field_num;i++)
			{

				if(strcmp(fds[i].name,"Auto_increment") == 0)
					break;
			}
			personId = atoi(row[i]);
		}
	}
	return personId;
}

bool Server::delete_person_from_database(int personId)
{
	char query[50];
	sprintf_s(query,"delete from person where id =%d",1009);
	return true;
}
int Server::find_person_id_in_database(string paper_id)
{
	string query;
	query="select person.id from person where paper_id = '" + paper_id + "'";
	int id = -1;
	MYSQL_RES* result;
	if(!mysql_query(sql_con,query.c_str()))
	{
		result = mysql_store_result(sql_con);
		MYSQL_ROW row = nullptr;
		row = mysql_fetch_row(result);
		if(row == nullptr)
		{
			return -1;
		}
		id = atoi(row[0]);

	}
	mysql_free_result(result);
	return id > 0?id:-1;
}