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
DWORD save_thread_id;
HANDLE CoreExitEvent;


HANDLE rects_mapping_mutex;//对检测结果的共享区域互斥访问
HANDLE file_mapping_mutex;//对待检测图片的共享区域互斥访问


bool core_exit = false;

#endif
#include "../message_types.h"



using namespace cv;
using namespace std;


#define FACE_WIDTH 92
#define FACE_HIGH 112
#define TrainDataPath "../LBP_TrainData.xml"

static const int async_run_num = 4;
const int IPC_BUFF_SIZE = 1024*1024*10*async_run_num;
//HANDLE file_mapping_op_finish;

Ptr<FaceRecognizer> model;//facerecognition ,use opencv lib

//处理队列
boost::mutex work_queue_mutex;
boost::mutex client_map_mutex;


template<class T>
void deserialze(char* p,size_t len,T& val)
{
	size_t size = sizeof(T);
	int n = min(len,size);
	memcpy(&val,p,n);
}

template<class T>
void serialze(char *out,size_t len,T& val)
{
	size_t size = sizeof(T);
	int n= min(len,size);
	memcpy(out,&val,n);
}
boost::mutex _mutex;
void Print_S(char*info )
{
	_mutex.lock();
	cout << info << endl;
	_mutex.unlock();
}




struct WorkInfo
{
	int work_type;
	int client_id;
	cv::Mat img;//Mat 类本身实现了对内存的动态管理，无需关心多个Mat对象共享同一块数据所带来析构的问题
	vector<int> personId;
	vector<CvRect> rects;
	WorkInfo& operator=(const WorkInfo& a)
	{
		this->img = a.img ;
		this->work_type = a.work_type;
		this->personId = a.personId;
		this->rects = a.rects;
		client_id = a.client_id;
		return *this;
	}
};

//需要被图片识别的任务队列
queue<WorkInfo> work_queue;
//queue<int> work_queue;
vector<WorkInfo> cli_vec;

//相当于session，保存当前客户端的一些数据
map<int,WorkInfo> client_map;

