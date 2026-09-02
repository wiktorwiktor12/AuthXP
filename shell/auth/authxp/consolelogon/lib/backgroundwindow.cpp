#include "pch.h"
#include "backgroundwindow.h"

typedef char TBOOL;

CBackgroundWindow::CBackgroundWindow(HINSTANCE hInstance) :
    _hInstance(hInstance),
    _hwnd(NULL)

{
    WNDCLASSEX  wndClassEx;

    ZeroMemory(&wndClassEx, sizeof(wndClassEx));
    wndClassEx.cbSize = sizeof(wndClassEx);
    wndClassEx.lpfnWndProc = WndProc;
    wndClassEx.hInstance = hInstance;
    wndClassEx.lpszClassName = s_szWindowClassName;
    wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
    _atom = RegisterClassEx(&wndClassEx);
}

CBackgroundWindow::~CBackgroundWindow(void)

{
    if (_hwnd != NULL)
    {
        (BOOL)DestroyWindow(_hwnd);
    }
    if (_atom != 0)
    {
        TBOOL(UnregisterClass(MAKEINTRESOURCE(_atom), _hInstance));
    }
}

HWND    CBackgroundWindow::Create(void)

{
    HWND    hwnd = NULL;

#if     _DEBUG

    hwnd = NULL;

#else

    hwnd = CreateWindowEx(0,
        s_szWindowClassName,
        NULL,
        WS_POPUP,
        GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN),
        NULL, NULL, _hInstance, this);
    if (hwnd != NULL)
    {
        (BOOL)ShowWindow(hwnd, SW_SHOW);
        TBOOL(SetForegroundWindow(hwnd));
        (BOOL)EnableWindow(hwnd, FALSE);
    }

#endif

    return(hwnd);
}

LRESULT     CALLBACK    CBackgroundWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)

{
    LRESULT             lResult;
    CBackgroundWindow* pThis;

    pThis = reinterpret_cast<CBackgroundWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (uMsg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* pCreateStruct;

        pCreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<CBackgroundWindow*>(pCreateStruct->lpCreateParams);
        (LONG_PTR)SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        lResult = 0;
        break;
    }
    case WM_PAINT:
    {
        HDC             hdcPaint;
        PAINTSTRUCT     ps;

        hdcPaint = BeginPaint(hwnd, &ps);
        TBOOL(FillRect(ps.hdc, &ps.rcPaint, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH))));
        TBOOL(EndPaint(hwnd, &ps));
        lResult = 0;
        break;
    }
    default:
        lResult = DefWindowProc(hwnd, uMsg, wParam, lParam);
        break;
    }
    return(lResult);
}
const TCHAR     CBackgroundWindow::s_szWindowClassName[] = TEXT("LogonUIBackgroundWindowClass");

