#include "CursorTracker.h"
import std;

#define MAX_LOADSTRING 100
#define PULSE_ID 10
#define CLOSE_ID 11
#define INSTRUCTIONS_ID 12
#define KEYS_SIZE 256
#define NOTIFICATION_TRAY_ICON_MSG (WM_USER + 0x100)

constexpr LPCWSTR settings_path = L"./settings.ini";
constexpr LPCWSTR open_settings_path = L"settings.ini";
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
NOTIFYICONDATA nid;

bool should_pulse = true;
bool show_instructions = true;
struct circular_buffer buffer;

HWND instructions_hwnd;
POINT m_screen_size{ GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
COLORREF circle_color = RGB(255, 255, 0);

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL				MyShellNotifyIcon(HINSTANCE, HWND);
void				ShowErrorMessage();
void				SaveSettings();
void				LoadSettings();
void				PickColor();
std::wstring		CreateKeysString();
LRESULT CALLBACK	RegisterKey(int, WPARAM, LPARAM);

std::array<bool, KEYS_SIZE>keys{ false };

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadSettings();

	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_CURSOR_TRACKER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	SaveSettings();
	Shell_NotifyIcon(NIM_DELETE, &nid);
	return (int)msg.wParam;
}



ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = szWindowClass;

	return RegisterClassExW(&wcex);
}

BOOL MyShellNotifyIcon(HINSTANCE hInstance, HWND hWnd)
{
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = NOTIFICATION_TRAY_ICON_MSG;
	nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	LoadString(hInstance, IDS_APP_TITLE, nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]));
	return Shell_NotifyIcon(NIM_ADD, &nid);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;


	HWND hWnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
		szWindowClass, szTitle, WS_POPUP | WS_DISABLED, 0, 0, m_screen_size.x, m_screen_size.y, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	if (!MyShellNotifyIcon(hInstance, hWnd))
	{
		ShowErrorMessage();
		return FALSE;
	}

	if (!RegisterHotKey(hWnd, PULSE_ID, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 0x43)
		|| !RegisterHotKey(hWnd, CLOSE_ID, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_F4)
		|| !RegisterHotKey(hWnd, INSTRUCTIONS_ID, MOD_CONTROL | MOD_NOREPEAT, VK_F12))
	{
		ShowErrorMessage();
		return FALSE;
	}


	BYTE alpha = 255 * 20 / 100;
	SetLayeredWindowAttributes(hWnd, 0, alpha, LWA_ALPHA | LWA_COLORKEY);
	SetWindowsHookEx(WH_KEYBOARD_LL, RegisterKey, hInstance, 0);

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	instructions_hwnd = CreateWindowEx(WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"STATIC", L"Instructions",
		WS_POPUP | WS_CHILD | SS_CENTER, m_screen_size.x / 2 - 150, m_screen_size.y / 2 - 100, 300, 100, hWnd, nullptr, hInstance, nullptr);

	if (!instructions_hwnd)
	{
		return FALSE;
	}
	SetLayeredWindowAttributes(instructions_hwnd, 0, 255, LWA_ALPHA);

	const wchar_t* instructionsText = L" - Press Ctrl + F12 to toggle instructions\n"
		L"- Press Alt + Shift + C to toggle pulsing\n"
		L"- Press Alt + Shift + F4 to close the app\n"
		L"- Use system tray to open or reload\nconfig or to pick a circle color";

	SendMessage(instructions_hwnd, WM_SETTEXT, 0, (LPARAM)instructionsText);

	if (show_instructions)
		ShowWindow(instructions_hwnd, nCmdShow);
	UpdateWindow(instructions_hwnd);
	return TRUE;
}



LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int radius = 50;
	static int stepSize = -1;
	constexpr int width = 300;
	constexpr int margin = 10;
	switch (message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hWnd, &ps);
			HDC hdcBuffer = CreateCompatibleDC(hdc);
			HBITMAP hbmBuffer = CreateCompatibleBitmap(hdc, m_screen_size.x, m_screen_size.y);
			HBITMAP hbmOldBuffer = (HBITMAP)SelectObject(hdcBuffer, hbmBuffer);

			int current = buffer.head;
			int i = 0;
			int height = buffer.element_count * 50 + (buffer.element_count - 1) * margin;
			HBRUSH rect_brush = CreateSolidBrush(RGB(254, 254, 254));
			COLORREF oldColor = SetBkColor(hdcBuffer, RGB(254, 254, 254));
			while (i < buffer.element_count)
			{
				RECT rc = { margin, m_screen_size.y - (50 + margin) * (i + 1), width, m_screen_size.y - (50 + margin) * i - margin };
				FillRect(hdcBuffer, &rc, rect_brush);
				DrawText(hdcBuffer, buffer.buffer[current].c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
				++i;
				current = (current + 1) % buffer.size;
			}
			SetBkColor(hdcBuffer, oldColor);
			DeleteObject(rect_brush);

			POINT mouse_pos;
			GetCursorPos(&mouse_pos);
			HBRUSH brush = CreateSolidBrush(circle_color);
			HBRUSH oldBrush = (HBRUSH)SelectObject(hdcBuffer, brush);
			HPEN pen = CreatePen(PS_SOLID, 1, circle_color);
			HPEN oldPen = (HPEN)SelectObject(hdcBuffer, pen);
			Ellipse(hdcBuffer, mouse_pos.x - radius, mouse_pos.y - radius,
				mouse_pos.x + radius, mouse_pos.y + radius);
			SelectObject(hdcBuffer, oldBrush);
			SelectObject(hdcBuffer, oldPen);
			DeleteObject(brush);
			DeleteObject(pen);


			BitBlt(hdc, 0, 0, m_screen_size.x, m_screen_size.y, hdcBuffer, 0, 0, SRCCOPY);
			DeleteObject(hbmOldBuffer);
			DeleteDC(hdcBuffer);
			DeleteObject(hbmBuffer);
			EndPaint(hWnd, &ps);
		}
		break;
		case WM_ERASEBKGND:
			return 1;
		case WM_CREATE:
		{
			SetTimer(hWnd, 7, 1, nullptr);
			SetTimer(hWnd, 8, 40, nullptr);
		}
		break;
		case WM_TIMER:
		{
			if (wParam == 7)
			{
				InvalidateRect(hWnd, nullptr, TRUE);
			}
			else if (wParam == 8)
			{
				if (should_pulse || radius < 50)
				{
					radius += stepSize;
					if (radius <= 25)
					{
						radius = 25;
						stepSize *= -1;
					}
					else if (radius >= 50)
					{
						radius = 50;
						stepSize *= -1;
					}
				}
				buffer.IncreaseTime(40);
				if (buffer.GetFirstTime() >= 5000)
					buffer.Pop();
			}
		}
		break;
		case WM_HOTKEY:
		{
			switch (wParam)
			{
				case PULSE_ID:
					should_pulse = !should_pulse;
					break;
				case CLOSE_ID:
					DestroyWindow(hWnd);
					break;
				case INSTRUCTIONS_ID:
					show_instructions = !show_instructions;
					ShowWindow(instructions_hwnd, show_instructions ? SW_SHOW : SW_HIDE);
					break;
			}
		}
		break;
		// https://stackoverflow.com/questions/68474486/creating-system-tray-right-click-menu-c
		case NOTIFICATION_TRAY_ICON_MSG:
		{
			switch (LOWORD(lParam))
			{
				case WM_RBUTTONDOWN:
				case WM_CONTEXTMENU:
				{
					POINT pt;
					GetCursorPos(&pt);

					HMENU hmenu = CreatePopupMenu();
					InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_EXIT, L"Exit");
					InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_OPEN_CONFIG, L"Open config file");
					InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_RELOAD_CONFIG, L"Reload config");
					InsertMenu(hmenu, 0, MF_BYPOSITION | MF_STRING, IDM_PICK_COLOR, L"Pick color...");

					SetForegroundWindow(hWnd);

					int cmd = TrackPopupMenu(hmenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, nullptr);

					switch (cmd)
					{
						case IDM_EXIT:
							DestroyWindow(hWnd);
							break;
						case IDM_OPEN_CONFIG:
							ShellExecute(nullptr, L"open", open_settings_path, nullptr, nullptr, SW_SHOWNORMAL);
							break;
						case IDM_RELOAD_CONFIG:
							LoadSettings();
							break;
						case IDM_PICK_COLOR:
							PickColor();
							break;
					}
				}
				break;
			}
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


