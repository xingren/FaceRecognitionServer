#include"stdafx.h"
#include "Client.h"



Client::Client(io_service& io,unsigned long server_thread_id_):cli_socket(io),server_thread_id(server_thread_id_),timer_(io,boost::posix_time::seconds(MAX_ALIVE_SECOND))
{
	id = ++client_id;
	recv_file = nullptr;

}

Client::~Client(void)
{
	//if(src != nullptr)
	//{
	//	cvReleaseImage(&src);
	//	src = nullptr;
	//}
	if(recv_buf != nullptr) delete[] recv_buf;
	rects_vec.clear();
}


void Client::serialize_int(int val,char *out)
{
	out[0] = out[1] = out[2] = out[3] = 0;
	out[0] = (val & 255);
	out[1] = (val >> 8) & 255;
	out[2] = (val >> 16)&255;
	out[3] = (val >> 24)&255;
}

void Client::post_recv()
{
	size_t read_size;
	boost::system::error_code ec;
	cli_socket.async_read_some(boost::asio::buffer(recv_package.get_data(),recv_package.get_total_len()),boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
}

bool Client::match_string(char* src,int len1,char* comp,int len2)
{
	int n = min(len1,len2);

	for(int i = 0;i < n;i++)
	{
		if(src[i] != comp[i])
			return false;
	}

	return true;
}

void Client::recv_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	if(!ec)
	{
		std::cout << "recv msg from " << cli_socket.remote_endpoint().address() << std::endl;
		std::cout << "recv length " << bytes_transferred << std::endl;
		std::cout << "content: " << recv_package.get_data() << std::endl;

		char *content = recv_package.body();
		if(match_string(content,bytes_transferred - recv_package.get_header_len(),"FILE",4))
		{
			std::cout << "prepare to recv file" << endl;
			//FILE 文件字节大小 每一帧的大小
			recv_bytes = total_bytes = 0;
			deserialze(content + 4,4,total_bytes);
			//deserialze(content + 8,4,file_frame_bytes);
			cout << "total bytes " << total_bytes << endl;
			//	cout << "file frame bytes " << file_frame_bytes << endl;
			recv_package.reCreateData(total_bytes);

			recv_buf = new uchar[total_bytes + 4];//前4字节用以保存文件大小
			serialize_int(total_bytes,(char*)recv_buf);
			recv_file = recv_buf + 4;

			//	cli_socket.async_read_some(buffer(recv_package.get_data(),recv_package.get_total_len()),
			//		boost::bind(&Client::recv_file_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),
				boost::bind(&Client::recv_file_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
		}
		else if(match_string(content,bytes_transferred - recv_package.get_header_len(),"ALIVE",5))
		{
			reset_timer();
		}
		else if(match_string(content,bytes_transferred - recv_package.get_header_len(),"MODIFY",6))
		{
			//包的格式:包的长度（4）+名字的长度（2）+名字+sex+CvRect

		}
		else
		{
			cli_socket.async_read_some(boost::asio::buffer(this->recv_package.get_data(),recv_package.get_total_len()),
				boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
		}
		/*boost::shared_ptr<std::string> pstr(new std::string("this is server\n"));
		cli_socket.async_write_some(buffer(*pstr),boost::bind(&Client::send_handler,shared_from_this(),pstr,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));*/
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
		memcpy(recv_file,recv_package.body(),bytes_transferred);
		int bRet,count = 0;
		while(count < 10 && (bRet = PostThreadMessage(server_thread_id,WM_IMAGE,this->id,(LPARAM)recv_buf) )<= 0)
		{
			cout << "failed in PostThreadMessage in recv_file_handler" << endl;
			DWORD e = GetLastError();
			LPVOID lpMsgBuf;
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,e,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&lpMsgBuf,0,NULL);
			char tmp[200];
			sprintf(tmp,"PostThreadMessage failed with error %d: %s",e,lpMsgBuf);
			cout << tmp << endl;
		}
		if(count == 10)
			cout << "failed to Post Image to Server::Core thread" << endl;
		cout << "file recv done" << endl;

		cli_socket.async_read_some(buffer(recv_package.get_data(),recv_package.get_total_len()),
			boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::recv_rects_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	if(!ec)
	{
		CvRect*p = (CvRect*)recv_package.get_data() + 4;
		memcpy(rects,p,nRects*sizeof(CvRect));

		one_frame_done = false;
		recv_bytes = 0;
		async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_at_least(4),
			boost::bind(&Client::recv_person_data_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::recv_person_data_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	static char* buf;
	if(!ec)
	{
		char *p = recv_package.get_data();
		if(0 == recv_bytes)//第一次收当前帧
		{
			deserialze(p,4,total_bytes);
			buf = new char[sizeof(nRects) + nRects*sizeof(CvRect) + total_bytes];//格式为个数+CvRect数组数据+“sex1 nam sex2 name2 .....”
			memcpy(buf,&nRects,sizeof(nRects));
			memcpy(buf+sizeof(nRects),rects,nRects*sizeof(CvRect));
			if(bytes_transferred - 4  == total_bytes) //当前帧接收完毕
			{
				memcpy(buf+sizeof(nRects)+nRects*sizeof(CvRect),p+4,total_bytes);
				PostThreadMessage(server_thread_id,WM_ADD_FACE,this->id,(LPARAM)buf);
				post_recv();
			}
			else
			{
				recv_bytes = bytes_transferred-4;
				memcpy(buf+sizeof(nRects)+nRects*sizeof(CvRect),p+4,bytes_transferred-4);
				async_read(cli_socket,buffer(recv_package.get_data(),total_bytes-bytes_transferred-4),
					boost::bind(&Client::recv_person_data_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
			}
		}
		else
		{
			memcpy(buf+sizeof(nRects)+nRects*sizeof(CvRect)+recv_bytes,recv_package.get_data(),bytes_transferred);

			PostThreadMessage(server_thread_id,WM_ADD_FACE,this->id,(LPARAM)buf);
			buf = NULL;
			post_recv();
		}
	}
	else //要释放上面buf所创建的内存
	{
		if(buf)
			delete buf;
		std::cout << boost::system::system_error(ec).what() << std::endl;

	}
}

void Client::send_handler(void* p,const boost::system::error_code &ec,size_t bytes_transferred)
{
	if(ec)
	{
		std::cout << "logger: send message to client failed" << std::endl;
		std::cout << "had send " << bytes_transferred << " bytes" << std::endl;
		//std::cout << "content: " << *message << std::endl;
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
	if(size != 0)
	{
		memcpy(data + 4 + PACKAGE_TYPE_LEN,result,size);
	}
	async_write(cli_socket,buffer(data,size + PACKAGE_TYPE_LEN + 4),
		boost::bind(&Client::send_handler,shared_from_this(),(void*)data,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	if(result != nullptr)
	{
		delete[] result;
	}
}

void Client::start()
{
	//	timer_.async_wait(boost::bind(&Client::alive_timeout,shared_from_this(),boost::asio::placeholders::error));
	post_recv();
	//	Sleep(5000);
}

void Client::alive_write_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	if(ec)//can't send to client
	{
		boost::system::error_code ec2;
		cli_socket.shutdown(cli_socket.shutdown_both,ec2);
		cli_socket.close();
		PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,client_id,NULL);
	}
}

void Client::wait_check_alive(const boost::system::error_code& ec)
{
	cout << "shutdown the client" << endl;
	boost::system::error_code ec2;
	cli_socket.shutdown(cli_socket.shutdown_both,ec2);
	cli_socket.close();
	PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,client_id,NULL);
}

void Client::alive_timeout(const boost::system::error_code& ec)
{
	if(ec == boost::asio::error::operation_aborted || ec)
		return;
	cout << "alive with nothing to do logger the time" << endl;
	async_write(cli_socket,buffer("IS_ALIVE"),boost::bind(&Client::alive_write_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	timer_.async_wait(boost::bind(&Client::wait_check_alive,shared_from_this(),boost::asio::placeholders::error));
}