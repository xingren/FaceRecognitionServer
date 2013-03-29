// FaceRecognition.cpp : 定义控制台应用程序的入口点。
//

/*
 * Copyright (c) 2011. Philipp Wagner <bytefish[at]gmx[dot]de>.
 * Released to public domain under terms of the BSD Simplified license.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the organization nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *   See <http://www.opensource.org/licenses/bsd-license>
 */

#include"stdafx.h"

#include <opencv2/core/core.hpp>
#include <opencv2/contrib/contrib.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv/highgui.h>
#include <opencv/cv.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include<queue>
#include<boost/thread.hpp>
#include<vector>
#include<map>
#include<boost/container/list.hpp>


#ifdef WIN32
#include <Windows.h>
#include<process.h>



DWORD server_thread_id;
HANDLE thread_done;
DWORD main_thread_id;
HANDLE hFileMapping;
HANDLE hResultMapping;
DWORD message_thread_id;
#endif
#include "../message_types.h"



using namespace cv;
using namespace std;






#define FACE_WIDTH 92
#define FACE_HIGH 112
#define TrainDataPath "../LBP_TrainData.xml"

static const int async_run_num = 4;
const int IPC_BUFF_SIZE = 1024*1024*10*async_run_num;
HANDLE file_mapping_op_finish;

Ptr<FaceRecognizer> model;//facerecognition ,use opencv lib

//处理队列
boost::mutex work_queue_mutex;
boost::mutex cli_map_mutex;
#define IMAGE 0
#define ADD 1
#define RECOGNIZE 2

struct WorkInfo
{
	int work_type;
	int cli_id;
	cv::Mat img;//Mat 类本身实现了对内存的动态管理，无需关心多个Mat对象共享同一块数据所带来析构的问题
	vector<int> personId;
	vector<CvRect> rects;
	WorkInfo& operator=(const WorkInfo& a)
	{
		this->img = a.img ;
		this->work_type = a.work_type;
		this->personId = a.personId;
		this->rects = a.rects;
		return *this;
	}
};

queue<WorkInfo> vw;
queue<int> work_queue;
vector<WorkInfo> cli_vec;

map<int,WorkInfo> cli_map;

uchar *file_mapping;
char *rects_mapping;
CvRect** displaydetection(IplImage *pInpImg,int &resCount)
{

	CvHaarClassifierCascade* pCascade=0;		//指向后面从文件中获取的分类器
	CvMemStorage* pStorage=0;					//存储检测到的人脸数据
	CvSeq* pFaceRectSeq;
	pStorage=cvCreateMemStorage(0);				//创建默认大先64k的动态内存区域

	//需要根据实际情况修改:加载分类器的路径
	pCascade=(CvHaarClassifierCascade*)cvLoad("../haarcascade_frontalface_alt2.xml");		//加载分类器
	if (!pInpImg||!pStorage||!pCascade)
	{
		printf("initialization failed:%s\n",(!pInpImg)?"can't load image file":(!pCascade)?"can't load haar-cascade---make sure path is correct":"unable to allocate memory for data storage","pass a wrong IplImage*");
		return NULL;
	}
	//人脸检测
	pFaceRectSeq=cvHaarDetectObjects(pInpImg,pCascade,pStorage,
		1.2,2,CV_HAAR_DO_CANNY_PRUNING,cvSize(40,40));
	//将检测到的人脸以矩形框标出。
	int i;
	cvNamedWindow("haar window",1);
	printf("the number of face is %d\n",pFaceRectSeq->total);
	if(pFaceRectSeq->total == 0)
		return NULL;

	IplImage *result = cvCreateImage(cvSize(92,112),pInpImg->depth,pInpImg->nChannels);
	resCount = pFaceRectSeq->total;
	//if(NULL == result)
	//	//添加错误处理：无法分配内存
	//{}

	IplImage *show_img = cvCreateImage(cvSize(92,112),pInpImg->depth,pInpImg->nChannels);

	char str[100];
	CvRect** res = new CvRect*[resCount];
	for (i=0; i<(pFaceRectSeq?pFaceRectSeq->total:0); i++)
	{

		CvRect* r=(CvRect*)cvGetSeqElem(pFaceRectSeq,i);
		res[i] = new CvRect;
		memcpy(res[i],r,sizeof(CvRect));
		CvPoint pt1= {r->x,r->y};
		CvPoint pt2= {r->x+r->width,r->y+r->height};

		cvSetImageROI(pInpImg,*r);

		//将捕捉的脸保存

		

		//cvSetImageROI(pInpImg,*r);
		cvResize(pInpImg,result,CV_INTER_LINEAR);
		sprintf(str,"%d",i);
		strcat(str,".jpg");
		cvSaveImage(str,result);

		cvResetImageROI(pInpImg);
		//cvResetImageROI(pInpImg);
		show_img = cvCloneImage(pInpImg);
		cvRectangle(show_img,pt1,pt2,CV_RGB(0,255,0),3,4,0);

	}

	cvShowImage("haar window",show_img);
	//	cvResetImageROI(pInpImg);
//	cvWaitKey(0);
	cvDestroyWindow("haar window");
	//cvReleaseImage(&pInpImg);
	cvReleaseHaarClassifierCascade(&pCascade);
	cvReleaseMemStorage(&pStorage);
	cvReleaseImage(&result);
	cvReleaseImage(&show_img);
	return res;
}

