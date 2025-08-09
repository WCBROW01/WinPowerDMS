#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "resource.h"

// Link Common Controls v6 for visual styling
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_ICON_ID 1001

typedef struct {
    int width;
    int height;
    int refresh;
} DISPLAY_MODE;

static DISPLAY_MODE modeBatt = { 0 };
static DISPLAY_MODE modeAC = { 0 };

BOOL DisplayModeEquals(const DISPLAY_MODE* a, const DISPLAY_MODE* b) {
    return a->width == b->width && a->height == b->height && a->refresh == b->refresh;
}

#define MODE_SELECTED(mode) (!DisplayModeEquals(&mode, &(DISPLAY_MODE) { 0 }))

// I hate parsing the string here, but the alternative requires extra memory management.
static DISPLAY_MODE GetModeFromCB(HWND hDlg, int nIDDlgItem) {
    DISPLAY_MODE mode = { 0 };
    HWND hComboBox = GetDlgItem(hDlg, nIDDlgItem);
    size_t selectedIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
    if (selectedIndex != CB_ERR) {
        LRESULT len = SendMessage(hComboBox, CB_GETLBTEXTLEN, selectedIndex, 0);
        if (len != CB_ERR) {
            LPWSTR text = HeapAlloc(GetProcessHeap(), 0, len * sizeof(*text));
            if (text) {
                SendMessage(hComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM) text);
                swscanf_s(text, L"%dx%d @ %d Hz", &mode.width, &mode.height, &mode.refresh);
                HeapFree(GetProcessHeap(), 0, text);
            }
        }
    }

    return mode;
}

// returns the result of the ChangeDisplaySettings call that this results in.
static LONG ChangeDisplayMode(const DISPLAY_MODE* mode, DWORD dwFlags) {
    DEVMODE devMode = {
        .dmSize = sizeof(devMode),
        .dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY,
        .dmPelsWidth = mode->width,
        .dmPelsHeight = mode->height,
        .dmDisplayFrequency = mode->refresh
    };

    return ChangeDisplaySettings(&devMode, dwFlags);
}

struct MessageBoxParams {
    HWND hWnd;
    LPCWSTR lpText;
    LPCWSTR lpCaption;
    UINT uType;
};

static DWORD WINAPI MessageBoxAsync(LPVOID lpParam) {
    struct MessageBoxParams* params = lpParam;
    return MessageBox(params->hWnd, params->lpText, params->lpCaption, params->uType);
}

struct TestDisplayModeParams {
    HWND hDlg;
    DISPLAY_MODE mode;
};

static DWORD WINAPI TestDisplayModeThread(LPVOID lpParam) {
    struct TestDisplayModeParams* params = lpParam;
    DEVMODE originalMode = { .dmSize = sizeof(originalMode) };
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &originalMode);
    ChangeDisplayMode(&params->mode, CDS_FULLSCREEN);

    // Create the message box about the resolution change
    WCHAR msgText[96];
    swprintf_s(msgText, sizeof(msgText) / sizeof(msgText[0]),
        L"Testing %dx%d @ %d Hz\nThe display mode will reset back in 10 seconds.",
        params->mode.width, params->mode.height, params->mode.refresh);
    struct MessageBoxParams mbParams = {
        .hWnd = params->hDlg,
        .lpText = msgText,
        .lpCaption = L"Resolution Test",
        .uType = MB_OK | MB_ICONINFORMATION
    };
    HANDLE mbThread = CreateThread(NULL, 0, MessageBoxAsync, &mbParams, 0, NULL);
    if (mbThread) {
        CloseHandle(mbThread);
        Sleep(10000);
        ChangeDisplaySettings(&originalMode, 0);
    }
    else {
        MessageBox(params->hDlg, L"Failed to test resolution.", L"Error", MB_OK | MB_ICONERROR);
    }
    HeapFree(GetProcessHeap(), 0, params);
    return 0;
}

