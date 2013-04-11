#pragma once
#include "common.h"
class PersonData
{
public:
	PersonData(int sex,char* name,int len,int id = -1);
	PersonData(){};
	~PersonData(void);
private:
	int id;
	int sex;
	std::string name;
	std::string school;
	cv::Mat img;
};

