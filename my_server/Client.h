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
	void recv_rects_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void recv_person_data_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void send_dective_result(void *rects,size_t size);
	void start();
	//void send_handler(const boost::system::error_code& ec,size_t bytes_transferred);
	void send_handler(void *p,const boost::system::error_code& ec,size_t bytes_transferred);
	//void send_handler(boost::shared_ptr<std::string> message,const boost::system::error_code& ec,size_t bytes_transferred);
	ip::tcp::socket& get_socket (){return cli_socket;}
	bool match_string(char* src,int len1,char* comp,int len2);
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
	
	ip::tcp::socket cli_socket;
	int recv_bytes;//用来到目前总共收到多少字节的文件
	int total_bytes;//文件总共有多少字节
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

