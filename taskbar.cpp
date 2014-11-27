#include <windows.h>
#include <wininet.h>
#include <atlstr.h>

#include "resource.h"

#pragma comment(lib, "wininet.lib")

#define LIST_CAP 16
#define ITEM_LEN 512

#define NID_UID										123
#define WM_TASKBARNOTIFY							(WM_USER + 100)	/* 0x464 */

#define WM_TASKBARNOTIFY_MENUITEM_SHOW				(WM_USER + 101)	/* 0x465 */
#define WM_TASKBARNOTIFY_MENUITEM_HIDE				(WM_USER + 102)	/* 0x466 */
#define WM_TASKBARNOTIFY_MENUITEM_RELOAD			(WM_USER + 103)	/* 0x467 */
#define WM_TASKBARNOTIFY_MENUITEM_ABOUT				(WM_USER + 104)	/* 0x468 */
#define WM_TASKBARNOTIFY_MENUITEM_EXIT				(WM_USER + 105)	/* 0x469 */

// PAC URL is base, and then other is after it.
#define WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE	(WM_USER + 200)	/* 0x4c8 */
// Reserved ..
#define WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_END     (WM_USER + 200 + LIST_CAP)

HINSTANCE hInst;			// EXE's Instance. the entry of main function.
HWND hWnd;					// create window.
HWND hConsole;				// create console.
HMENU hMenu, hSubMenu;		// pop up menu.

const CString g_cfgpath = ".\\taskbar.ini";

CString g_cmdtitle;		    // cmd window title.
CString g_cmdline;			// cmd line.
CString g_arguments;		// arguments. not use.
CString g_bartitle;			// task bar title.
CString g_visible;			// cmd window is visible at first.
CString g_traytip;			// task bar tip
CString g_balloom;			// task bar balloon message.
CString g_pac;				// PAC URL
CString g_proxylist[LIST_CAP];
CString g_envlist[LIST_CAP];	
INT g_currentproxyid;		// index which proxy used.
DWORD g_CmdProcessId;		// CMD's process id.
CString g_wndclassname = "Taskbar";	// register window's name.

// proxy type.
typedef enum tagProxyType
{
	PROXY_DIRECT,
	PROXY_PAC,
	PROXY_SERVER
}EN_PROXY_TYPE;

// array size.
#define ARRAY_SIZE(AX) ( sizeof(AX)/sizeof(AX[0]) )

BOOL TrayIconOperate(DWORD dwOp, CString info)
{
	NOTIFYICONDATA NotifyData	= {0};
	NotifyData.cbSize			= (DWORD)sizeof(NOTIFYICONDATA);
	NotifyData.hWnd				= hWnd;
	NotifyData.uID				= NID_UID;
	NotifyData.uFlags			= NIF_ICON|NIF_MESSAGE|NIF_INFO|NIF_TIP;
	NotifyData.dwInfoFlags		= NIIF_INFO;
	NotifyData.uCallbackMessage = WM_TASKBARNOTIFY;
	NotifyData.hIcon			= LoadIcon(hInst, (LPCTSTR)IDI_SMALL);

	// tip
	StrCpy(NotifyData.szTip, g_traytip);

	// notify info. only change this.
	StrCpy(NotifyData.szInfoTitle, g_bartitle);
	StrCpy(NotifyData.szInfo, info);
	
	return Shell_NotifyIcon(dwOp, &NotifyData);
}

BOOL GetSystemProxy(EN_PROXY_TYPE& ProxyType, CString& ProxyStr)
{
	INTERNET_PER_CONN_OPTION_LIST    List; 
	INTERNET_PER_CONN_OPTION         Option[3]; 
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST); 

	Option[0].dwOption = INTERNET_PER_CONN_FLAGS;			// from flags, we know the proxy's use states. 
	Option[1].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;	// PAC
	Option[2].dwOption = INTERNET_PER_CONN_PROXY_SERVER;	// proxy server.

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST); 
	List.pszConnection = NULL; 
	List.dwOptionCount = 3; 
	List.dwOptionError = 0; 
	List.pOptions = Option; 

	if(!InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize)) 
	{
		printf("InternetQueryOption failed! (%d)\n", GetLastError()); 
		return FALSE;
	}

	if((Option[0].Value.dwValue & INTERNET_PER_CONN_AUTOCONFIG_URL) != NULL)
	{
		ProxyStr.SetString(Option[1].Value.pszValue);
		GlobalFree(Option[1].Value.pszValue);
		ProxyType = PROXY_PAC;
	}
	else if((Option[0].Value.dwValue & INTERNET_PER_CONN_PROXY_SERVER) != NULL)
	{
		ProxyStr.SetString(Option[2].Value.pszValue);
		GlobalFree(Option[2].Value.pszValue);
		ProxyType = PROXY_SERVER;
	}
	else if ((Option[0].Value.dwValue & PROXY_TYPE_DIRECT) != NULL)
	{
		ProxyStr.SetString("Direct Connection!");
		ProxyType = PROXY_DIRECT;
	}
	else
	{
		printf("Exception: %ld\n", Option[0].Value.dwValue);
		return FALSE;
	}

	return TRUE;
}

