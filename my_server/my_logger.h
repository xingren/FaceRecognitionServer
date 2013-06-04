#pragma once
#include <iostream>
#include <fstream>
#include<boost/thread/thread.hpp>
using namespace std;

class my_logger
{
private:
	fstream log_file;
	boost::mutex mutex;
public:
	my_logger(void);
	my_logger(char *filename);

	bool PrintLog(string title,string content);

	~my_logger(void);
};

