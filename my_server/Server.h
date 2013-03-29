#pragma once
#include "common.h"
#include "client.h"
#include <mysql/mysql.h>

//define thread message
#include "../message_types.h"
using namespace boost::asio;

#pragma comment(lib,"ws2_32")

static const int async_run_num = 4;
const int IPC_BUFF_SIZE = 1024*1024*10*async_run_num;

class Server
{
	
public:
	typedef boost::shared_ptr<Client> client_ptr;
	Server(boost::asio::io_service &io,unsigned int port,std::string = std::string());
	~Server();
	void start();
	void set_server_addr(unsigned int port,const std::string ip = std::string());
	bool init();
	void start_next_accept();
	void stop_listen();
	void accept_handler(const boost::system::error_code &ec,client_ptr& client_ptr);
	boost::shared_ptr<Client> create_client();
	void Server::run();
	static int count;
	unsigned int WINAPI Core();
	void serialize_int(int val,char* out);
	void deserialize_int(char *in,int& val);
	unsigned int WINAPI write_share_file();
	unsigned int WINAPI read_result();

public:
	io_service& io_service_;
	ip::tcp::acceptor acceptor;
	
	map<int,client_ptr> client_map;
	boost::container::vector<Client> reuse_client_can;
	ip::tcp::endpoint server_addr;
	bool is_start;
	boost::thread service_run_thread;
	
	//windows system part
	DWORD recognition_thread_id;
	HANDLE CoreStartEvent;
	HANDLE wrEvent;
	unsigned int core_thread_id;
	PROCESS_INFORMATION recognition_process_info;
	HANDLE core_thread_handle;
	DWORD recognition_main_thread_id;
	HANDLE write_share_mem_thread;
	HANDLE read_share_mem_thread;
	unsigned int write_share_mem_thread_id;
	unsigned int read_share_mem_thread_id;
	HANDLE hFileMapping;
	HANDLE hResultMapping;
	void* file_mapping_buf;
	void* rects_mapping_buf;

	//process and thread sync var
	HANDLE file_mapping_op_finish;
	boost::mutex cli_map_mutex;
	//HANDLE share_mem_mutex;
	HANDLE rects_mapping_mutex;
	//mariadb part
	MYSQL* sql_con;
	//int in,out;
};