static void TestDisplayMode(HWND hDlg, DISPLAY_MODE* mode) {
    struct TestDisplayModeParams* tdmParams = HeapAlloc(GetProcessHeap(), 0, sizeof(*tdmParams));
    if (tdmParams) {
        tdmParams->hDlg = hDlg;
        tdmParams->mode = *mode;
        HANDLE tdmThread = CreateThread(NULL, 0, TestDisplayModeThread, tdmParams, 0, NULL);
        if (tdmThread) CloseHandle(tdmThread);
        else {
            HeapFree(GetProcessHeap(), 0, tdmParams);
            MessageBox(hDlg, L"Failed to test resolution.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
    else {
        MessageBox(hDlg, L"Failed to test resolution.", L"Error", MB_OK | MB_ICONERROR);
    }
}

static INT_PTR CALLBACK PrefsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        HWND hComboBatt = GetDlgItem(hDlg, IDC_COMBO_BATT);
        HWND hComboAC = GetDlgItem(hDlg, IDC_COMBO_AC);

        // devMode object that will be enumerated
        DEVMODE devMode;
        ZeroMemory(&devMode, sizeof(devMode));
        devMode.dmSize = sizeof(devMode);

        size_t modeCount = 0;
        int modeNum = 0;
        DISPLAY_MODE lastMode = { 0 };
        while (EnumDisplaySettings(NULL, modeNum++, &devMode)) {
            DISPLAY_MODE currentMode = {
                .width = devMode.dmPelsWidth,
                .height = devMode.dmPelsHeight,
                .refresh = devMode.dmDisplayFrequency
            };

            if (!DisplayModeEquals(&currentMode, &lastMode)) {
                // Format resolution as "WidthxHeight @ Refresh Hz"
                WCHAR resText[32];
                swprintf_s(resText, sizeof(resText) / sizeof(resText[0]), L"%dx%d @ %d Hz",
                    currentMode.width, currentMode.height, currentMode.refresh);

                // Add to both combo boxes and check if this is the mode that was set.
                SendMessage(hComboBatt, CB_ADDSTRING, 0, (LPARAM)resText);
                if (DisplayModeEquals(&modeBatt, &currentMode))
                    SendMessage(hComboBatt, CB_SETCURSEL, modeCount, 0);

                SendMessage(hComboAC, CB_ADDSTRING, 0, (LPARAM)resText);
                if (DisplayModeEquals(&modeAC, &currentMode))
                    SendMessage(hComboAC, CB_SETCURSEL, modeCount, 0);
                
                ++modeCount;
                lastMode = currentMode;
            }
        }

        // select the highest display mode if one hasn't been selected yet
        if (!MODE_SELECTED(modeBatt)) SendMessage(hComboBatt, CB_SETCURSEL, modeCount - 1, 0);
        if (!MODE_SELECTED(modeAC)) SendMessage(hComboAC, CB_SETCURSEL, modeCount - 1, 0);
        return TRUE;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BUTTON_APPLY:
        case IDOK: {
            modeBatt = GetModeFromCB(hDlg, IDC_COMBO_BATT);
            modeAC = GetModeFromCB(hDlg, IDC_COMBO_AC);
            if (LOWORD(wParam) == IDC_BUTTON_APPLY) return TRUE;
        }
        case IDCANCEL: {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        case IDC_BUTTON_TEST_BATT: {
            DISPLAY_MODE mode = GetModeFromCB(hDlg, IDC_COMBO_BATT);
            TestDisplayMode(hDlg, &mode);
            return TRUE;
        }
        case IDC_BUTTON_TEST_AC: {
            DISPLAY_MODE mode = GetModeFromCB(hDlg, IDC_COMBO_AC);
            TestDisplayMode(hDlg, &mode);
            return TRUE;
        }
        break;
        }
    }
    }
    return FALSE;
}

enum TRAY_IDS {
    ID_TRAY_PREFS = 2001,
    ID_TRAY_ABOUT,
    ID_TRAY_EXIT
};

static NOTIFYICONDATA nid;
static HWND hWnd;
static HMENU hMenu;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // Required for menu to disappear correctly

            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_PREFS:
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PREFSDIALOG), hWnd, PrefsDialogProc);
            break;
        case ID_TRAY_ABOUT:
            MessageBoxW(NULL, L"WinPowerDMS\nA utility for switching the resolution of your laptop's display based on the current power state.", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;

    case WM_POWERBROADCAST:
        if (LOWORD(wParam) == PBT_POWERSETTINGCHANGE) {
            OutputDebugString(L"Power status changed\n");
            SYSTEM_POWER_STATUS powerStatus;
            if (GetSystemPowerStatus(&powerStatus)) {
                ChangeDisplayMode(powerStatus.ACLineStatus ? &modeAC : &modeBatt, CDS_UPDATEREGISTRY);
            }
        }
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize the controls
    InitCommonControls();

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TrayAppClass";

    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, L"TrayAppClass", NULL, 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL);
    RegisterPowerSettingNotification(hWnd, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // Create context menu
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_PREFS, L"Preferences");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, L"About");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    // Setup tray icon
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Replace this with my own icon at some point
    wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), L"WinPowerDMS");

    Shell_NotifyIcon(NIM_ADD, &nid);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}