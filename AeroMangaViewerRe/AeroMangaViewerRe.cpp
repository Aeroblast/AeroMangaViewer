// AeroMangaViewerRe.cpp : 定义应用程序的入口点。
//

#include "stdafx.h"
#include "AeroMangaViewerRe.h"

#define TIMER_PullPage 5
#define TIMER_StopPulling 6
#define TIMER_Pulling 7
#define MAX_LOADSTRING 100
#define DebugLog(a) MessageBoxW(0,a,0,1)

// 全局变量: 
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
WCHAR exeDirPath[MAX_PATH];
WCHAR mangaDirPath[MAX_PATH];
WCHAR mangaTitle[100];
HWND hMWnd;
HDC hBackDc[3];
CRITICAL_SECTION* criticalSection[3];
int picHeight[3];
int picWidth;
RECT clientRect;
int seekerWidth;
int seekerHeight=0;
RECT seekerRect;
RECT seekerRect2;
RECT seekerFullRect;
int seekPage;
HFONT hFont;
HANDLE hCmd=0;
HMENU hMenu;
vector<WCHAR*> filePaths;
WCHAR lastPath[MAX_PATH] = { 0 };
WCHAR nextPath[MAX_PATH] = {0};
//bool loading=false;
bool autoPlay = false;
bool sized = false;//开始、最大化、恢复时使用
bool isSeeking;
int page;
int position;
int delta;
int pullPauseTime=2000,pullLastingTime=500,pullDistance,pullDelta;//pullDelta为负值
LPWSTR cmdLine=0;
// 此代码模块中包含的函数的前向声明: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Settings(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK     JumpTo(HWND, UINT, WPARAM, LPARAM);
void Open(WCHAR* fullPath,int);
void ClearTempDir();
void AdjustViewer();
void ResetAll(int mode);
DWORD WINAPI LoadPageThread(PVOID);

