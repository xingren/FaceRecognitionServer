#pragma once
#include "common.h"
#include "Package.h"
#include<boost/array.hpp>
#include<stdio.h>
#include<boost/date_time/posix_time/posix_time.hpp>

using namespace boost::asio;
using namespace cv;
#define FILE_INFO 0 //文件信息
#define COMMAND 1 //命令


#include "../message_types.h"
using namespace std;


class MyRect:public CvRect//继承CvRect，并且重载了<，==,()，用于map中
{
public:
	bool operator < (const MyRect& a) const
	{
		if(a.x < this->x)
			return true;
		else if(a.x == this->x)
		{
			if(a.y < this->y)
				return true;
			else if(a.y == this->y)
			{
				if(a.width < this->width)
					return true;
				else if(a.width == this->width)
				{
					return a.height < this->height;
				}
				return false;
			}
			return false;
		}
		return false;
	}
	bool operator ==(const MyRect& a)
	{
		return (a.x == this->x && a.y == this->y && a.width == this->width && a.height == this->height );
	}
	bool operator()(const MyRect&a,const MyRect&b)const
	{
		if(a.x < b.x)
			return true;
		else if(a.x == b.x)
		{
			if(a.y < b.y)
				return true;
			else if(a.y == b.y)
			{
				if(a.width < b.width)
					return true;
				else if(a.width == b.width)
				{
					return a.height < b.height;
				}
				return false;
			}
			return false;
		}
		return false;
	}
};
	typedef pair<string,int> Type;//包的类型，第二个模板参数表示第一个的字符串长度
	static Type FILE_type("FILE",4) ;
	static Type MODIFY_type("MODIFY",6);
	static Type ALIVE_type("ALIVE",5);
class Client:public boost::enable_shared_from_this<Client>
{
	static const int MAX_ALIVE_SECOND = 50;
	static const int PACKAGE_TYPE_LEN = 20;

	

public:

	Client(io_service &io,unsigned long server_thread_id);
	~Client(void);
	//	Client(const Client& cli);
	void set_thread_id(int thread_id){in_thread_id = thread_id;}
	void post_recv();
	void recv_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void recv_file_handler(const boost::system::error_code& ec,size_t bytes_transferred);//use to recv file
	void recv_modify_handler(const boost::system::error_code& ec,size_t bytes_transferred);

	void send_dective_result(void *rects,size_t size);
	void start();
	//void send_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void send_handler(void *p,const boost::system::error_code& ec,size_t bytes_transferred);
	//void send_handler(boost::shared_ptr<std::string> message,const boost::system::error_code& ec,size_t bytes_transferred);
	ip::tcp::socket& get_socket (){return cli_socket;}
	bool match_string(const char* src,int len1,const char* comp,int len2);
	void alive_timeout(const boost::system::error_code& ec);
	void alive_write_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void wait_check_alive(const boost::system::error_code& ec);
	void reset_timer()
	{
		timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(MAX_ALIVE_SECOND));
		timer_.async_wait(boost::bind(&Client::alive_timeout,shared_from_this(),boost::asio::placeholders::error));
	}
public:
	int in_thread_id;//for debug
	int id; 
	int isDestruct;
	ip::tcp::socket cli_socket;
	map<MyRect,int> personId_map;//将检测出来的人脸和数据库中的人的ID关联起来

	int recv_bytes;//到目前总共收到多少字节,只用于接收请求包
	int total_bytes;//包总共有多少字节，只用于接收请求包
	int file_frame_bytes;
	int recv_file_size;
	uchar* recv_file;
	uchar* recv_buf;
	FILE *out;//for debug
	bool one_frame_done;//for judging if recieve one frame
	void serialize_int(int val,char *out);
	//char* add_buf;




	//shared_ptr<uchar> f;

	//opencv part
	IplImage* img; //client's face image
	cv::Mat img_mat; //face image mat
	vector<CvRect> rects_vec;
	CvRect* rects;
	int nRects;
	Package recv_package;


	//windows part
	DWORD server_thread_id;


	//timer,to decide the client wether in alive
	boost::asio::deadline_timer timer_;
};

