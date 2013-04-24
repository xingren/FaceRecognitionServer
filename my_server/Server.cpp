#include"stdafx.h"
#include"Server.h"

Server::Server(io_service &io,unsigned int port,std::string ip):io_service_(io),acceptor(io),is_start(false),sql_con(nullptr)
{

	controler = 0;
	char* command_line = GetCommandLine();
	if(strcmp(command_line,"") != 0)
	{
		stringstream sstr(command_line);
		string str;
		//printf("%s\n",command_line);
		cout << "command line:" << command_line << endl;
		sstr >> str;
		sstr >> controler;
		cout << "controler:" << controler << endl;
		
	}
	else
	{
		printf("no common line\n");
	}

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


	//进程通信，线程同步操作初始化
	//share_mem_mutex = CreateMutex(NULL,FALSE,"share-mem-mutex");
	/*share_mem_full = CreateSemaphore(NULL,0,async_run_num,NULL);
	share_mem_empty = CreateSemaphore(NULL,async_run_num,async_run_num,NULL);*/
	hFileMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"file-mapping");
	hResultMapping = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,IPC_BUFF_SIZE,"result-mapping");
	file_mapping_buf = (char *)MapViewOfFile(hFileMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_buf = MapViewOfFile(hResultMapping,FILE_MAP_ALL_ACCESS,0,0,0);
	rects_mapping_mutex = CreateMutex(NULL,FALSE,"Global\\rects_mapping_mutex");//本线程拥有mutex，其它线程无法拥有
	file_mapping_mutex = CreateMutex(NULL,FALSE,"Global\\file_mapping_mutex");
	wait_for_recognition_load = CreateEvent(NULL,FALSE,FALSE,"recognition_load");
//	file_mapping_op_finish = CreateEvent(NULL,FALSE,TRUE,"file_mapping_op_finish");

	if(nullptr == rects_mapping_mutex)
	{
		printf("error in CreateMutext,rects_mapping_mutex:%ld\n",GetLastError());
		return;
	}
	if(nullptr == file_mapping_mutex)
	{
		printf("error in CreateMutext,file_mapping_mutex:%ld\n",GetLastError());
		return;
	}
	if(nullptr == file_mapping_buf )
	{
		printf("error in MapViewOffFile with file_mapping_buf: %ld\n",GetLastError());
		return;
	}
	if(nullptr == rects_mapping_buf)
	{
		printf("error in MapViewOffFile with rects_mapping_buf: %ld\n",GetLastError());
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

	if(!CreateProcess(NULL,command,NULL,NULL,NULL,CREATE_NEW_CONSOLE,NULL,NULL,&si,&recognition_process_info))
	{
		printf("failed to start FaceRecognition Process\n");
		return;
	}
	Sleep(1000);
	CloseHandle(CoreStartEvent);
	
	WaitForSingleObject(wait_for_recognition_load,INFINITE);
	
	std::cout << "begin init accept" << std::endl;
	
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

	CloseHandle(file_mapping_mutex);
	CloseHandle(rects_mapping_mutex);
	CloseHandle(wait_for_recognition_load);
}
bool Server::init()
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
				WaitForSingleObject(file_mapping_mutex,INFINITE);
				memcpy(file_mapping_buf,recv_buf,total_bytes+4);
				ReleaseMutex(file_mapping_mutex);
				int count = 0;
				while(count < 10 && PostThreadMessage(recognition_thread_id,WM_IMAGE,clientId,NULL) == 0)
				{
					printf("error in post thread,tell recognition recieve image:%ld\n",GetLastError());
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
					cout << "client " << clientId << " MODIFY request has wrong package" << endl;
					delete[] ptr;
					break;
				}
				paper_id.append(ptr+4+2,paper_id_len);
				name.append(ptr+4+2+paper_id_len+2,nameLen);
				sex = ptr[4+2+paper_id_len+2+nameLen];

				MyRect rect;
				memcpy(&rect,ptr+4+2+nameLen+2+paper_id_len+1,sizeof(CvRect));
				cout << "CvRect: " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;
				int personId;
				client_ptr client;
				client_map_mutex.lock();

				map<int,client_ptr>::iterator client_iter = client_map.find(clientId);
				
				if(client_iter == client_map.end())
				{
					cout << "client had been removed from client_map !!" << endl;
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
					cout << "not exist this CvRect: " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;
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
					WaitForSingleObject(rects_mapping_mutex,INFINITE);
					memcpy(rects_mapping_buf,&personId,4);
					memcpy((char *)rects_mapping_buf + 4,&rect,sizeof(rect));
					PostThreadMessage(recognition_thread_id,WM_ADD_FACE,clientId,NULL);
					ReleaseMutex(rects_mapping_mutex);
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
						WaitForSingleObject(rects_mapping_mutex,INFINITE);
						memcpy(rects_mapping_buf,&tmpId,4);
						memcpy((char *)rects_mapping_buf + 4,&rect,sizeof(rect));
						PostThreadMessage(recognition_thread_id,WM_ADD_FACE,clientId,NULL);
						ReleaseMutex(rects_mapping_mutex);
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
				printf("Get Recognition ID\n");
			}
			break;
		case WM_RESULT_DECTIVE://从FaceRecognition传送过来的检测消息
			{
				int clientId = msg.wParam;
				cout << "WM_RESULT_DECTIVE with client_id "<< clientId << endl;
				int size = msg.lParam;
				char *result = nullptr; //如果有在堆上申请内存，最终会在Client::send_dective_result内释放
				int len = 0;
				if(size != 0)
				{
					result = new char[(sizeof(CvRect)+4)*size];
					WaitForSingleObject(rects_mapping_mutex,INFINITE);
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
						cout << "client had been removed from client_map !!" << endl;
						delete[] result;
						break;;
					}
					client_ptr client = client_iter->second;
					for(int i = 0;i < size ;i++)
					{
						memcpy(&person_id,result + i*(sizeof(CvRect) + 4),4);
						memcpy(&rect,result + i*(sizeof(CvRect)+4)+4,sizeof(CvRect));

						cout << "CvRect: " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;

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
						printf("the %d rect is:%d,%d,%d,%d\n",i,rect.x,rect.y,rect.width,rect.height);
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
						printf("%d the sex is %d,%d\n",i,persons.at(i).sex,char(*(ptr+2+nlen)));
						ptr += 2+ persons.at(i).paper_id.length() + 2 + nlen + 1 + sizeof(CvRect);
					}

				}
				
				client_map_mutex.lock();
				map<int,client_ptr>::iterator iter = client_map.find(clientId);
				if(iter != client_map.end())
				{
					cout << "send recognition result to client: " << clientId  << "result length " << len << endl;
					iter->second->send_dective_result(result,len+4);
				}
				client_map_mutex.unlock();
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
				client_map_mutex.lock();
				
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
		
		std::cout << "logger: client "<< client->id <<" connect" << std::endl;
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


PersonData Server::get_person_from_database(int person_id)
{
	PersonData p;
	p.id = person_id;
	if(person_id == -1)
	{
		return p;
	}
	char query[50];
	sprintf(query,"select * from person where Id=%d",person_id);
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
	//sprintf(query,"update person set name='%s',sex=%d where id=%d",name.c_str(),sex,personId);
	char id[12];
	sprintf(id,"%d",personId);
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
	sprintf(query,"delete from person where id =%d",1009);
	return true;
}

string Server::AnsiToUTF8(const char src[],int len)
{
	int size = MultiByteToWideChar(CP_ACP,0,src,len,NULL,0);
	wchar_t *dst = new wchar_t[size];
	if(!MultiByteToWideChar(CP_ACP,0,src,len,dst,size))
	{
		delete[] dst;
		return "";
	}

	//unicode to utf8
	string str;
	char s[6];
	for(int i = 0;i < size;i++)
	{
		if(dst[i] >=0 && dst[i] <0x80)
		{
			str.push_back(dst[i]);
		}
		else if(dst[i] >= 0x80 && dst[i] < 0x800)
		{
			s[1] = dst[i] & 0x3f | 0x80;
			s[0] = (dst[i] >> 6) | 0xc0;
			str.append(s,2);
		}
		else if(dst[i] >= 0x800 && dst[i] < 0x10000)
		{
			s[2] = dst[i] & 0x3f | 0x80;
			s[1] = (dst[i] >> 6) & 0x3f | 0x80;
			s[0] = (dst[i] >> 12) & 0x3f | 0xe0;
			str.append(s,3);
		}
		else if(dst[i] >= 0x10000 && dst[i] < 0x200000)
		{}
		else if(dst[i] >= 0x200000 && dst[i] < 0x4000000)
		{}
		else if(dst[i] >= 0x4000000 && dst[i] < 0x80000000)
		{}
		else //error
		{
		}
	}
	delete[] dst;
	return str;
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