uchar *file_mapping;
char *rects_mapping;
CvRect* displaydetection(IplImage *pInpImg,int &resCount)
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
	
	printf("the number of face is %d\n",pFaceRectSeq->total);
	resCount = pFaceRectSeq->total;
	if(pFaceRectSeq->total == 0)
	{
		cvReleaseHaarClassifierCascade(&pCascade);
		cvReleaseMemStorage(&pStorage);
		return nullptr;
	}
	cvNamedWindow("haar window",1);
	IplImage *result = cvCreateImage(cvSize(92,112),pInpImg->depth,pInpImg->nChannels);
	
	//if(NULL == result)
	//	//添加错误处理：无法分配内存
	//{}

	IplImage *show_img = cvCreateImage(cvSize(92,112),pInpImg->depth,pInpImg->nChannels);

	char str[100];
	CvRect* res = new CvRect[resCount];
	for (i=0; i<(pFaceRectSeq?pFaceRectSeq->total:0); i++)
	{

		CvRect* r=(CvRect*)cvGetSeqElem(pFaceRectSeq,i);
		
		memcpy(&res[i],r,sizeof(CvRect));
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




unsigned int WINAPI Core(VOID* param)
{
	WorkInfo tmp;
	int client_id;
	
	while(!core_exit)
	{
		
		work_queue_mutex.lock();
		if(work_queue.size() == 0)
		{
			work_queue_mutex.unlock();
			Sleep(1000);
			continue;
		}
		tmp = work_queue.front();
		work_queue.pop();
		//vw.pop();
		work_queue_mutex.unlock();

		client_id = tmp.client_id;
		switch(tmp.work_type)
		{
		case WM_IMAGE:
			{
				printf("recognition with client id %d\n",client_id);

		//		imshow("test",tmp.img);
		//		waitKey();
		//		cv::destroyWindow("test");


				IplImage img = IplImage(tmp.img); //不用Release，因为是共享内存
				int resCount;
				CvRect* rects;
				rects = displaydetection(&img,resCount);

				if(nullptr != rects)
				{
					for(int i = 0;i < resCount;i++)
						tmp.rects.push_back(rects[i]);
					//识别
					IplImage *face_gray = cvCreateImage(cvSize(FACE_WIDTH,FACE_HIGH),img.depth,1);
					IplImage *face = cvCreateImage(cvSize(FACE_WIDTH,FACE_HIGH),img.depth,img.nChannels);
					WaitForSingleObject(rects_mapping_mutex,INFINITE);
					for(int i = 0;i<resCount;i++)
					{
						printf("the %d rect is:%d,%d,%d,%d\n",i,rects[i].x,rects[i].y,rects[i].width,rects[i].height);
						cvSetImageROI(&img,rects[i]);
						printf("after cvSetImageROI\n");
						cvResize(&img,face);
						printf("after cvResize\n");
						cvCvtColor(face,face_gray,CV_RGB2GRAY);
						printf("after cvCvtColor\n");
						int label = -1;
						label = model->predict(Mat(face_gray));
						label = (label < 1000)?-1:label;
						serialze(rects_mapping + i*(sizeof(CvRect) + 4),4,label);
						tmp.personId.push_back(label);

						//这里要互斥访问
						memcpy(rects_mapping + i*(sizeof(CvRect) + 4)+4,&rects[i],sizeof(CvRect));
						

					}
					ReleaseMutex(rects_mapping_mutex);
					cvReleaseImage(&face_gray);
					cvReleaseImage(&face);
				}
				if(nullptr!=rects)
					delete[] rects;
								
				cout << "send the dective results to server" << endl;
				PostThreadMessage(server_thread_id,WM_RESULT_DECTIVE,client_id,resCount);
				
			}
			break;
		case WM_ADD:
			{
				printf("Recognition: client id %d:ADD\n",client_id);
				IplImage img = IplImage(tmp.img);

				//用以model->update的参数
				vector<Mat> input;
				vector<int> labels;

		//		imshow("show",tmp.img);
		//		waitKey(0);
		//		cv::destroyWindow("show");

				CvRect rect = tmp.rects.at(0);
				printf("the rect is:%d %d %d %d\n",rect.x,rect.y,rect.width,rect.height);

				cvSetImageROI(&img,rect);
				IplImage *face_gray = cvCreateImage(cvSize(FACE_WIDTH,FACE_HIGH),img.depth,1);
				IplImage *face = cvCreateImage(cvSize(FACE_WIDTH,FACE_HIGH),img.depth,img.nChannels);
				cvResize(&img,face);
				cvCvtColor(face,face_gray,CV_RGB2GRAY);
				printf("the person id:%d\n",tmp.personId.at(0));
				input.push_back(Mat(face_gray,false));
				labels.push_back(tmp.personId.at(0));
				model->update(input,labels);
				cvReleaseImage(&face);
				cvReleaseImage(&face_gray);
			}
			break;
		}
	}
	SetEvent(CoreExitEvent);
	cout << "Core Thread exit" << endl;
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
				WaitForSingleObject(file_mapping_mutex,INFINITE);
				uchar* buf = file_mapping + 4;
				size_t len;
				deserialze((char*)file_mapping,4,len);
				vector<uchar> vu;
				for(int i = 0;i < len;i++)
					vu.push_back(buf[i]);
				ReleaseMutex(file_mapping_mutex);
				//getchar();
				//shared memory read finish,
				//SetEvent(file_mapping_op_finish);
				//flag2
				

				printf("Recognition: WM_IMAGE flag1 done\n");
				WorkInfo tmp;
				tmp.client_id = msg.wParam;
				printf("recognition WM_IMAGE with client id %d\n",tmp.client_id);
				tmp.personId.push_back(msg.wParam);
				Mat img = imdecode(vu,CV_LOAD_IMAGE_COLOR);
				printf("Recognition: WM_IMAGE flag2 done\n");
				tmp.img = img;
				tmp.work_type = WM_IMAGE;
				work_queue_mutex.lock();
				//vw.push(tmp);
				work_queue.push(tmp);
				work_queue_mutex.unlock();


				//更新client_map的信息
				client_map_mutex.lock();
				map<int,WorkInfo>::iterator iter;
				iter = client_map.find(tmp.client_id);
				if(client_map.end() != iter)
				{
					iter->second.img = tmp.img;
					printf("FaceRecognition:client id %d:change image \n");
				}
				else
				{
					client_map.insert(pair<int,WorkInfo>(tmp.client_id,tmp));
					printf("FaceRecognition:client id %d:insert this client id to client_map\n");
				}
				client_map_mutex.unlock();
			}
			break;
		case WM_ADD_FACE:
			{

				printf("Recognition:WM_ADD_FACE\n");

				char content[4+sizeof(CvRect)];
				WaitForSingleObject(rects_mapping_mutex,INFINITE);

				memcpy(content,rects_mapping,4+sizeof(CvRect));

				ReleaseMutex(rects_mapping_mutex);
				int personId;
				memcpy(&personId,content,4);
				CvRect rect;
				memcpy(&rect,content+4,sizeof(CvRect));
				int clientId = msg.wParam;
				client_map_mutex.lock();

				map<int,WorkInfo>::iterator iter = client_map.find(clientId);
				client_map_mutex.unlock();

				if(iter == client_map.end())//找不到客户端
				{
					PostThreadMessage(server_thread_id,WM_ADD_FACE_FAILED,clientId,personId);
					
				}
				else
				{
					
				WorkInfo work = iter->second;
				
				work.work_type = WM_ADD;
				work.rects.clear();
				work.rects.push_back(rect);
				work.personId.clear();
				work.personId.push_back(personId);
				work_queue_mutex.lock();
				work_queue.push(work);
				work_queue_mutex.unlock();
				
				
				}
				
			}
			break;
		case WM_EXIT:
			{
				core_exit = true;
				WaitForSingleObject(CoreExitEvent,10000);
				PostThreadMessage(main_thread_id,WM_EXIT,NULL,NULL);
				return 0;
			}
			break;
		case WM_CLIENT_DISCONNECT:
			{
				int id = msg.wParam;
				map<int,WorkInfo>::iterator iter = client_map.find(id);


				if(iter != client_map.end())
				{
					client_map_mutex.lock();
					client_map.erase(iter);
					client_map_mutex.unlock();
				}
				else
				{
					cout << "recognition not exist this id:" << id << endl;
				}
			}
			break;
		default:
			{
				cout << "undefine message type in process thread" << endl;
			}
		}
	}
	cout << "process message thread exit" << endl;
	return 0;
}