void ShowErrorMessage()
{
	DWORD lastError = GetLastError();
	LPWSTR errorMessage;
	const int bufSize = 256;
	TCHAR buf[bufSize];
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errorMessage, 0, nullptr);
	if (errorMessage != nullptr)
	{
		_stprintf_s(buf, bufSize, _T("Failed with error %d:\n%s"), lastError, errorMessage);
		MessageBox(nullptr, buf, L"Error", MB_OK);
		LocalFree(errorMessage);
	}
}

void SaveSettings()
{
	WritePrivateProfileString(L"Settings", L"Color", std::to_wstring(circle_color).c_str(), settings_path);
	WritePrivateProfileString(L"Settings", L"Instructions", show_instructions ? L"TRUE" : L"FALSE", settings_path);
	WritePrivateProfileString(L"Settings", L"Pulse", should_pulse ? L"TRUE" : L"FALSE", settings_path);
}

void LoadSettings()
{
	TCHAR Buffer[50];
	GetPrivateProfileString(L"Settings", L"Color", L"0", Buffer, 50, settings_path);
	circle_color = _wtoi(Buffer);
	GetPrivateProfileString(L"Settings", L"Instructions", L"TRUE", Buffer, 50, settings_path);
	show_instructions = wcscmp(Buffer, L"TRUE") == 0;
	GetPrivateProfileString(L"Settings", L"Pulse", L"TRUE", Buffer, 50, settings_path);
	should_pulse = wcscmp(Buffer, L"TRUE") == 0;
}

void PickColor()
{
	CHOOSECOLOR cc{};
	static COLORREF acrCustClr[16];
	cc.lStructSize = sizeof(cc);
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult = circle_color;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	if (ChooseColor(&cc))
	{
		circle_color = cc.rgbResult;
	}
}



LRESULT RegisterKey(int code, WPARAM wParam, LPARAM lParam)
{
	LPKBDLLHOOKSTRUCT p = (LPKBDLLHOOKSTRUCT)lParam;
	std::wstring keysString;
	if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
	{
		keys[p->vkCode] = true;
		keysString = CreateKeysString();
		buffer.Push(keysString);
	}
	else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
	{
		keys[p->vkCode] = false;
	}

	return CallNextHookEx(nullptr, code, wParam, lParam);
}


std::wstring CreateKeysString()
{
	int firstKeyIndex;
	std::wstring keysString;
	TCHAR buffer[50];
	for (firstKeyIndex = 0; firstKeyIndex < KEYS_SIZE; firstKeyIndex++)
	{
		if (keys[firstKeyIndex])
		{
			unsigned int scanCode = MapVirtualKey(firstKeyIndex, MAPVK_VK_TO_VSC);
			GetKeyNameText(scanCode << 16, buffer, 50);
			keysString = buffer;
			break;
		}
	}
	for (int i = firstKeyIndex + 1; i < KEYS_SIZE; i++)
	{
		if (keys[i])
		{
			unsigned int scanCode = MapVirtualKey(i, MAPVK_VK_TO_VSC);
			GetKeyNameText(scanCode << 16, buffer, 50);
			keysString += L" + ";
			keysString += buffer;
		}
	}
	return keysString;
}