BOOL SetSystemProxy(EN_PROXY_TYPE ProxyType, CString ProxyStr)
{
	INTERNET_PER_CONN_OPTION_LIST    List; 
	INTERNET_PER_CONN_OPTION         Option[3]; 
	ULONG							 nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST); 

	DWORD ConFlag = 0;
	DWORD ConOption = 0;
	DWORD UseOptCount = 3;

	if (ProxyType == PROXY_PAC)
	{
		ConOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
		ConFlag = PROXY_TYPE_AUTO_PROXY_URL;
	} 
	else if (ProxyType == PROXY_SERVER)
	{
		ConOption = INTERNET_PER_CONN_PROXY_SERVER;
		ConFlag = PROXY_TYPE_PROXY;
	}
	else if(ProxyType == PROXY_DIRECT)
	{
		UseOptCount = 1;
	}
	else
	{
		return FALSE;
	}

	Option[0].dwOption = INTERNET_PER_CONN_FLAGS; 
	Option[0].Value.dwValue = ConFlag; 
	Option[0].Value.dwValue |= PROXY_TYPE_DIRECT; 

	Option[1].dwOption = ConOption; 
	Option[1].Value.pszValue = (LPSTR)(LPCSTR)ProxyStr; 

	Option[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS; 
	Option[2].Value.pszValue = "<local>"; 

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST); 
	List.pszConnection = NULL; 
	List.dwOptionCount = UseOptCount; 
	List.dwOptionError = 0; 
	List.pOptions = Option; 

	if(!InternetSetOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, nSize))
	{
		printf("InternetSetOption failed! (%d)\n", GetLastError()); 
		return FALSE;
	}

	InternetSetOption(NULL, INTERNET_OPTION_REFRESH, NULL,NULL); 

	return TRUE;
}

BOOL UpdateProxyCurrendId(INT id)
{
	if (id != -1)
	{
		g_currentproxyid = id;
		return TRUE;
	}

	CString ProxyStr;
	EN_PROXY_TYPE ProxyType;
	GetSystemProxy(ProxyType, ProxyStr);
	
	g_currentproxyid = -1;
	
	if (ProxyType == PROXY_DIRECT)
		g_currentproxyid = 0; 
	else if (ProxyType == PROXY_PAC && 0 == ProxyStr.Compare(g_pac))
		g_currentproxyid = 1;
	else if (ProxyType = PROXY_SERVER)
	{
		CString *pCell = g_proxylist;
		int i = 0;
		while (*pCell != "")
		{
			if (ProxyStr.Compare(*pCell) == 0)
			{
				g_currentproxyid = i + 2;
				break;
			}
			i++;
			pCell++;
		}
	}
	else
	{
		// error.
		return FALSE;
	}

	return TRUE;
}

BOOL CreateTrayMenu()
{
	// Create SubMenu and set it.
	hSubMenu = CreatePopupMenu();
	AppendMenu(hSubMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE, "");
	AppendMenu(hSubMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE+1, "");
	CString* p=g_proxylist;
	for (int i=0; *p!=""; p++,i++)
		AppendMenu(hSubMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + 2 + i, "");
	
	// Create MainMenu and set it.
	hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_SHOW, "显示");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_HIDE, "隐藏");
	AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT_PTR)hSubMenu, "设置全局代理");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_RELOAD, "重新载入");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_ABOUT, "关于");
	AppendMenu(hMenu, MF_STRING, WM_TASKBARNOTIFY_MENUITEM_EXIT, "退出");

	return TRUE;
}