static void read_csv(const string& filename, vector<Mat>& images, vector<int>& labels, char separator = ';') {
    std::ifstream file(filename.c_str(), ifstream::in);
    if (!file) {
        string error_message = "No valid input file was given, please check the given filename.";
        CV_Error(CV_StsBadArg, error_message);
    }
    string line, path, classlabel;
    while (getline(file, line)) {
        stringstream liness(line);
        getline(liness, path, separator);
        getline(liness, classlabel);
        if(!path.empty() && !classlabel.empty()) {
            images.push_back(imread(path, 0));
            labels.push_back(atoi(classlabel.c_str()));
        }
    }
}

void MyPostThreadMessage(DWORD thread_id,MSG &msg)
{
	int ret;
	int count = 0;
	while((ret = PostThreadMessage(thread_id,msg.message,msg.wParam,msg.lParam)) == 0 
		&& count < 10)
	{
		count++;
		DWORD e = GetLastError();
		LPVOID lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,e,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),(LPSTR)&lpMsgBuf,0,NULL);
		char tmp[200];
		sprintf(tmp,"PostThreadMessage failed with error %d: %s",e,lpMsgBuf);
		printf("%s\n",tmp);
	}
	if(count == 10)
	{
		printf("failed in PostMessage with %d\n",msg.message);
	}
}

template<class T>
void deserialze(char* p,size_t len,T& val)
{
	size_t size = sizeof(T);
	int n = min(len,size);
	memcpy(&val,p,n);
}



unsigned int WINAPI Core(VOID* param)
{
	WorkInfo tmp;
	int cli_id;
	while(1)
	{
		
		work_queue_mutex.lock();
		if(work_queue.size() == 0)
		{
			work_queue_mutex.unlock();
			Sleep(100);
			continue;
		}
		cli_id = work_queue.front();
		work_queue.pop();
		//vw.pop();
		work_queue_mutex.unlock();
		cli_map_mutex.lock();
		map<int,WorkInfo>::iterator iter = cli_map.find(tmp.cli_id);
		cli_map_mutex.unlock();

		tmp = iter->second;

		switch(tmp.work_type)
		{
		case IMAGE:
			{
				

				imshow("test",iter->second.img);
				waitKey();
				cv::destroyWindow("test");


				IplImage img = IplImage(tmp.img);
				int count;
				CvRect** rects;
				rects = displaydetection(&img,count);

				if(nullptr != rects)
				{
					for(int i = 0;i < count;i++)
						tmp.rects.push_back(*rects[i]);
				}

				//之所以再搜索是因为iter所指向的可能会被删除，所以要确定是否还在
				cli_map_mutex.lock();

				iter = cli_map.find(cli_id);
				if(cli_map.end() != iter)//没有被删除
				{
					iter->second = tmp;
				}

				memcpy(rects_mapping,rects,sizeof(CvRect)*count);

				PostThreadMessage(server_thread_id,WM_RESULT_DECTIVE,cli_id,count);
				cli_map_mutex.unlock();
			}
			break;
		case ADD:
			{
			}
			break;
		}
	}
	return 0;
}