unsigned int WINAPI SaveThread(VOID* PVOID)
{
	MSG msg;
	BOOL bRet;
	while((bRet = GetMessage(&msg,NULL,NULL,NULL))>0)
	{
		TranslateMessage(&msg);
		switch(msg.message)
		{
		case WM_SAVE:
			model->save("../FaceData.xml");
			break;
		case WM_EXIT:
			model->save("../FaceData.xml");
			return 0;
			break;
		}

	}
	cout << "Save Thread Exit" << endl;
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

	file_mapping_mutex = CreateMutex (NULL,FALSE,"Global\\file_mapping_mutex");
	rects_mapping_mutex = CreateMutex (NULL,FALSE,"Global\\rects_mapping_mutex");

	if(rects_mapping_mutex == NULL)
	{
		printf("failed to open mutex:file_mapping_mutex:%ld\n",GetLastError());
		PostThreadMessage(server_thread_id,WM_RECOGNITION_ERROR,NULL,NULL);
		return -1;
	}
	if(file_mapping_mutex == NULL)
	{
		printf("failed to open mutex:rects_mapping_mutex:%ld\n",GetLastError());
		PostThreadMessage(server_thread_id,WM_RECOGNITION_ERROR,NULL,NULL);
		return -1;
	}
	CoreExitEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	main_thread_id = GetCurrentThreadId();
	thread_done = CreateEvent(NULL,FALSE,FALSE,"build-thread-queue");
	HANDLE get_message_thread = (HANDLE)_beginthreadex(NULL,NULL,ProcessMessage,NULL,NULL,(unsigned int*)&message_thread_id);
	WaitForSingleObject(thread_done,INFINITE);
	
	HANDLE save_add_face_thread = (HANDLE)_beginthreadex(NULL,NULL,SaveThread,NULL,NULL,(unsigned int*)save_thread_id);
	printf("create ProcessMessage \n");
	DWORD core_thread_id;
	HANDLE core_thread_handler = (HANDLE)_beginthreadex(NULL,NULL,Core,NULL,NULL,(unsigned int*)&core_thread_id);
	printf("create core thread \n");
	//freopen("../train.txt","r",stdin);
    
	HANDLE wait_load = CreateEvent(NULL,FALSE,FALSE,"recognition_load");

    model = createLBPHFaceRecognizer();
    //0model->train(images, labels);
	model->load("../FaceData.xml");

	SetEvent(wait_load);

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
		
			model->save("../FaceData.xml");


			CloseHandle(get_message_thread);
			CloseHandle(thread_done);
			CloseHandle(core_thread_handler);
			CloseHandle(file_mapping_mutex);
			CloseHandle(rects_mapping_mutex);
			CloseHandle(save_add_face_thread);
			CloseHandle(CoreExitEvent);
			CloseHandle(hFileMapping);
			CloseHandle(hResultMapping);
			return 0;
			break;
		}
	}
	model->save("../FaceData.xml");
	CloseHandle(get_message_thread);
	CloseHandle(thread_done);
	CloseHandle(core_thread_handler);
	CloseHandle(file_mapping_mutex);
	CloseHandle(rects_mapping_mutex);
	CloseHandle(save_add_face_thread);
	CloseHandle(hFileMapping);
	CloseHandle(hResultMapping);
    return 0;
}
