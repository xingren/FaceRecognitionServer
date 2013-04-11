<<<<<<< HEAD
#ifndef COMMON_H
#define COMMON_H

#ifdef WIN32
//#include<Windows.h>


#elif LINUX

#endif


//boost lib
#include <boost/asio.hpp>
#include<boost/bind.hpp>
#include<boost/container/vector.hpp>
#include<boost/container/list.hpp>
#include<boost/make_shared.hpp>
#include<boost/thread/thread.hpp>
#include <boost/enable_shared_from_this.hpp>
//opencv lib
#include<opencv/cv.h>
#include<opencv/highgui.h>
#include<opencv/cxcore.h>
#include <opencv2/core/core.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <opencv2/highgui/highgui.hpp>

//stl
#include <vector>


#define DEFAULT_IP "127.0.0.1"
#define PORT 3598
#define DEFAULT_IP_VERSION boost::asio::ip::tcp::v4()

const int THREAD_RUN_NUM = 4;
const int DEFAULT_ASYNC_ACCEPT = 1;
const int MALE = 0;
const int FEMALE = 1;

static int client_id = 0;//for debug
template<class T>
void deserialze(char* p,size_t len,T& val)
{
	size_t size = sizeof(T);
	int n = min(len,size);
	memcpy(&val,p,n);
}
#endif

=======
#ifndef COMMON_H
#define COMMON_H

#ifdef WIN32
//#include<Windows.h>


#elif LINUX

#endif


//boost lib
#include <boost/asio.hpp>
#include<boost/bind.hpp>
#include<boost/container/vector.hpp>
#include<boost/container/list.hpp>
#include<boost/make_shared.hpp>
#include<boost/thread/thread.hpp>
#include <boost/enable_shared_from_this.hpp>
//opencv lib
#include<opencv/cv.h>
#include<opencv/highgui.h>
#include<opencv/cxcore.h>
#include <opencv2/core/core.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <opencv2/highgui/highgui.hpp>

//stl
#include <vector>


#define DEFAULT_IP "127.0.0.1"
#define PORT 3598
#define DEFAULT_IP_VERSION boost::asio::ip::tcp::v4()

const int THREAD_RUN_NUM = 4;
const int DEFAULT_ASYNC_ACCEPT = 1;
const int MALE = 0;
const int FEMALE = 1;

static int client_id = 0;//for debug
template<class T>
void deserialze(char* p,size_t len,T& val)
{
	size_t size = sizeof(T);
	int n = min(len,size);
	memcpy(&val,p,n);
}
#endif

>>>>>>> ceaf66a127ebb6267244e49b2ed69def37b5572d
