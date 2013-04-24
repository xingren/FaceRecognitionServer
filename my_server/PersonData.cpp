#include"stdafx.h"
#include "PersonData.h"


PersonData::PersonData(int sex,char* name,int len,int id)
{
	this->sex = sex;
	this->name = std::string(name,len);
	this->id = id;
}
PersonData::PersonData(char data[]):paper_id(""),name("")
{
	//paper_id的长度(2) + paper_id + name的长度(2) + name + sex(1) + CvRect 
	int len;
	memcpy(&len,data,2);
	paper_id.append(data+2,len);
	memcpy(&len,data+2+len,2);
	name.append(data+2+paper_id.length() + 2);
	memcpy(&sex,data+2 + paper_id.length() + 2 + name.length(),1);


}

PersonData::~PersonData(void)
{
}
