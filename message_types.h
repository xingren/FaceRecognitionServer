#include<windows.h>

#define WM_USER 0x0400
#define WM_IMAGE WM_USER + 1
#define WM_ADD_FACE WM_USER + 2
#define WM_ADD_PERSON WM_USER +3
#define WM_GET_RECOGNITION_ID WM_USER + 4
#define WM_GET_SERVER_ID WM_USER + 5
#define WM_EXIT WM_USER + 6
#define WM_RESULT WM_USER + 7
#define WM_RESULT_DECTIVE WM_USER + 8
#define WM_RESULT_FIND WM_USER + 9
#define WM_RECOGNITION_ERROR WM_USER + 10
#define WM_CLIENT_DISCONNECT WM_USER + 11