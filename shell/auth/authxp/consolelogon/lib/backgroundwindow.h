#pragma once
#include <windows.h>

class   CBackgroundWindow
{
public:
	CBackgroundWindow(HINSTANCE hInstance);
	~CBackgroundWindow(void);

	HWND                    Create(void);
private:
	static  LRESULT     CALLBACK    WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
private:
	HINSTANCE               _hInstance;
	ATOM                    _atom;
	HWND                    _hwnd;

	static  const TCHAR             s_szWindowClassName[];
};
