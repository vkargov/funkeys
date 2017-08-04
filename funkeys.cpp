// funkeys.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "funkeys.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

#ifdef _DEBUG
#define LOG(expr)\
{\
std::ostringstream sm;\
sm << expr;\
OutputDebugStringA(sm.str().c_str());\
}
#else
#define LOG(expr)
#endif
#define LOGN(expr) LOG(expr << "\r\n")

LRESULT CALLBACK FixKeys(
	int nCode,
	WPARAM wParam,
	LPARAM lParam
)
{
	// kev supports extended flags
	// But it's 0 for my RControl, so I guess it just always sends LControl
	// Hence not bothering transferring that flag into keybd_evfent
	// And just treating both Control keys as "left".
	enum { HW_RETURN = 0x1C, HW_CONTROL = 0x1D, HW_CAPSLOCK = 0X3A };

	KBDLLHOOKSTRUCT* kev = (KBDLLHOOKSTRUCT*)lParam;

	// We'll need to remember some keystroke info about...
	// ... the past
	static BOOL wasUp = FALSE;
	static BYTE lastVk;
	static DWORD returnPressTimestamp = 0;
	// ... the present
	BOOL isUp = (kev->flags & LLKHF_UP) != 0;
	BOOL isInjected = (kev->flags & LLKHF_INJECTED) != 0;
	BYTE vk = kev->vkCode;
	// ... and the future
	BYTE newVk = 0, newScan = 0;
	DWORD newFlags = isUp ? KEYEVENTF_KEYUP : 0;

	LOGN("> Key " << std::hex << kev->scanCode << (isUp ? " up" : " down") << ((kev->flags & LLKHF_INJECTED) ? " (injected)" : ""));

	if (nCode == HC_ACTION && !isInjected) // TODO check if the second condition is really needed
	{
		switch (kev->scanCode)
		{
		case HW_RETURN:
			LOGN("  RETURN");
			if (!isUp)
			{
				if (lastVk != VK_RETURN)
				{
					returnPressTimestamp = GetTickCount();
					LOGN("  Remembering the timestamp.");
				}
				else
				{
					// emit control instead
					newVk = VK_RCONTROL, newScan = HW_CONTROL, newFlags |= KEYEVENTF_EXTENDEDKEY;
					LOGN("  Emitting RControl down.");
				}
			}
			else
			{
				if (lastVk == VK_RETURN && GetTickCount() - returnPressTimestamp < 200 /*ms*/)
				{
					newVk = VK_RETURN, newScan = HW_RETURN;
					LOGN("  Quick poke, sending return down & preparing for up");
					keybd_event(newVk, newScan, 0 /*down*/, 0);
					// up is sent at the end of the function

				}
				else
				{
					newVk = VK_RCONTROL, newScan = HW_CONTROL, newFlags |= KEYEVENTF_EXTENDEDKEY;
					LOGN("  Released slowly or after another key, emit control.");
				}
				lastVk = 0; // forget about it
			}
			break;
		//case HW_CONTROL:
		//	if (kev->vkCode == VK_LCONTROL)
		//	{
		//		LOGN("  LCONTROL");
		//		newVk = VK_CAPITAL, newScan = HW_CAPSLOCK;
		//	}
		//	else
		//	{
		//		/*idk*/;
		//		LOGN("  RCONTROL");
		//	}
		//	break;
		//case HW_CAPSLOCK:
		//	LOGN("  CAPSLOCK");
		//	newVk = VK_LCONTROL, newScan = HW_CONTROL;
		//	break;
		}

		if (lastVk == VK_RETURN && !isUp && kev->vkCode != VK_RETURN)
		{
			LOGN("  Pressed return in the past and now another key => emit control down and prepare for sending that key that key");
			keybd_event(VK_RCONTROL, HW_CONTROL, KEYEVENTF_EXTENDEDKEY, 0);
		}

		if (!isUp)
			lastVk = kev->vkCode;
	}

	if (newVk)
	{
		// Choke this keystroke and send the new one
		LOGN("  Key was substituted, issuing a new event keybd_event(vk = " <<
			std::hex << (long)newVk << ", scan = " << (long)newScan << ", flags = " << (long)newFlags << ", 0)");
		keybd_event(newVk, newScan, newFlags, 0);
		return 1;
	}
	else if (vk != VK_RETURN || isInjected)
	{
		// False alarm, didn't mean to catch this key. Carry on.
		LOGN("  Forwarding the event untouched to the remaining hooks.");
		return CallNextHookEx(NULL, nCode, wParam, lParam);
	}
	else
	{
		LOGN("  Skipping/choking uninjected return.")
		return 1;
	}
}

void RegisterIconThing(HINSTANCE hInst, HWND hWnd)
{
	NOTIFYICONDATA nid = {};
	nid.cbSize = sizeof(nid);
	nid.hWnd = hWnd;
	nid.uFlags = NIF_ICON | NIF_TIP | NIF_GUID;

	// Note: This is an example GUID only and should not be used.
	// Normally, you should use a GUID-generating tool to provide the value to
	// assign to guidItem.
	//static const GUID myGUID =
	//{ 0x23977b55, 0x10e0, 0x4041,{ 0xb8, 0x62, 0xb1, 0x95, 0x41, 0x96, 0x36, 0x69 } };
	//nid.guidItem = 0;
	// TODO figure out how to do it and enable

	// This text will be shown as the icon's tooltip.
	StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"Funkeys!");

	// Load the icon for high DPI.
	nid.hIcon=LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));

	// Show the notification.
	Shell_NotifyIcon(NIM_ADD, &nid) ? S_OK : E_FAIL;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
	HHOOK hFixKeys = SetWindowsHookEx(WH_KEYBOARD_LL, &FixKeys, hInstance, NULL);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_FUNKEYS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_FUNKEYS));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FUNKEYS));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_FUNKEYS);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	// Register the notification icon.
	RegisterIconThing(hInst, hWnd);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
