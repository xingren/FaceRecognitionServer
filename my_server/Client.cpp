#include"stdafx.h"
#include "Client.h"



Client::Client(io_service& io,unsigned long server_thread_id_):
	cli_socket(io),server_thread_id(server_thread_id_),
	timer_(io,boost::posix_time::seconds(MAX_ALIVE_SECOND))

{
	id = ++assign_client_id;
	recv_buf = recv_file = nullptr;
	isDestruct = false;
	
	cout << "Client "<< id <<" Is construct!!!" << endl;
}

Client::~Client(void)
{
	//if(src != nullptr)
	//{
	//	cvReleaseImage(&src);
	//	src = nullptr;
	//}
	cout << "Client "<< id <<" Is destruct!!!" << endl;
	if(recv_buf != nullptr) delete[] recv_buf;
	rects_vec.clear();
	personId_map.clear();
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
//	size_t read_size;
	boost::system::error_code ec;
	total_bytes = recv_bytes = 0;
	//cli_socket.async_read_some(boost::asio::buffer(recv_package.get_data(),recv_package.get_total_len()),boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	//transfer_all(),��buffer��ȫ�������ŵ��ûص�������transfer_at_least(size_t size)���ٴ�size�ֽ�,
	async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_at_least(4),
		boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
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


//����һ�����������
void Client::recv_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{
	
	if(!ec)
	{
		std::cout << "recv msg from " << cli_socket.remote_endpoint().address() << std::endl;
		std::cout << "recv length " << bytes_transferred << std::endl;
		//td::cout << "content: " << recv_package.get_data() << std::endl;
		std::cout.write(recv_package.get_data(),bytes_transferred);

		memcpy(&total_bytes,recv_package.get_data(),4);
		if(total_bytes < 0 || total_bytes > bytes_transferred)
		{
			cout << "���紫�����" << endl;//Ҳ�п����ǿͻ�����BUG
			post_recv();
			return;
		}
		if(recv_bytes == 0 && total_bytes != bytes_transferred - 4)//��һ���գ����ұ��������û������
		{
			recv_bytes = bytes_transferred - 4;//���ܻ�Ϊ��
			recv_buf = new uchar[total_bytes];
			memcpy(recv_buf,recv_package.body(),recv_bytes);

			recv_package.reCreateData(total_bytes-recv_bytes);

			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_exactly(total_bytes - recv_bytes),
				boost::bind(&Client::recv_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
			return;
		}
		else if(recv_bytes != 0)//�ڶ�����
		{
			memcpy(recv_buf,recv_package.get_data(),bytes_transferred);
			
		}
		else//ʣ�µ��������һ�ξ������˺͵�һ�յ����ֽڵ�����
		{
			if(recv_buf == nullptr) //��һ����
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
			//���ĸ�ʽ���������� + �ļ���С��4��+ �ļ�֡��С
			std::cout << "prepare to recv file" << endl;
			//FILE �ļ��ֽڴ�С ÿһ֡�Ĵ�С
			int fileLen;
			memcpy(&fileLen,content+FILE_type.second,4);

			cout << "total file byte num: " << fileLen << endl;
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
			//����ʽ:�������� + ��Ҫ���͵İ��Ĵ�С
			int len;
			memcpy(&len,content+MODIFY_type.second,4);
			recv_package.reCreateData(len);
			async_read(cli_socket,buffer(recv_package.get_data(),recv_package.get_total_len()),transfer_exactly(len),
				boost::bind(&Client::recv_modify_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));

		}
		else
		{
			post_recv();
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
		uchar *image = new uchar[bytes_transferred+4];
		memcpy(image,&bytes_transferred,4);
		memcpy(image+4,recv_package.get_data(),bytes_transferred);

		PostThreadMessage(server_thread_id,WM_IMAGE,id,(LPARAM)image);

		post_recv();
	}
	else
	{
		std::cout << boost::system::system_error(ec).what() << std::endl;
	}
}

void Client::recv_modify_handler(const boost::system::error_code& ec,size_t bytes_transferred)
{

	//���ĸ�ʽ:���ֵĳ��ȣ�2��+����+sex+CvRect

	if(!ec)
	{
		char* content = new char[bytes_transferred+4];
		memcpy(content,&bytes_transferred,4);
		memcpy(content+4,recv_package.get_data(),bytes_transferred);

		PostThreadMessage(server_thread_id,WM_MODIFY,id,(LPARAM)content);

		post_recv();
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
		std::cout << "logger: send message to client failed" << std::endl;
		std::cout << "had send " << bytes_transferred << " bytes" << std::endl;
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
		PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
	}
}

void Client::wait_check_alive(const boost::system::error_code& ec)
{
	cout << "shutdown the client" << endl;
	boost::system::error_code ec2;
	cli_socket.shutdown(cli_socket.shutdown_both,ec2);
	cli_socket.close();
	PostThreadMessage(server_thread_id,WM_CLIENT_DISCONNECT,id,NULL);
}

void Client::alive_timeout(const boost::system::error_code& ec)
{
	if(ec == boost::asio::error::operation_aborted || ec)
		return;
	cout << "alive with nothing to do logger the time" << endl;
	async_write(cli_socket,buffer("IS_ALIVE"),boost::bind(&Client::alive_write_handler,shared_from_this(),boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
	timer_.async_wait(boost::bind(&Client::wait_check_alive,shared_from_this(),boost::asio::placeholders::error));
}