#include "pch.h"
#include <lib/util.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct MinMaxWrapperContext
{
    WNDPROC origWndProc;
    LONG_PTR origUserData;
    RECT rect;
};

bool IsFullscreenMonitor(HWND hwnd, WINDOWPOS* windowpos)
{
    RECT windowRect;
    windowRect.left = windowpos->x;
    windowRect.top = windowpos->y;
    windowRect.right = windowpos->x + windowpos->cx;
    windowRect.bottom = windowpos->y + windowpos->cy;

    HMONITOR curMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo;
    ZeroMemory(&monitorInfo, sizeof(MONITORINFO));
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(curMonitor, &monitorInfo);

    return EqualRect(&windowRect, &monitorInfo.rcMonitor);
}

void GetEffectiveWindowRect(LPRECT windowRect, HWND hwnd, WINDOWPOS* windowpos)
{
    if (!(windowpos->flags & SWP_NOMOVE) && !(windowpos->flags & SWP_NOSIZE))
    {
        windowRect->left = windowpos->x;
        windowRect->top = windowpos->y;
        windowRect->right = windowpos->x + windowpos->cx;
        windowRect->bottom = windowpos->y + windowpos->cy;
    }
    else
    {
        GetWindowRect(hwnd, windowRect);
    }
}

bool GetRectMargins(LPRECT margins, LPCRECT inner, LPCRECT outer)
{
    RECT temp;
    SubtractRect(&temp, inner, outer);
    if (IsRectEmpty(&temp))
    {
        margins->left = outer->left - inner->left; 
        margins->top = outer->top - inner->top;
        margins->right = outer->right - inner->right;
        margins->bottom = outer->bottom - inner->bottom;
        return true;
    }

    return false;
}

LRESULT CALLBACK MinMaxWrapperWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Subclasses the WndProc for a single message so we can modify it
void MinMaxWrapperStartInterceptWinProc(HWND hwnd, RECT& rect)
{
    MinMaxWrapperContext* context = new MinMaxWrapperContext();
    context->origWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    context->origUserData = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    context->rect = rect;

    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)&MinMaxWrapperWinProc);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)context);
}

// Ends our temporary WndProc subclass
void MinMaxWrapperEndInterceptWinProc(HWND hwnd, WNDPROC& origWndProc, RECT& rect)
{
    MinMaxWrapperContext* context = (MinMaxWrapperContext*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)context->origUserData);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)context->origWndProc);

    origWndProc = context->origWndProc;
    rect = context->rect;

    delete context;
}

LRESULT CALLBACK MinMaxWrapperWinProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Restore original WndProc and UserData
    WNDPROC origWndProc;
    RECT rect;
    MinMaxWrapperEndInterceptWinProc(hwnd, origWndProc, rect);

    LRESULT result;

    if (uMsg == WM_GETMINMAXINFO)
    {
        MINMAXINFO* info = (MINMAXINFO*)lParam;

        // Get Default MINMAXINFO so we can use it to determine the real margins
        MINMAXINFO defaultInfo;
        CopyMemory(&defaultInfo, info, sizeof(MINMAXINFO));
        HMONITOR curMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo;
        ZeroMemory(&monitorInfo, sizeof(MONITORINFO));
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(curMonitor, &monitorInfo);
		
        // Add the margins using the MINMAX provided by DefWindowProc
        rect.left += defaultInfo.ptMaxPosition.x - monitorInfo.rcMonitor.left;
        rect.top += defaultInfo.ptMaxPosition.y - monitorInfo.rcMonitor.top;
        rect.right += defaultInfo.ptMaxPosition.x + defaultInfo.ptMaxSize.x - monitorInfo.rcMonitor.right;
        rect.bottom += defaultInfo.ptMaxPosition.y + defaultInfo.ptMaxSize.y - monitorInfo.rcMonitor.bottom;

        result = CallWindowProc(origWndProc, hwnd, uMsg, wParam, lParam);

        info->ptMaxPosition.x = rect.left;
        info->ptMaxPosition.y = rect.top;
        info->ptMaxSize.x = rect.right - rect.left;
        info->ptMaxSize.y = rect.bottom - rect.top;
    }
    else if (uMsg == WM_WINDOWPOSCHANGING)
    {
        WINDOWPOS* windowpos = reinterpret_cast<WINDOWPOS*>(lParam);

        RECT windowRectBefore;
        GetEffectiveWindowRect(&windowRectBefore, hwnd, windowpos);

        result = CallWindowProc(origWndProc, hwnd, uMsg, wParam, lParam);

        if (!(windowpos->flags & SWP_NOMOVE) && !(windowpos->flags & SWP_NOSIZE))
        {
            RECT windowRectAfter;
            GetEffectiveWindowRect(&windowRectAfter, hwnd, windowpos);

            // Get the monitor we'll be maximizing on
            HMONITOR curMonitor = MonitorFromRect(&windowRectAfter, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo;
            ZeroMemory(&monitorInfo, sizeof(MONITORINFO));
            monitorInfo.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(curMonitor, &monitorInfo);

            RECT margins;
            if (GetRectMargins(&margins, &monitorInfo.rcMonitor, &windowRectAfter) || GetRectMargins(&margins, &monitorInfo.rcWork, &windowRectAfter))
            {
                rect.left += margins.left;
                rect.top += margins.top;
                rect.right += margins.right;
                rect.bottom += margins.bottom;

                windowpos->x = rect.left;
                windowpos->y = rect.top;
                windowpos->cx = rect.right - rect.left;
                windowpos->cy = rect.bottom - rect.top;
            }
        }
    }
    else
    {
        // MinMaxWrapperWinProcHook makes sure this never happens, but passthrough if it does
        result = CallWindowProc(origWndProc, hwnd, uMsg, wParam, lParam);
    }

    return result;
}