unsigned int WINAPI ProcessMessage(VOID* param)
{
	MSG msg;
	PeekMessage(&msg,NULL,WM_USER,WM_USER,PM_NOREMOVE);
	SetEvent(thread_done);
	hFileMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS,FALSE,"file-mapping");
	
	if(NULL == hFileMapping)
	{
		cout << "error in hFileMapping " << GetLastError() << endl;
		
	}

	hResultMapping = OpenFileMapping(FILE_MAP_ALL_ACCESS,FALSE,"result-mapping");

	if(NULL == hResultMapping)
	{
		cout << "error in hResultMapping" << endl;
	}

	

	
	BOOL bRet;
	while((bRet = GetMessage(&msg,NULL,0,0)) > 0)
	{
		TranslateMessage(&msg);
		switch(msg.message)
		{
		case WM_GET_SERVER_ID:
			{
				server_thread_id = msg.wParam;

			}
			break;
		case WM_IMAGE:
			{
				
				printf("Recognition: WM_IMAGE\n");
				//flag1
				uchar* buf = file_mapping + 4;
				size_t len;
				deserialze((char*)file_mapping,4,len);
				vector<uchar> vu;
				for(int i = 0;i < len;i++)
					vu.push_back(buf[i]);
				//getchar();
				//shared memory read finish,
				SetEvent(file_mapping_op_finish);
				//flag2
				int cli_id = msg.wParam;

				printf("Recognition: WM_IMAGE flag1 done\n");
				WorkInfo tmp;
				tmp.personId.push_back(msg.wParam);
				Mat img = imdecode(vu,CV_LOAD_IMAGE_COLOR);
				printf("Recognition: WM_IMAGE flag2 done\n");
				tmp.img = img;
				tmp.work_type = IMAGE;
				work_queue_mutex.lock();
				vw.push(tmp);
				work_queue.push(cli_id);
				work_queue_mutex.unlock();
			}
			break;
		case WM_ADD_FACE:
			{
				
			}
			break;
		case WM_EXIT:
			{
				PostThreadMessage(main_thread_id,WM_EXIT,NULL,NULL);
			}
			break;
		case WM_CLIENT_DISCONNECT:
			{
				int id = msg.wParam;
				map<int,WorkInfo>::iterator iter = cli_map.find(id);


				if(iter != cli_map.end())
				{
					cli_map_mutex.lock();
					cli_map.erase(iter);
					cli_map_mutex.unlock();
				}
				else
				{
					printf("recognition: not exist this id: %d",id);
				}
			}
			break;
		default:
			{
				printf("undefine message type\n");
			}
		}
	}
	return 0;
}




int _tmain(int argc, _TCHAR* argv[]) {
	SetConsoleTitle("recognition");

	char *command = GetCommandLine();
	stringstream sstr(command);
	string str;
	printf("%s\n",command);
	sstr >> str;
	sstr >> server_thread_id;

	file_mapping_op_finish = OpenEvent(EVENT_ALL_ACCESS,FALSE,"file_mapping_op_finish");
	
	thread_done = CreateEvent(NULL,FALSE,FALSE,"build-message-queue");
	

	file_mapping = (uchar*)MapViewOfFile(hFileMapping,FILE_MAP_ALL_ACCESS,
					0,0,0);
	rects_mapping = (char*)MapViewOfFile(hResultMapping,FILE_MAP_ALL_ACCESS,
					0,0,0);

	if(nullptr == file_mapping )
	{
		printf("error in MapViewOffFile with file_mapping: %ld\n",GetLastError());
		PostThreadMessage(server_thread_id,WM_RECOGNITION_ERROR,NULL,NULL);
		return -1;
	}
	if(nullptr == rects_mapping)
	{
		printf("error in MapViewOffFile with rects: %ld\n",GetLastError());
		PostThreadMessage(server_thread_id,WM_RECOGNITION_ERROR,NULL,NULL);
		return -1;
	}

	printf("file_mapping point:%x\n",file_mapping);
	printf("rects point:%x\n",rects_mapping);

	main_thread_id = GetCurrentThreadId();
	thread_done = CreateEvent(NULL,FALSE,FALSE,"build-thread-queue");
	HANDLE get_message_thread = (HANDLE)_beginthreadex(NULL,NULL,ProcessMessage,NULL,NULL,(unsigned int*)&message_thread_id);
	WaitForSingleObject(thread_done,INFINITE);
	printf("create ProcessMessage \n");
	DWORD core_thread_id;
	HANDLE core_thread_handler = (HANDLE)_beginthreadex(NULL,NULL,Core,NULL,NULL,(unsigned int*)&core_thread_id);
	printf("create core thread \n");
	//freopen("../train.txt","r",stdin);
    
    model = createLBPHFaceRecognizer();
    //0model->train(images, labels);
//	model->load("../FaceData.xml");
    // The following line predicts the label of a given
    // test image:
	//model->save("LBP_TrainData.xml");
  //  int predictedLabel = model->predict(testSample);
    //
    // To get the confidence of a prediction call the model with:
    //
    //      int predictedLabel = -1;
    //      double confidence = 0.0;
    //      model->predict(testSample, predictedLabel, confidence);
    //

	
	MSG msg;
	msg.message = WM_GET_RECOGNITION_ID;
	msg.wParam = message_thread_id;
	msg.lParam = main_thread_id;
	MyPostThreadMessage(server_thread_id,msg);
	BOOL bRet;
	while((bRet = GetMessage(&msg,NULL,0,0)) > 0)
	{
		TranslateMessage(&msg);
		switch(msg.message)
		{
		case WM_EXIT:
			
			CloseHandle(get_message_thread);
			CloseHandle(thread_done);
			CloseHandle(core_thread_handler);
			CloseHandle(file_mapping_op_finish);
			return 0;
			break;
		}
	}
	CloseHandle(get_message_thread);
	CloseHandle(thread_done);
	CloseHandle(core_thread_handler);
	CloseHandle(file_mapping_op_finish);
    return 0;
}
