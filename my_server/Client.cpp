#include"stdafx.h"
#include "Client.h"


char Client::text[1000];


Client::Client(io_service& io,unsigned long server_thread_id_):
	cli_socket(io),server_thread_id(server_thread_id_),
	timer_(io,boost::posix_time::seconds(MAX_ALIVE_SECOND))

{
	id = ++assign_client_id;
	recv_buf = recv_file = nullptr;
	isDestruct = false;
	sprintf(text,"Client %d is construct\n",id);
	
	logger.PrintLog("Client construct",text);
	print_by_lock(text);
}

Client::~Client(void)
{
	//if(src != nullptr)
	//{
	//	cvReleaseImage(&src);
	//	src = nullptr;
	//}
	//cout << "Client "<< id <<" Is destruct!!!" << endl;

	sprintf(text,"Client %d is disconstruct\n",id);
	logger.PrintLog("Client disconstruct",text);
	print_by_lock(text);

	if(recv_buf != nullptr) delete[] recv_buf;
	rects_vec.clear();
	personId_map.clear();
}
void Client::post_recv_request()
{
//	size_t read_size;
	boost::system::error_code ec;
	total_bytes = recv_bytes = 0;
	//cli_socket.async_read_some(boost::asio::buffer(recv_package.get_data(),recv_package.get_total_len()),boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	//transfer_all(),将buffer的全部填满才调用回调函数，transfer_at_least(size_t size)至少传size字节,
	async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_at_least(4),
		boost::bind(&Client::recv_request_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
}

bool Client::match_string(const char* src,int len1,const char* comp,int len2)
{
	int n = min(len1,len2);

	for(int i = 0;i < n;i++)
	{
		if(src[i] != comp[i])
			return false;
	}

	return true;
}


//接收一个包最多两次
void Client::recv_request_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	
	if(!ec)
	{
		sprintf_s(text,"recv msg from %s \n",cli_socket.remote_endpoint().address().to_string().c_str());
		print_by_lock(text);
		sprintf_s(text,"recv length %d\n",bytes_transferred);
		//td::cout << "content: " << recv_package.get_data() << std::endl;
		std::cout.write(recv_package.get_data(),bytes_transferred);

		memcpy(&total_bytes,recv_package.get_data(),4);
		if(total_bytes < 0 || total_bytes > bytes_transferred)
		{
			print_by_lock( "网络传输错误\n");//也有可能是客户端有BUG
			post_recv_request();
			return;
		}
		if(recv_bytes == 0 && total_bytes != bytes_transferred - 4)//第一次收，而且本次请求包没接收完
		{
			recv_bytes = bytes_transferred - 4;//可能会为零
			recv_buf = new uchar[total_bytes];
			memcpy(recv_buf,recv_package.body(),recv_bytes);

			recv_package.reCreateData(total_bytes-recv_bytes);

			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_exactly(total_bytes - recv_bytes),
				boost::bind(&Client::recv_request_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
			return;
		}
		else if(recv_bytes != 0)//第二次收
		{
			memcpy(recv_buf,recv_package.get_data(),bytes_transferred);
			
		}
		else//剩下的情况就是一次就收完了和第一收到零字节的数据
		{
			if(recv_buf == nullptr) //第一次收
			{
				recv_buf = new uchar[total_bytes];
				memcpy(recv_buf,recv_package.body(),total_bytes);

			}
			else
			{
				memcpy(recv_buf,recv_package.get_data(),bytes_transferred);
			}
			
		}

		char *content = (char*)recv_buf;
		if(match_string(content,bytes_transferred - recv_package.get_header_len(),FILE_type.first.c_str(),FILE_type.second))
		{
			//包的格式：包的类型 + 文件大小（4）+ 文件帧大小
			print_by_lock("prepare to recv file\n");
			//FILE 文件字节大小 每一帧的大小
			int fileLen;
			memcpy(&fileLen,content+FILE_type.second,4);

			sprintf(text,"total file byte num: %d\n",fileLen);
			print_by_lock(text);
			//	cout << "file frame bytes " << file_frame_bytes << endl;
			recv_package.reCreateData(fileLen);

			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_exactly(fileLen),
				boost::bind(&Client::recv_file_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
		}
		else if(match_string(content,bytes_transferred - recv_package.get_header_len(),ALIVE_type.first.c_str(),ALIVE_type.second))
		{
			reset_timer();
		}
		else if(match_string(content,total_bytes,MODIFY_type.first.c_str(),MODIFY_type.second))
		{
			//包格式:包的类型 + 将要发送的包的大小
			int len;
			memcpy(&len,content+MODIFY_type.second,4);
			recv_package.reCreateData(len);
			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_exactly(len),
				boost::bind(&Client::recv_modify_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));

		}
		else
		{
			post_recv_request();
		}
		if(recv_buf != nullptr)
		{
			delete[] recv_buf;
			recv_file = recv_buf = nullptr;
		}
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::recv_file_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	if(!ec)
	{
		personId_map.clear();
		uchar *image = new uchar[bytes_transferred+4];
		memcpy(image,&bytes_transferred,4);
		memcpy(image+4,recv_package.get_data(),bytes_transferred);

		PostThreadMessage(server_thread_id,WM_IMAGE,id,(LPARAM)image);

		post_recv_request();
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::recv_modify_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{

	//包的格式:paper_id的长度（2） + paper_id + 名字的长度（2）+名字+sex+CvRect

	if(!ec)
	{
		char* content = new char[bytes_transferred+4];
		memcpy(content,&bytes_transferred,4);
		memcpy(content+4,recv_package.get_data(),bytes_transferred);

		PostThreadMessage(server_thread_id,WM_MODIFY,id,(LPARAM)content);

		post_recv_request();
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::send_handler(void* p,const boost::system::error_code &ec,size_t bytes_transferred)
{
	if(ec)
	{
		print_by_lock("logger: send message to client failed");
		sprintf(text,"had send %d bytes\n",bytes_transferred);
		print_by_lock(text);
		//std::cout << "content: " << *message << std::endl;
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
	if(p != NULL)
		delete p;
}

void Client::send_dective_result(void *result,size_t size)
{
	
	char tmp[PACKAGE_TYPE_LEN] = "DETECT_RESULT";
	char *data = new char[size+PACKAGE_TYPE_LEN+4];
	//serialize_int(size+PACKAGE_TYPE_LEN,data);
	int len = size+PACKAGE_TYPE_LEN+4;
	memcpy(data,&len,4);
	memcpy(data + 4,tmp,PACKAGE_TYPE_LEN);
	if(size)
		memcpy(data + 4 + PACKAGE_TYPE_LEN,result,size);
	async_write(cli_socket,buffer(data,size + PACKAGE_TYPE_LEN + 4),
		boost::bind(&Client::send_handler,shared_from_this(),(void*)data,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	if(result != nullptr)
	{
		delete[] result;
	}
}

void Client::start_recv_request()
{
	//	timer_.async_wait(boost::bind(&Client::alive_timeout,shared_from_this(),boost::asio::placeholders::error));
	post_recv_request();
	//	Sleep(5000);
}

void Client::alive_write_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	if(ec)//can't send to client
	{
		boost::system::error_code ec2;
		cli_socket.shutdown(cli_socket.shutdown_both,ec2);
		cli_socket.close();
		PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
	}
}

void Client::wait_check_alive(const boost::system::error_code& ec)
{
	print_by_lock("shutdown the client\n");
	boost::system::error_code ec2;
	cli_socket.shutdown(cli_socket.shutdown_both,ec2);
	cli_socket.close();
	PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
}

void Client::alive_timeout(const boost::system::error_code& ec)
{
	if(ec == boost::asio::error::operation_aborted || ec)
		return;
	print_by_lock("alive with nothing to do logger the time\n");
	async_write(cli_socket,buffer("IS_ALIVE"),boost::bind(&Client::alive_write_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	timer_.async_wait(boost::bind(&Client::wait_check_alive,shared_from_this(),boost::asio::placeholders::error));
}