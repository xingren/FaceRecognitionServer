#include "stdafx.h"
#include "my_logger.h"
#include <boost/date_time/posix_time/posix_time.hpp> 

my_logger::my_logger(void)
{
	log_file.open("log.txt",ios::out);
}


my_logger::~my_logger(void)
{
	log_file.close();
}

bool my_logger::PrintLog(string title,string content)
{
	std::string strTime = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time()); 	
	mutex.lock();
	log_file << strTime << ": " << title << ": " << content << endl;
	log_file.flush();
	mutex.unlock();

	return true;
}