DWORD WINAPI CmdUnpackThread(PVOID h)
{
	WCHAR path[MAX_PATH];
	wcscpy_s(path,(WCHAR*)h);
	ClearTempDir();
	int i = 0;
	vector<WCHAR*>passwords;
	//读取预置密码
	WCHAR readPath[MAX_PATH];
	wcscpy_s(readPath, exeDirPath);
	wcscat_s(readPath, L"passwords");
	wfstream pwsStream(readPath);
	if (pwsStream.fail())
	{
		return 0;
	}
	else
	{
		WCHAR tempStr[100];
		while (pwsStream.getline(tempStr, 99))
		{
			passwords.push_back(new WCHAR[wcslen(tempStr) + 1]);
			wcscpy_s(passwords[i], wcslen(tempStr) + 1, tempStr);
			i++;
		} 
	}
	WCHAR tempPath[MAX_PATH];
	wcscpy_s(tempPath, exeDirPath);
	wcscat_s(tempPath, L"temp");
	i = 0;
	do
	{
		SECURITY_ATTRIBUTES sa;
		HANDLE hRead, hWrite;
		WCHAR command[1024];
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;
		if (!CreatePipe(&hRead, &hWrite, &sa, 0))break;
		wcscpy_s(command, L"7z e \"");
		wcscat_s(command, path);
		wcscat_s(command, L"\" -o\"");
		wcscat_s(command, tempPath);
		wcscat_s(command, L"\" -y -p");
		if (i<(int)(passwords.size()))wcscat_s(command, passwords[i]);

		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		si.cb = sizeof(STARTUPINFO);
		GetStartupInfo(&si);
		si.hStdError = hWrite;            //把创建进程的标准错误输出重定向到管道输入
		si.hStdOutput = hWrite;           //把创建进程的标准输出重定向到管道输入
		si.wShowWindow = SW_HIDE;
		si.dwXCountChars = 1024;
		si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES | STARTF_USECOUNTCHARS;
		//关键步骤，CreateProcess函数参数意义请查阅MSDN
		if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &si, &pi))
		{
			CloseHandle(hWrite);
			CloseHandle(hRead);
		}
		CloseHandle(hWrite);

		//CHAR buffer[1024] = { 0 };
		//DWORD bytesRead;
		//		while (true)
		//	{
		//	if (ReadFile(hRead, buffer, 1023, &bytesRead, NULL) == NULL)break;
		//MessageBoxA(NULL, buffer, NULL, NULL);
		//	}
		CloseHandle(hRead);
		DWORD ec;
		hCmd = pi.hProcess;
		WaitForSingleObject(pi.hProcess, -1);
		GetExitCodeProcess(pi.hProcess, &ec);
		if (ec == 2)i++;
		if (ec == 0) 
		{
			Open(tempPath,1);
			break; 
		}
		if (i >= (int)passwords.size())
		{
			WCHAR tempStr0[100];
			GetWindowText(hMWnd,tempStr0,99);
			wcscat_s(tempStr0,L"失败！？");
			SetWindowText(hMWnd,tempStr0);
			hCmd = 0; return 1;
			break;
		}
	} while (true);
	HANDLE hFind;
	WIN32_FIND_DATA wfd = {};

		WCHAR wildcard[MAX_PATH];
	    wcscpy_s(wildcard, tempPath);
		wcscat_s(wildcard, L"\\*.*");
		hFind = FindFirstFile(wildcard, &wfd);
		do
		{
			if (wfd.cFileName[0] == '.')
				continue;
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				wcscpy_s(mangaTitle, wfd.cFileName); break;
			}
		} while (FindNextFile(hFind, &wfd));
		WCHAR tempStr[120];
		wcscpy_s(tempStr, L"Aero Manga Viewer - ");
		wcscat_s(tempStr, mangaTitle);
		SetWindowText(hMWnd, tempStr);
	hCmd = 0;
	return 0;
}
void ClearTempDir()
{ 
	WCHAR tempStr[MAX_PATH];
	wcscpy_s(tempStr, exeDirPath);
	wcscat_s(tempStr, L"temp\\*");
	WIN32_FIND_DATA wfd = { 0 };
	HANDLE hFind = FindFirstFile(tempStr, &wfd);
	do
	{
		if (wfd.cFileName[0] == '.')
			continue;
		if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			WCHAR delFull[MAX_PATH];
			wcscpy_s(delFull, exeDirPath);
			wcscat_s(delFull, L"temp\\");
			wcscat_s(delFull, wfd.cFileName);
			RemoveDirectory(delFull);
		}
		else
		{
			WCHAR delFull[MAX_PATH];
			wcscpy_s(delFull, exeDirPath);
			wcscat_s(delFull, L"temp\\");
			wcscat_s(delFull, wfd.cFileName);
			DeleteFile(delFull);
		}
	} while (FindNextFile(hFind, &wfd));
	FindClose(hFind);
}
void CopyTitle()
{
	size_t len = lstrlen(mangaTitle) + 1;
	HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, len * sizeof(WCHAR));
	WCHAR* pGlobal = (WCHAR*)GlobalLock(hGlobal);
	wcscpy_s(pGlobal, len, mangaTitle);
	GlobalUnlock(hGlobal);
	OpenClipboard(hMWnd);
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hGlobal);
	CloseClipboard();

}
bool isImageExt(WCHAR*ext) 
{
	if (_wcsicmp(ext, L".jpg") == 0 || _wcsicmp(ext, L".jpeg") == 0 || _wcsicmp(ext, L".png") == 0 || _wcsicmp(ext, L".bmp") == 0)return true;
	return false;
}
bool isArchiveExt(WCHAR*ext)
{
	if (_wcsicmp(ext, L".7z") == 0 || _wcsicmp(ext, L".rar") == 0 || _wcsicmp(ext, L".zip") == 0 )return true;
	return false;
}
void UpdateSeekerRect(int page,LPRECT r) 
{
	if (filePaths.size() == 0)return;
	r->top = (clientRect.bottom*(page)) / filePaths.size();
	r->bottom = r->top + seekerHeight;
}
bool wcsort(const WCHAR*a, const WCHAR*b)
{
	return 	 _wcsicmp(a, b)<0;
}
void ResetAll(int mode) 
{
	filePaths.clear();
	page = 0;
	position = 0;
	mangaTitle[0] = 0;
	if (mode == 1)return;
	nextPath[0] = 0;
	lastPath[0] = 0;
}
void GetPreNext(WCHAR *fullPath) 
{
	HANDLE hFind;
	WIN32_FIND_DATA wfd = {};
	WCHAR dir_[MAX_PATH];
	WCHAR dir[MAX_PATH];
	WCHAR name[MAX_PATH];
	WCHAR ext[_MAX_EXT];
	WCHAR wildcard[MAX_PATH];
	WCHAR next[MAX_PATH] = { 0 }, last[MAX_PATH] = { 0 };
	nextPath[0] = 0;
	lastPath[0] = 0;
	_wsplitpath_s(fullPath, dir_, _MAX_DRIVE, dir, MAX_PATH, name, MAX_PATH, ext, _MAX_EXT);
	wcscat_s(name, ext);
	wcscat_s(dir_, dir);
	wcscpy_s(wildcard, dir_);
	wcscat_s(wildcard, L"*.*");
	WCHAR file_ext[_MAX_EXT];
	WCHAR file_path[MAX_PATH];
	hFind = FindFirstFile(wildcard, &wfd);
	do
	{
		if (wfd.cFileName[0] == '.')
			continue;
		else
		{
			wcscpy_s(file_path, dir_);
			wcscat_s(file_path, wfd.cFileName);
			_wsplitpath_s(wfd.cFileName, NULL, 0, NULL, 0, NULL, 0, file_ext, _MAX_EXT);
			if (!(FILE_ATTRIBUTE_DIRECTORY&GetFileAttributes(file_path)) && !isArchiveExt(file_ext)) continue;
			int cmp = _wcsicmp(wfd.cFileName, name);
			if (cmp> 0 && (next[0] == 0 || _wcsicmp(wfd.cFileName, next)<0))
			{
				wcscpy_s(next, wfd.cFileName);
			}
			if (cmp< 0 && _wcsicmp(wfd.cFileName, last)>0)
			{
				wcscpy_s(last, wfd.cFileName);
			}
		}
	} while (FindNextFile(hFind, &wfd));
	if (next[0]) { wcscpy_s(nextPath, dir_); wcscat_s(nextPath, next); }
	if (last[0]) { wcscpy_s(lastPath, dir_); wcscat_s(lastPath, last); }
	FindClose(hFind);
}
void Open(WCHAR* fullPath,int mode)
{
	//mode用于分辨内部二次调用打开temp和一次调用打开文件
	HANDLE hFind;
	WIN32_FIND_DATA wfd = {};
	
	if (GetFileAttributes(fullPath)&FILE_ATTRIBUTE_DIRECTORY)//文件夹的情况
	{
		WCHAR wildcard[MAX_PATH];
		WCHAR extName[_MAX_EXT];
		wcscpy_s(wildcard, fullPath);
		if (_wcsicmp(&(wildcard[wcslen(wildcard) - 1]), L"\\") != 0) { wcscat_s(wildcard, L"\\"); }
		wcscat_s(wildcard, L"*.*");
		hFind = FindFirstFile(wildcard, &wfd);
		if (hFind != NULL)
		{ ResetAll(mode); }
		do
		{
			if (wfd.cFileName[0] == '.')
				continue;
			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;
			else
			{
				_wsplitpath_s(wfd.cFileName, NULL, 0, NULL, 0, NULL, 0, extName, _MAX_EXT);
				if (isImageExt(extName)) {
					filePaths.push_back(new WCHAR[MAX_PATH]);
					wcscpy_s(filePaths[filePaths.size() - 1], MAX_PATH, wfd.cFileName);
				}
			}
		} while (FindNextFile(hFind, &wfd));
		std::sort(filePaths.begin(), filePaths.end(), wcsort);
		FindClose(hFind);

		if (mode == 0)GetPreNext(fullPath);
		wcscpy_s(mangaDirPath, MAX_PATH,fullPath );
		wcscat_s(mangaDirPath,L"\\");
		_wsplitpath_s(fullPath, 0, 0, 0, 0, mangaTitle, 100, 0, 0);
		CreateThread(NULL, 0, LoadPageThread, 0, 0, NULL);
		CreateThread(NULL, 0, LoadPageThread, (LPVOID)1, 0, NULL);
	}
	else //if (GetFileAttributes(fullPath)&FILE_ATTRIBUTE_NORMAL)
	{
		WCHAR extName[_MAX_EXT];
		_wsplitpath_s(fullPath, NULL, 0, NULL, 0, NULL, 0, extName, _MAX_EXT);
		if (isImageExt(extName))//一张图的情况
		{
			WCHAR dri[_MAX_DRIVE], dir[MAX_PATH], file[MAX_PATH],ext[_MAX_EXT];
			_wsplitpath_s(fullPath, dri, _MAX_DRIVE,dir, MAX_PATH,file,MAX_PATH ,NULL, 0);
			WCHAR tempStr[MAX_PATH];
			WCHAR tempStrMangaTitle[100];
			wcscpy_s(tempStr,dri);
			wcscat_s(tempStr,dir);
			int x = wcslen(tempStr);
			tempStr[x - 1] = 0;
			_wsplitpath_s(tempStr,NULL,0,NULL,0,tempStrMangaTitle,100,NULL,0);
			tempStr[x - 1] = L'\\';
			wcscpy_s(mangaDirPath,tempStr);
			wcscat_s(tempStr, L"*.*");
			hFind = FindFirstFile(tempStr, &wfd);
			if (hFind != NULL) { ResetAll(0); }
			wcscpy_s(mangaTitle,tempStrMangaTitle);
			do
			{
				if (wfd.cFileName[0] == '.')
					continue;
				if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;
				else
				{

					_wsplitpath_s(wfd.cFileName, NULL, 0, NULL, 0, NULL, 0, ext, _MAX_EXT);
					if (isImageExt(ext)) 
					{
						filePaths.push_back(new WCHAR[MAX_PATH]);
						wcscpy_s(filePaths[filePaths.size() - 1], MAX_PATH, wfd.cFileName);
					}
				}
			} while (FindNextFile(hFind, &wfd));
			FindClose(hFind);
			std::sort(filePaths.begin(), filePaths.end(), wcsort);
			wcscat_s(file,extName);
             GetPreNext(mangaDirPath);
			for (unsigned int i = 0; i < filePaths.size(); i++)
			{
				if (wcscmp(file, filePaths[i]) == 0) { page = i; }
			}
			CreateThread(NULL, 0, LoadPageThread, (LPVOID)page, 0, NULL);
			if (page > 0) 
			{
				CreateThread(NULL, 0, LoadPageThread, (LPVOID)(page-1), 0, NULL);
			}
			if (page < (int)filePaths.size() - 1)
			{
				CreateThread(NULL, 0, LoadPageThread, (LPVOID)(page+1), 0, NULL);
			}
		}
		else if (isArchiveExt(extName))//压缩包
		{
			ResetAll(1);
			GetPreNext(fullPath);
			CreateThread(NULL, 0, CmdUnpackThread, fullPath, 0, NULL);
			wcscpy_s(mangaTitle,L"解压中……");
		}
	
	}

	WCHAR tempStr[120];
	wcscpy_s(tempStr, L"Aero Manga Viewer - ");
	wcscat_s(tempStr, mangaTitle);
	SetWindowText(hMWnd, tempStr);
	AdjustViewer();
}
void JumpTo(int p)
{
	if (p >= 0 && p != page&&p < (int)filePaths.size())
	{
		page = p;
		UpdateSeekerRect(page,&seekerRect);
		position = 0;
		CreateThread(NULL, 0, LoadPageThread, (LPVOID)page, 0, NULL);
		if (page > 0)
		{
			CreateThread(NULL, 0, LoadPageThread, (LPVOID)(page - 1), 0, NULL);
		}
		if (page < (int)filePaths.size() - 1)
		{
			CreateThread(NULL, 0, LoadPageThread, (LPVOID)(page + 1), 0, NULL);
		}
		InvalidateRect(hMWnd, NULL, TRUE);
	}

}
DWORD WINAPI LoadPageThread(PVOID page)
{
	if (filePaths.size() == 0)return -1;
	int i = ((int)page) % 3;
	EnterCriticalSection(criticalSection[i]);
	WCHAR temp[MAX_PATH];
	wcscpy_s(temp, mangaDirPath);
	wcscat_s(temp, filePaths[(int)page]);
	Bitmap *image = new Bitmap(temp);
	if (image->GetWidth() == 0)return -1;
	
	picHeight[i] = image->GetHeight()* picWidth / image->GetWidth();
	DeleteDC(hBackDc[i]);

	hBackDc[i] = CreateCompatibleDC(NULL);
	HBITMAP nullBitmap; int y = 0;
	if (picHeight[i] < clientRect.bottom)
	{
		nullBitmap = CreateCompatibleBitmap(GetWindowDC(hMWnd), picWidth, clientRect.bottom);
		y = clientRect.bottom-picHeight[i];
		y /= 2;
	}
	else nullBitmap = CreateCompatibleBitmap(GetWindowDC(hMWnd), picWidth, picHeight[i]);
	SelectObject(hBackDc[i], nullBitmap);
	
	
		Gdiplus::Graphics* g = new Gdiplus::Graphics(hBackDc[i]);
		g->SetInterpolationMode(InterpolationModeBilinear);
		
		if (y != 0)g->FillRectangle(new SolidBrush(Color(255,255,255)),0,0,picWidth,clientRect.bottom);
		g->DrawImage(image, 0, y, picWidth, picHeight[i]);

	delete image; image = 0;
	SelectObject(hBackDc[i], GetStockObject(BLACK_BRUSH));
	DeleteObject(nullBitmap);
	if (y != 0) { picHeight[i] = clientRect.bottom; }
	LeaveCriticalSection(criticalSection[i]);
	InvalidateRect(hMWnd, NULL, FALSE);
	
	return 0;
}
void AdjustViewer()
{
	GetClientRect(hMWnd, &clientRect);
	picWidth = clientRect.right - clientRect.left - seekerWidth - seekerWidth;
	seekerFullRect.bottom = clientRect.bottom;
	seekerFullRect.left = clientRect.right - seekerWidth;
	seekerFullRect.right = clientRect.right;

	seekerRect.left = clientRect.right -seekerWidth;
	seekerRect.right = clientRect.right;
	//if (paths.size() != 0)seekerRect.top = clientRect.top + (clientRect.bottom - clientRect.top - 10)*(page + 1) / paths.size() - 5;

	if (filePaths.size() > 0) 
	{
		seekerHeight = clientRect.bottom / filePaths.size();	
	}
	if (seekerHeight < seekerWidth / 2)seekerHeight = seekerWidth / 2;
	UpdateSeekerRect(page,&seekerRect);
	//pageLogRect.top = seekerRect.top;
	//pageLogRect.bottom = seekerRect.bottom + 5;
	//pageLogRect.left = seekerRect.left - 40;
	//pageLogRect.right = seekerRect.left;

	//delta = (clientRect.bottom - clientRect.top) / 6;
}
void Pull(int d)
{
	if (filePaths.size() <= 0)return;
	position += d;
	int i = page % 3;
	UpdateSeekerRect(page,&seekerRect);
	//顶头or到底的矫正
	if (page == filePaths.size() - 1 && position <clientRect.bottom - picHeight[i]) 
	{ 
		position = clientRect.bottom - picHeight[i]; 
	}
	if (page == 0 && position > 0)
	{
		position = 0;
	}

	//换页
	if (position+picHeight[i]<0) 
	{
		position += picHeight[i];
		page++;
		if (page + 1 < (int)filePaths.size()) 
		{
			CreateThread(NULL, 0, LoadPageThread, (PVOID)(page + 1), 0, NULL);
		}
	}
	if (position > clientRect.bottom ) 
	{
		page--;
		position -= picHeight[page%3];
		
		if (page - 1 >= 0) 
		{ 
			CreateThread(NULL, 0, LoadPageThread, (PVOID)(page - 1), 0, NULL); 
		}
	}
	InvalidateRect(hMWnd, NULL, FALSE);
}
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    // TODO: 在此放置代码。
	if (lpCmdLine[0] == L'\"') 
	{
		lpCmdLine[wcslen(lpCmdLine) - 1] = 0;
		cmdLine = lpCmdLine + sizeof(WCHAR)/2;
	}
	ClearTempDir();
    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_AEROMANGAVIEWERRE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
	WCHAR tempStr[MAX_PATH];
	GetModuleFileName(hInstance, exeDirPath, MAX_PATH);
	_wsplitpath_s(exeDirPath, tempStr,_MAX_DRIVE,exeDirPath, MAX_PATH, NULL, 0, NULL, 0);
	wcscat_s(tempStr, exeDirPath);
	wcscpy_s(exeDirPath,tempStr);
	seekerWidth=GetSystemMetrics(SM_CXCURSOR);
	criticalSection[0] = new CRITICAL_SECTION();
	criticalSection[1] = new CRITICAL_SECTION();
	criticalSection[2] = new CRITICAL_SECTION();
	InitializeCriticalSection(criticalSection[0]);
	InitializeCriticalSection(criticalSection[1]);
	InitializeCriticalSection(criticalSection[2]);
	delta = 100;
	// 创建字体 
	LOGFONT LogFont;
	memset(&LogFont, 0, sizeof(LOGFONT));
	lstrcpy(LogFont.lfFaceName, L"微软雅黑");
	LogFont.lfWeight = FW_BLACK;//FW_NORMAL; 
	LogFont.lfHeight = 24; // 字体大小 
	LogFont.lfCharSet = 134;
	LogFont.lfOutPrecision = 3;
	LogFont.lfClipPrecision = 2;
	LogFont.lfOrientation = 45;
	LogFont.lfQuality = 1;
	LogFont.lfPitchAndFamily = 2;
	 hFont = CreateFontIndirect(&LogFont);
    // 执行应用程序初始化: 
	SetProcessDPIAware();
	hMenu=LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU));
	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR           gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_AEROMANGAVIEWERRE));

    MSG msg;

    // 主消息循环: 
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
	GdiplusShutdown(gdiplusToken);
	if (hCmd != 0)TerminateProcess(hCmd, 255);
	ClearTempDir();
    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc; 
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_AEROMANGAVIEWERRE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_AEROMANGAVIEWERRE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目的: 保存实例句柄并创建主窗口
//
//   注释: 
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}



LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {

	case WM_CREATE: 
	{
		hMWnd = hWnd;
		DragAcceptFiles(hWnd, TRUE);
		AdjustViewer();
		WCHAR iniFile[MAX_PATH];
		wcscpy_s(iniFile, exeDirPath);
		wcscat_s(iniFile, L"settings.ini");
		WCHAR defaultW[5];
		WCHAR defaultH[5];
		WCHAR lastX[5];
		WCHAR lastY[5];
		GetPrivateProfileString(L"Default", L"Width", L"720", defaultW, 5, iniFile);
		GetPrivateProfileString(L"Default", L"Height", L"720", defaultH, 5, iniFile);	
		GetPrivateProfileString(L"Last", L"X", L"10", lastX, 5, iniFile);
		GetPrivateProfileString(L"Last", L"Y", L"10", lastY, 5, iniFile);
		SetWindowPos(hWnd,0, _wtoi(lastX), _wtoi(lastY), _wtoi(defaultW),_wtoi(defaultH), 0);
		WCHAR iniTemp[5];
		GetPrivateProfileString(L"", L"Delta", L"100", iniTemp, 5, iniFile);
		delta = _wtoi(iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullPauseTime", L"2000", iniTemp, 5, iniFile);
		pullPauseTime = _wtoi(iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullLastingTime", L"500", iniTemp, 5, iniFile);
		pullLastingTime = _wtoi(iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullDistance", L"2000", iniTemp, 5, iniFile);
		pullDistance = _wtoi(iniTemp);
		pullDelta = -pullDistance/(pullLastingTime / 25);
		if (cmdLine != 0) 
		{
			Open(cmdLine,0);
		}
	};
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择: 
            switch (wmId)
            {
			case ID_COPYTITLE:
				CopyTitle(); break;
			case ID_ESCAPE:
				SendMessage(hWnd,WM_DESTROY,0,0); break;
			case ID_SETTINGS:
				DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd,Settings);
				break;
			case ID_JUMPTO:
				if (filePaths.size() == 0)break;				
				DialogBox(hInst, MAKEINTRESOURCE(IDD_JUMPTO), hWnd, JumpTo);
				break;
			case ID_PINTOP:
				if (!GetMenuState(GetSubMenu(hMenu,0), ID_PINTOP, MF_CHECKED))
				{
					SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					CheckMenuItem(GetSubMenu(hMenu, 0), ID_PINTOP, MF_CHECKED);
				}
				else 
				{
					SetWindowPos(hWnd, HWND_NOTOPMOST,0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					CheckMenuItem(GetSubMenu(hMenu, 0),ID_PINTOP, MF_UNCHECKED);
				}			
				break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
	case WM_DROPFILES:
	{
		HDROP hDrop;
		WCHAR fullPath[MAX_PATH] = { 0 };
		hDrop = (HDROP)wParam;
		DragQueryFile(hDrop, 0, fullPath, MAX_PATH);
		DragFinish(hDrop);
		Open(fullPath,0);
		InvalidateRect(hWnd, NULL, TRUE);
	}
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
			FillRect(hdc,&seekerFullRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
			HBRUSH hBlue = CreateSolidBrush(RGB(100, 200, 255));
			SelectObject(hdc,hBlue);
			FillRect(hdc,&seekerRect,hBlue);
			if (isSeeking) 
			{
				hBlue = CreateSolidBrush(RGB(150, 250, 255));
				FillRect(hdc, &seekerRect2, hBlue);
			}
			//SelectObject(hdc,hFont);
			//TextOut(hdc, 0, 0, L"加载中", 3);
			int i = page % 3;
			if (hBackDc[i] != 0) 
			{
				EnterCriticalSection(criticalSection[i]);
				BitBlt(hdc, clientRect.left + seekerWidth, clientRect.top + position, picWidth, picHeight[i], hBackDc[i], 0, 0, SRCCOPY);
				LeaveCriticalSection(criticalSection[i]);
			}
			if (position + picHeight[i] < clientRect.bottom - clientRect.top)
			{
				int j = i + 1;
				if (j > 2)j -= 3;
				EnterCriticalSection(criticalSection[j]);
				BitBlt(hdc, clientRect.left + seekerWidth, clientRect.top + position + picHeight[i], picWidth, picHeight[j], hBackDc[j], 0, 0, SRCCOPY);
				LeaveCriticalSection(criticalSection[j]);
			}
			if (position > 0)
			{
				int j = i - 1;
				if (j < 0)j += 3;
				EnterCriticalSection(criticalSection[j]);
				BitBlt(hdc, clientRect.left + seekerWidth, clientRect.top + position - picHeight[j], picWidth, picHeight[j], hBackDc[j], 0, 0, SRCCOPY);
				LeaveCriticalSection(criticalSection[j]);
			}

            EndPaint(hWnd, &ps);
        }
        break;
	case WM_MOUSEWHEEL:
	{
		if ((short)HIWORD(wParam)< 0)
		{
			Pull(-delta);
		}
		else
		{
			Pull(delta);
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		POINT pt;
		pt.x = LOWORD(lParam); pt.y = HIWORD(lParam);
		if (pt.x >= seekerRect.left
			&& pt.x <= seekerRect.right
			&& pt.y >= seekerRect.top
			&& pt.y <= seekerRect.bottom
			)
		{
			//Logger

		}
		else
		{
			//取消Logger
		}

		if (isSeeking)
		{
			if (pt.y > seekerRect2.bottom) 
			{
				seekPage++;
				UpdateSeekerRect(seekPage,&seekerRect2);
			}
			if (pt.y < seekerRect2.top)
			{
				seekPage--;
				UpdateSeekerRect(seekPage, &seekerRect2);
			}
		InvalidateRect(hWnd, &seekerFullRect, TRUE);
		}
	}
	break;
	case WM_LBUTTONDOWN:
	{
		POINT pt;
		pt.x = LOWORD(lParam); pt.y = HIWORD(lParam);
		if (pt.x >= seekerRect.left
			&& pt.x <= seekerRect.right
			&& pt.y >= seekerRect.top
			&& pt.y <= seekerRect.bottom
			)
		{
			seekerRect2 = seekerRect;
			seekPage = page;
			isSeeking = true;
		}
	}
	break;
	case WM_RBUTTONDOWN: 
	{
		POINT pt;
		pt.x = LOWORD(lParam); pt.y = HIWORD(lParam);
		ClientToScreen(hWnd,&pt);
		TrackPopupMenu(
			GetSubMenu(hMenu, 0),
			0,pt.x,pt.y,0,hWnd,NULL
		);
		
	}
	break;
	case WM_LBUTTONUP:
	{
		if (isSeeking)
		{
			isSeeking = false;
			JumpTo(seekPage);
		}
	}
	break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_UP: Pull(delta); break;
		case VK_DOWN:Pull(-delta); break;
		case VK_RETURN:
		{
			
		    autoPlay = !autoPlay;
			if (autoPlay)
			{
				SendMessage(hWnd, WM_TIMER, TIMER_PullPage, 0);
			}
			else
			{
				KillTimer(hWnd, TIMER_PullPage);
				KillTimer(hWnd, TIMER_StopPulling);
				KillTimer(hWnd, TIMER_Pulling);
			}
		 		
		}
		break;
		case VK_NEXT:
			if (nextPath[0])
			{
				WCHAR pathTemp[MAX_PATH];
				wcscpy_s(pathTemp, nextPath);
				Open(pathTemp, 0);
			}
			else
				MessageBox(hWnd,L"没有下一个啦！",L"提示",0);
			break;
		case VK_PRIOR:			
			if (lastPath[0])
			{
				WCHAR pathTemp[MAX_PATH];
				wcscpy_s(pathTemp, lastPath);
				Open(pathTemp, 0);
			}
			else
				MessageBox(hWnd, L"没有上一个啦！", L"提示", 0);
			break;
		}
		break;
	case WM_TIMER:
		switch (wParam)
		{
		case TIMER_PullPage:
			KillTimer(hWnd, TIMER_PullPage);
			SetTimer(hWnd, TIMER_Pulling, 25, NULL);
			SetTimer(hWnd, TIMER_StopPulling, pullLastingTime, NULL);
			break;
		case TIMER_Pulling:
			Pull(pullDelta);
			break;
		case TIMER_StopPulling:
			KillTimer(hWnd, TIMER_Pulling);
			KillTimer(hWnd, TIMER_StopPulling);
			SetTimer(hWnd, TIMER_PullPage, pullPauseTime, NULL);
			break;
		}
		break;
	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_RESTORE:
		case SC_MAXIMIZE: 
		{
			sized = true;
		}
		
		default: return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_SIZE:
		if (!sized)break;//后面一定跟着WM_EXITSIZEMOVE以应对最大化后的AdjustViewer
	case WM_EXITSIZEMOVE:
	{
		
		int w = picWidth;
		AdjustViewer();
		if (picWidth != w) 
		{
			CreateThread(NULL, 0, LoadPageThread, (PVOID)(page), 0, NULL);
			if (page > 0)CreateThread(NULL, 0, LoadPageThread, (PVOID)(page - 1), 0, NULL);
			if (page < (int)filePaths.size() - 1)CreateThread(NULL, 0, LoadPageThread, (PVOID)(page + 1), 0, NULL);
		}
		InvalidateRect(hWnd, NULL, TRUE);
		sized = false;
		break;
	}
	case WM_DESTROY: 
	{
		WCHAR iniFile[MAX_PATH];
		wcscpy_s(iniFile, exeDirPath);
		wcscat_s(iniFile, L"settings.ini");
		WCHAR lastX[5];
		WCHAR lastY[5];
		RECT winRect;
		GetWindowRect(hWnd, &winRect);
		_itow_s(winRect.top, lastY, 10);
		_itow_s(winRect.left,lastX,10);
		WritePrivateProfileString(L"Last", L"X", lastX, iniFile);
		WritePrivateProfileString(L"Last", L"Y", lastY, iniFile);
		
		PostQuitMessage(0);
		break;
	}	
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:	
	{
		WCHAR iniFile[MAX_PATH];
		wcscpy_s(iniFile,exeDirPath);
		wcscat_s(iniFile,L"settings.ini");
		WCHAR defaultW[5];
		WCHAR defaultH[5];
		GetPrivateProfileString(L"Default",L"Width",L"720",defaultW,5,iniFile);
		GetPrivateProfileString(L"Default", L"Height", L"720", defaultH, 5, iniFile);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITWIDTH),defaultW);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITHEIGHT), defaultH);
		
		WCHAR iniTemp[5];
		GetPrivateProfileString(L"", L"Delta", L"100", iniTemp, 5, iniFile);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITDELTA), iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullPauseTime", L"2000", iniTemp, 5, iniFile);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITPULLPAUSETIME), iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullLastingTime", L"500", iniTemp, 5, iniFile);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITPULLLASTINGTIME), iniTemp);
		GetPrivateProfileString(L"Pulling", L"PullDistance", L"2000", iniTemp, 5, iniFile);
		SetWindowText(GetDlgItem(hDlg, IDC_EDITPULLDISTANCE), iniTemp);
		

		WCHAR pwsPath[MAX_PATH];
		wcscpy_s(pwsPath,exeDirPath);
		wcscat_s(pwsPath,L"passwords");
		wfstream pwsStream(pwsPath);
		WCHAR pws[1024] = {0};//一定归个0先……
		WCHAR tempStr[100];
		pwsStream.getline(tempStr, 99);
		do 
		{
			wcscat_s(pws, tempStr);
			if (pws[wcslen(pws) - 1] == L'\r')pws[wcslen(pws) - 1] = 0;
			wcscat_s(pws,L"\r\n");
		} while (pwsStream.getline(tempStr, 100));
		pws[wcslen(pws) - 2] = 0;
		SetWindowText(GetDlgItem(hDlg,IDC_EDITPASSWORDS),pws);
		return (INT_PTR)TRUE;
	}	
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_OK)
        {
			WCHAR iniFile[MAX_PATH];
			wcscpy_s(iniFile, exeDirPath);
			wcscat_s(iniFile, L"settings.ini");
			WCHAR defaultW[5];
			WCHAR defaultH[5];
			HWND hEditW = GetDlgItem(hDlg, IDC_EDITWIDTH); GetWindowText(hEditW, defaultW,5);
			HWND hEditH = GetDlgItem(hDlg, IDC_EDITHEIGHT); GetWindowText(hEditH, defaultH,5);
			WritePrivateProfileString(L"Default", L"Width", defaultW,iniFile);
			WritePrivateProfileString(L"Default", L"Height", defaultH, iniFile);
			SetWindowPos(hMWnd, 0, 0, 0, _wtoi(defaultW), _wtoi(defaultH), SWP_NOMOVE);
			WCHAR iniTemp[5];
			GetWindowText(GetDlgItem(hDlg, IDC_EDITDELTA), iniTemp,5);
			WritePrivateProfileString(L"", L"Delta", iniTemp, iniFile);
			delta = _wtoi(iniTemp);

			GetWindowText(GetDlgItem(hDlg, IDC_EDITPULLPAUSETIME), iniTemp,5);
			WritePrivateProfileString(L"Pulling", L"PullPauseTime" ,iniTemp,  iniFile);
			pullPauseTime = _wtoi(iniTemp);
			GetWindowText(GetDlgItem(hDlg, IDC_EDITPULLLASTINGTIME), iniTemp,5);
			WritePrivateProfileString(L"Pulling", L"PullLastingTime", iniTemp,  iniFile);
			pullLastingTime = _wtoi(iniTemp);
			GetWindowText(GetDlgItem(hDlg, IDC_EDITPULLDISTANCE), iniTemp,5);
			WritePrivateProfileString(L"Pulling", L"PullDistance", iniTemp, iniFile);
			pullDistance = _wtoi(iniTemp);
			pullDelta = -pullDistance / (pullLastingTime / 25);

			WCHAR pwsPath[MAX_PATH];
			wcscpy_s(pwsPath, exeDirPath);
			wcscat_s(pwsPath, L"passwords");
			wfstream pwsStream(pwsPath);
			WCHAR pws[1024];
			GetWindowText(GetDlgItem(hDlg, IDC_EDITPASSWORDS),pws,1024);

			pwsStream.write(pws,wcslen(pws));
			pwsStream.close();
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
		if (LOWORD(wParam) == IDC_CANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
        break;
    }
    return (INT_PTR)FALSE;
}


INT_PTR CALLBACK JumpTo(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		WCHAR pageStr[5];
		_itow_s(page+1,pageStr,10);
		SetWindowText(GetDlgItem(hDlg, IDC_PAGE), pageStr);
		WCHAR tempStr[10];
		wcscpy_s(tempStr,L"共");
		_itow_s(filePaths.size(),pageStr,10);
		wcscat_s(tempStr,pageStr);
		wcscat_s(tempStr,L"页");
		SetWindowText(GetDlgItem(hDlg, IDC_TEXT), tempStr);
		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_OK)
		{		
			WCHAR pageStr[5];
			GetWindowText(GetDlgItem(hDlg, IDC_PAGE),pageStr,5);
			seekPage = _wtoi(pageStr) - 1;
			if(page!=seekPage)JumpTo(seekPage);
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		if (LOWORD(wParam) == ID_ESCAPE) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
	case WM_SYSCOMMAND:
		if (LOWORD(wParam) == SC_CLOSE) {

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}		
	}
	return (INT_PTR)FALSE;
}
