#pragma once
#include "common.h"
#include "client.h"
#include <mysql/mysql.h>
#include "PersonData.h"

//define thread message
#include "../message_types.h"
using namespace boost::asio;

#pragma comment(lib,"ws2_32")

static const int async_run_num = 4;
const int IPC_BUFF_SIZE = 1024*1024*10*async_run_num;
#define MAX_WAIT_TIME 1000*100
class Server
{
	
public:
	typedef boost::shared_ptr<Client> client_ptr;
	Server(boost::asio::io_service &io,unsigned int port,std::string = std::string());
	~Server();
	void start();
	void set_server_addr(unsigned int port,const std::string ip = std::string());
	bool acceptor_init();
	void start_next_accept();
	void stop_listen();
	void accept_handler(const boost::system::error_code &ec,client_ptr& client_ptr);
	boost::shared_ptr<Client> create_client();
	void Server::run();
	static int count;
	unsigned int WINAPI Core();
	void serialize_int(int val,char* out);
	void deserialize_int(uchar *in,int& val);

	//以下是数据库操作
	PersonData get_person_from_database(int client_id);
	int add_person_to_database(string paper_id,string name,char sex);//返回分配的person ID
	bool update_person_in_database(int personId,string name,char sex);
	int get_next_database_insert_id();
	bool delete_person_from_database(int personId);
	int find_person_id_in_database(string paper_id);
	string AnsiToUTF8(const char src[],int len);//将GBK编码转换成UTF8
	UINT WINAPI Test(VOID* p);
public:

	static char text[1000];

	io_service& io_service_;
	ip::tcp::acceptor acceptor;
	
	map<int,client_ptr> client_map;
	
	boost::container::vector<Client> reuse_client_can;
	ip::tcp::endpoint server_addr;
	bool is_start;
	
	
	//windows system part
	DWORD recognition_thread_id;
	HANDLE CoreStartEvent;
	HANDLE wrEvent;
	unsigned int core_thread_id;
	PROCESS_INFORMATION recognition_process_info;
	HANDLE core_thread_handle;
	DWORD recognition_main_thread_id;
	HANDLE wait_for_recognition_load;
	boost::thread service_run_thread;


	HANDLE hFileMapping;
	HANDLE hResultMapping;
	HANDLE hAddMapping;
	void* file_mapping_buf;
	void* rects_mapping_buf;
	void* add_mapping_buf;
	DWORD controler;
	//process and thread sync var

	boost::mutex client_map_mutex;
	
	HANDLE rects_mapping_mutex;//FaceRecognition将检测结果写入
	HANDLE file_mapping_mutex;
	HANDLE add_mapping_mutex;//my_server将添加的命令写入
	

	//mariadb part
	MYSQL* sql_con;
	//int in,out;
};