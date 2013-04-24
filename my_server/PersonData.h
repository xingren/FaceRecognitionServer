#pragma once
#include "common.h"
class PersonData
{
public:
	PersonData(int sex,char* name,int len,int id = -1);
	PersonData()
	{
		name = "";
		sex = -1;

	}
	~PersonData(void);
	int id;
	int sex;
	std::string name;
	std::string paper_id;
	PersonData(char data[]);
	cv::Mat img;
};

