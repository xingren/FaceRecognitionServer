#include"stdafx.h"
#include "PersonData.h"


PersonData::PersonData(int sex,char* name,int len,int id)
{
	this->sex = sex;
	this->name = std::string(name,len);
	this->id = id;
}


PersonData::~PersonData(void)
{
}
