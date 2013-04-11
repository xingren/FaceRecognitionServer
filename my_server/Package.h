<<<<<<< HEAD
#include "common.h"
using namespace std;

#define HEADER_LEN 4
#define BODY_LEN 1020

//in file transfer,package data contain the header and body, header part indicate the ID of the file part
/*

包的格式：
包头加内容
在一般非文件传输的包，包头表示着内容的所占的字节数大小，内容则是type request_content
type有一般请求和文件传输请求

*/

class Package
{
public:


	Package(void);
	~Package(void);
	
	//char* create_package(size_t package_size);
	bool write_to_package_data(char *buf,size_t len);
	bool write_to_package_header(char *buf,size_t len);
	bool read_from_package_data(char *buf,size_t len);

	bool read_header(char *header,size_t len);
	unsigned long long read_header_int();
	bool read_body(char *body,size_t len);
	bool write_header(char *header,size_t);
	bool write_header(size_t len);
	bool write_body(char *body,size_t);
	void reCreateData(int body_len);
	char* header(){return data;}
	char* body(){return data+header_len;}
	char* get_data(){return data;}
	int get_total_len(){return body_len + header_len;}
	int get_header_len(){return header_len;}
	int get_body_len(){return body_len;}
private:
	char *data;
	int header_len;
	int body_len;
};

=======
#include "common.h"
using namespace std;

#define HEADER_LEN 4
#define BODY_LEN 1020

//in file transfer,package data contain the header and body, header part indicate the ID of the file part
/*

包的格式：
包头加内容
在一般非文件传输的包，包头表示着内容的所占的字节数大小，内容则是type request_content
type有一般请求和文件传输请求

*/

class Package
{
public:


	Package(void);
	~Package(void);
	
	//char* create_package(size_t package_size);
	bool write_to_package_data(char *buf,size_t len);
	bool write_to_package_header(char *buf,size_t len);
	bool read_from_package_data(char *buf,size_t len);

	bool read_header(char *header,size_t len);
	unsigned long long read_header_int();
	bool read_body(char *body,size_t len);
	bool write_header(char *header,size_t);
	bool write_header(size_t len);
	bool write_body(char *body,size_t);
	void reCreateData(int body_len);
	char* header(){return data;}
	char* body(){return data+header_len;}
	char* get_data(){return data;}
	int get_total_len(){return body_len + header_len;}
	int get_header_len(){return header_len;}
	int get_body_len(){return body_len;}
private:
	char *data;
	int header_len;
	int body_len;
};

>>>>>>> ceaf66a127ebb6267244e49b2ed69def37b5572d
