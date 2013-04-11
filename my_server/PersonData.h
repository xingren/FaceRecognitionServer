<<<<<<< HEAD
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

=======
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

>>>>>>> ceaf66a127ebb6267244e49b2ed69def37b5572d