LRESULT CALLBACK MinMaxWrapperWinProcHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        CWPSTRUCT* msg = reinterpret_cast<CWPSTRUCT*>(lParam);
        /*if (msg->message == WM_GETMINMAXINFO)
        {
            RECT rect = FancyZonesUtils::GetWindowMaximizedSizeAndOrigin(msg->hwnd);
            if (rect.left != rect.right)
            {
                // Set our custom wrapper as temporary WndProc
                MinMaxWrapperStartInterceptWinProc(msg->hwnd, rect);
            }
        }
        else */
        if (msg->message == WM_WINDOWPOSCHANGING)
        {
            WINDOWPOS* windowpos = reinterpret_cast<WINDOWPOS*>(msg->lParam);
            // Internal flag designating it's changing the window state
            bool windowStateChanging = (windowpos->flags & 0x8000);

            RECT rect = FancyZonesUtils::GetWindowMaximizedSizeAndOrigin(msg->hwnd);
            if (rect.left != rect.right)
            {        
                WINDOWPLACEMENT placement;
                placement.length = sizeof(placement);
                GetWindowPlacement(msg->hwnd, &placement);
                bool isMaximized = (placement.showCmd == SW_MAXIMIZE);
                bool isMinimized = (placement.showCmd == SW_MINIMIZE);

                // Edge goes from maximized to borderless fullscreen 
                bool isBorderlessFullscreen = !isMaximized && !isMinimized && IsFullscreenMonitor(msg->hwnd, windowpos);

                // Cancel if we are no longer maximized
                if (windowStateChanging && !isMaximized && !isMinimized && !isBorderlessFullscreen)
                    FancyZonesUtils::ResetWindowMaximizedSizeAndOrigin(msg->hwnd);
                else if (isMaximized || isBorderlessFullscreen)
                    MinMaxWrapperStartInterceptWinProc(msg->hwnd, rect);
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

extern "C" __declspec(dllexport) HHOOK __cdecl MinMaxAttachHook()
{
    HMODULE hmod = 0;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&MinMaxWrapperWinProcHook, &hmod))
    {
        return SetWindowsHookEx(WH_CALLWNDPROC, MinMaxWrapperWinProcHook, hmod, NULL);
    }

    return 0;
}

extern "C" __declspec(dllexport) bool __cdecl MinMaxDetachHook(HHOOK hhook)
{
    if (hhook && UnhookWindowsHookEx(hhook))
    {
        PostMessage(HWND_BROADCAST, WM_NULL, 0, 0L);
        Sleep(100);
        PostMessage(HWND_BROADCAST, WM_NULL, 0, 0L);

        return true;
    }

    return false;
}