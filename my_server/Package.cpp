#include"stdafx.h"
#include "Package.h"


Package::Package(void)
{
	data = new char[HEADER_LEN + BODY_LEN];
	header_len = HEADER_LEN;
	body_len = BODY_LEN;
}


Package::~Package(void)
{
	delete[] data;
}

void Package::reCreateData(int body_len)
{
	if(body_len == this->body_len) return ;
	delete[] data;
	this->body_len = body_len;
	data = new char[HEADER_LEN + this->body_len];
}

bool Package::read_body(char *body,size_t len)
{
	if(len < body_len)
	   return false;
	memcpy(body,data+header_len,body_len);
	return true;
}
unsigned long long Package::read_header_int()
{
	long long len = 0;
	
	for(int i = 0;i<HEADER_LEN;i++)
	{
		len |= (data[i] << (i*8));
	}
	
	return len;
}
bool Package::read_header(char *header,size_t len)
{
	if(len < header_len)
		return false;

	memcpy(header,data,header_len);
	return true;
}

bool Package::write_header(char *header,size_t len)
{
	if(len > header_len) return false;

	memcpy(data,header,len);
	return true;
}

bool Package::write_header(size_t len)
{
	char tmp[4];
	sprintf(tmp,"%4d",len);
	write_header(tmp,4);
	return true;
}

bool Package::write_body(char *body,size_t len)
{
	if(len > body_len)
		reCreateData(len);

	memcpy(data+header_len,body,len);
	return true;
}