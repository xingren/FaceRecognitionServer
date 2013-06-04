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
#include "my_logger.h"

#define DEFAULT_IP "127.0.0.1"
#define PORT 3598
#define DEFAULT_IP_VERSION boost::asio::ip::tcp::v4()

const int THREAD_RUN_NUM = 4;
const int DEFAULT_ASYNC_ACCEPT = 1;
const int MALE = 0;
const int FEMALE = 1;
const int MAX_NAME_LEN = 256;

static int assign_client_id = 0;//for debug
static my_logger logger;

static boost::mutex print_mutex;

static void print_line_by_lock(const char text[])
{
	print_mutex.lock();
	cout << text << endl;
	print_mutex.unlock();
}

static void print_by_lock(const char text[])
{
	print_mutex.lock();
	cout << text;
	print_mutex.unlock();
}

#endif