BOOL ShowTrayMenu()
{
	POINT pt; GetCursorPos(&pt);

	ModifyMenu(hSubMenu, 0, MF_STRING | MF_BYPOSITION, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE, "禁止代理");
	ModifyMenu(hSubMenu, 1, MF_STRING | MF_BYPOSITION, WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + 1, g_pac);
	CString* p=g_proxylist;
	for (int i=0; *p!=""; p++,i++)
		ModifyMenu(hSubMenu, i+2, MF_STRING|MF_BYPOSITION,	\
				   WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + 2 + i, \
				   g_proxylist[i]);

	// check the submenu item.
	if (g_currentproxyid == 0)
		ModifyMenu(hSubMenu, 0, MF_STRING|MF_CHECKED|MF_BYPOSITION,	\
				   WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE, "禁止代理");
	else if (g_currentproxyid == 1)
		ModifyMenu(hSubMenu, 1, MF_STRING|MF_CHECKED|MF_BYPOSITION,	\
				   WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + 1, g_pac);
	else if (g_currentproxyid > 1)
		ModifyMenu(hSubMenu, g_currentproxyid,	   \
				  MF_STRING|MF_CHECKED|MF_BYPOSITION,	\
				  WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + g_currentproxyid, \
				  g_proxylist[g_currentproxyid - 2]);
	else{}

	TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
	// TODO: It work??
	//PostMessage(hWnd, WM_NULL, 0, 0);

	return TRUE;
}

BOOL CreateHideWindow()
{
   if (!( hWnd = CreateWindow(g_wndclassname, g_cmdtitle, WS_OVERLAPPED|WS_SYSMENU, \
							  NULL, NULL, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL)))
   	  return FALSE;
   
   ShowWindow(hWnd, SW_HIDE);
   UpdateWindow(hWnd);

   return TRUE;
}

BOOL SetCurrentDir()
{
	TCHAR szPath[MAX_PATH] = {0};

	GetModuleFileName(NULL, szPath, ARRAY_SIZE(szPath)-1);
	*StrRChr(szPath, NULL, '\\') = '\0';

	SetCurrentDirectory(szPath);
	
	SetEnvironmentVariable("CWD", szPath);
	
	return TRUE;
}

BOOL Semicolon2List(CString sem, CString*list, int size)
{
	int pos = 0;
	CString token = sem.Tokenize(";", pos);
	for (int i=0; !token.IsEmpty(); i++)
	{
		if (i == size)
			return FALSE;
		list[i] = token;
		token = sem.Tokenize(";", pos);
	}

	return TRUE;
}

