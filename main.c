#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>

RECT chosenRect = {0};

typedef struct {
    RECT rect;
    WCHAR name[32];
} MonitorChoice;

#define MAX_DISPLAYS 16

MonitorChoice allMonitors[MAX_DISPLAYS];
int monitorCount = 0;

// The "Fingerprint" counter
BOOL CALLBACK CountGears(HWND hwnd, LPARAM lParam) {
    int* count = (int*)lParam;
    (*count)++;
    return TRUE;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    if (monitorCount >= MAX_DISPLAYS) return FALSE;
    
    MONITORINFOEXW info;
    info.cbSize = sizeof(MONITORINFOEXW);
    if (GetMonitorInfo(hMonitor, (LPMONITORINFO)&info)) {
        allMonitors[monitorCount].rect = info.rcMonitor;
        wcscpy(allMonitors[monitorCount].name, info.szDevice);
        monitorCount++;
    }
    return TRUE; 
}

void ForceMoveMusic(HWND hwnd) {
    ShowWindow(hwnd, SW_RESTORE);
    Sleep(100); 

    // Force removal of WS_CAPTION and WS_THICKFRAME styles left behind
    // by Apple Music when entering fullscreen mode on secondary displays.
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

    int targetW = chosenRect.right - chosenRect.left;
    int targetH = chosenRect.bottom - chosenRect.top;

    SetWindowPos(hwnd, HWND_TOP, 
                 chosenRect.left, chosenRect.top, 
                 targetW, targetH, 
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
                 
    UpdateWindow(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
    switch(uMsg) {
        case WM_CREATE:
            CreateWindowW(L"STATIC", L"Select destination monitor:", WS_VISIBLE | WS_CHILD, 10, 10, 250, 20, hwnd, NULL, NULL, NULL);
            for(int i=0; i < monitorCount; i++) {
                WCHAR btnText[64];
                swprintf(btnText, 64, L"Monitor %d (%dx%d)", i+1, 
                         allMonitors[i].rect.right - allMonitors[i].rect.left,
                         allMonitors[i].rect.bottom - allMonitors[i].rect.top);
                CreateWindowW(L"BUTTON", btnText, WS_VISIBLE | WS_CHILD, 10, 40 + (i * 40), 250, 30, hwnd, (HMENU)(INT_PTR)(i+1), NULL, NULL);
            }
            break;
        case WM_COMMAND:
            int wmId = LOWORD(wParam);
            if(wmId > 0 && wmId <= monitorCount) {
                chosenRect = allMonitors[wmId-1].rect;
                DestroyWindow(hwnd);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main() {
    SetProcessDPIAware();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);

    //Create the simplified monitor picker
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"MonitorSelectorClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwndUI = CreateWindowExW(0, L"MonitorSelectorClass", L"Monitor Setup", 
                                  WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, 
                                  CW_USEDEFAULT, CW_USEDEFAULT, 300, 100 + (monitorCount * 40), 
                                  NULL, NULL, GetModuleHandle(NULL), NULL);
    ShowWindow(hwndUI, SW_SHOW);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (chosenRect.right == 0 && chosenRect.bottom == 0) {
        printf("No monitor selected. Exiting.\n");
        return 0;
    }

    printf("Automation active. Watching for 3-Gear Player...\n");

    while (1) {
        HWND searchHwnd = NULL;
        while ((searchHwnd = FindWindowExW(NULL, searchHwnd, L"WinUIDesktopWin32WindowClass", L"Apple Music"))) {
            int gears = 0;
            EnumChildWindows(searchHwnd, CountGears, (LPARAM)&gears);

            // Apple Music (WinUI 3) instantiates multiple top-level HWNDs with the same class
            // Enumerating child windows isolates the 3-element active viewport from the main application
            if (gears == 3 && IsWindowVisible(searchHwnd)) {
                RECT r;
                GetWindowRect(searchHwnd, &r);
                
                LONG_PTR style = GetWindowLongPtr(searchHwnd, GWL_STYLE);
                BOOL hasTitleBar = (style & WS_CAPTION);
                BOOL onWrongMonitor = (r.left < chosenRect.left - 50 || r.left > chosenRect.left + 50);

                // Trigger if it's in the wrong place OR if it still has labels/title bar
                if (onWrongMonitor || hasTitleBar) {
                    printf("Player detected. Removing borders/moving...\n");
                    ForceMoveMusic(searchHwnd);
                }
            }
        }
        Sleep(1000); 
    }
    return 0;
}