BOOL SetConfigInfo()
{
	CString temp;
	
		GetPrivateProfileString("Task_Bar", "Visible", "NULL", g_visible.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_visible.ReleaseBuffer();

	GetPrivateProfileString("Task_Bar", "TrayTip", "NULL", g_traytip.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_traytip.ReleaseBuffer();

	GetPrivateProfileString("Task_Bar", "Title", "NULL", g_bartitle.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_bartitle.ReleaseBuffer();

	GetPrivateProfileString("Task_Bar", "Balloon", "NULL", g_balloom.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_balloom.ReleaseBuffer();
	
	GetPrivateProfileString("Command_Line", "Environments", "NULL", temp.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	temp.ReleaseBuffer();
	Semicolon2List(temp, g_envlist, LIST_CAP);
	temp.Empty();

	GetPrivateProfileString("Command_Line", "Command", "NULL", g_cmdline.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_cmdline.ReleaseBuffer();

	GetPrivateProfileString("Command_Line", "Title", "NULL", g_cmdtitle.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_cmdtitle.ReleaseBuffer();

	GetPrivateProfileString("Command_Line", "Arguments", "NULL", g_arguments.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_arguments.ReleaseBuffer();

	GetPrivateProfileString("IE_Proxy", "Proxy", "NULL", temp.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	temp.ReleaseBuffer();
	Semicolon2List(temp, g_proxylist, LIST_CAP);
	temp.Empty();

	GetPrivateProfileString("IE_Proxy", "Pac", "NULL", g_pac.GetBuffer(ITEM_LEN), ITEM_LEN, g_cfgpath);
	g_pac.ReleaseBuffer();

	return TRUE;
}

BOOL CreateConsole()
{
	AllocConsole();
	freopen("CONIN$",  "r+t", stdin);
	freopen("CONOUT$", "w+t", stdout);

	hConsole = GetConsoleWindow();
	SetWindowText(hConsole, g_cmdtitle);

	if (g_visible == "0")
		ShowWindow(hConsole, SW_HIDE);
	else
		SetForegroundWindow(hConsole);

	return TRUE;
}

BOOL ExecCmdline()
{
	STARTUPINFO StartUpInfo = { sizeof(StartUpInfo) };
	PROCESS_INFORMATION ProcessInfo;
	StartUpInfo.dwFlags = STARTF_USESHOWWINDOW;
	StartUpInfo.wShowWindow = TRUE;
	if (CreateProcess(NULL, (LPSTR)(LPCSTR)g_cmdline, \
					  NULL, NULL, FALSE, NULL, NULL, NULL, 
					  &StartUpInfo, &ProcessInfo))
		// TODO: pi.dwProcessId is same with MyGetProcesId.
		//g_CmdProcessId = MyGetProcessId(pi.hProcess);
		g_CmdProcessId = ProcessInfo.dwProcessId;
	else
	{
		printf("ExecCmdline \"%s\" failed!\n", g_cmdline);
		MessageBox(NULL, g_cmdline, "Error: 执行命令失败!", MB_OK);
		ExitProcess(0);
	}

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	return TRUE;
}

BOOL ReloadCmdline()
{
	HANDLE hProcess=OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_CmdProcessId);
	if(hProcess==NULL)
	{
		printf("OpenProcess error: %d\n", GetLastError());
		return FALSE;
	}
	if(!TerminateProcess(hProcess,0))
	{
		printf("TerminateProcess error: %d\n", GetLastError());
		return FALSE;
	}

	ShowWindow(hConsole, SW_SHOW);
	SetForegroundWindow(hConsole);
	printf("\n\n");
	Sleep(200);
	ExecCmdline();

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//static const UINT WM_TASKBARCREATED = ::RegisterWindowMessage(L"TaskbarCreated");
	//int nID;
	switch (message)
	{	
		// TASKBAR NOTIFY.
		case WM_TASKBARNOTIFY:
			{	// left button up.
				if (lParam == WM_LBUTTONUP)
				{
					ShowWindow(hConsole, !IsWindowVisible(hConsole));
					SetForegroundWindow(hConsole);
				}
				// right button up.
				else if (lParam == WM_RBUTTONUP)
				{
					SetForegroundWindow(hWnd);
					UpdateProxyCurrendId(-1);
					ShowTrayMenu();
				}
				break;
			}
		case WM_COMMAND:
			{
				UINT nID = LOWORD(wParam);
				if (nID == WM_TASKBARNOTIFY_MENUITEM_SHOW)
				{
					ShowWindow(hConsole, SW_SHOW);
					SetForegroundWindow(hConsole);
				}
				else if (nID == WM_TASKBARNOTIFY_MENUITEM_HIDE)
				{
					ShowWindow(hConsole, SW_HIDE);
				}
				if (nID == WM_TASKBARNOTIFY_MENUITEM_RELOAD)
				{
					ReloadCmdline();
				}
				else if (nID == WM_TASKBARNOTIFY_MENUITEM_ABOUT)
				{
					MessageBox(hWnd, "For Study!\n ", g_wndclassname, MB_OK | MB_ICONWARNING);
				}
				else if (nID == WM_TASKBARNOTIFY_MENUITEM_EXIT)
				{
					TrayIconOperate(NIM_DELETE, "");
					//DeleteTrayIcon();
					PostMessage(hConsole, WM_CLOSE, 0, 0);
				}
				else if (WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE <= nID && \
						 nID < WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE + LIST_CAP)
				{
					UpdateProxyCurrendId(nID - WM_TASKBARNOTIFY_MENUITEM_PROXYLIST_BASE);
					
					if (g_currentproxyid == 0)
					{
						SetSystemProxy(PROXY_DIRECT, "");
						TrayIconOperate(NIM_MODIFY, "禁止代理");
					}
					else if (g_currentproxyid == 1)
					{
						SetSystemProxy(PROXY_PAC, g_pac);
						TrayIconOperate(NIM_MODIFY, g_pac);
					}
					else
					{
						SetSystemProxy(PROXY_SERVER, g_proxylist[g_currentproxyid-2]);
						TrayIconOperate(NIM_MODIFY, g_proxylist[g_currentproxyid-2]);
					}
				}
				break;
			}
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

ATOM MyRegisterClass()
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInst;
	wcex.hIcon			= LoadIcon(hInst, (LPCTSTR)IDI_TASKBAR);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCTSTR)NULL;
	wcex.lpszClassName	= g_wndclassname;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	return RegisterClassEx(&wcex);
}

int WINAPI WinMain(
	HINSTANCE hInstance,      // handle to current instance
	HINSTANCE hPrevInstance,  // handle to previous instance
	LPSTR lpCmdLine,          // command line
	int nCmdShow              // show state
	)
{
	hInst = hInstance;

	// Global Variable. so do not need parameter.
	SetCurrentDir();
	SetConfigInfo();
	MyRegisterClass();
	CreateHideWindow();
	CreateConsole();
	ExecCmdline();
	TrayIconOperate(NIM_ADD, g_balloom);
	CreateTrayMenu();

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}

