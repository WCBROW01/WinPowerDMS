#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#define NTDDI_VERSION NTDDI_VISTA

#include <stdio.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "resource.h"
#include "DisplayMode.h"

// Link Common Controls v6 for visual styling
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define WM_TRAYICON (WM_USER + 1)
#define TRAY_ICON_ID 1001

typedef struct {
    DISPLAY_MODE modeBatt;
    DISPLAY_MODE modeAC;
    BOOL runAtStartup;
} WINPOWERDMS_PREFS;

static WINPOWERDMS_PREFS userPrefs = { 0 };

// Save user preferences to registry.
static BOOL SavePrefs(void) {
    BOOL saved = FALSE;

    HKEY regKey;
    if (!RegCreateKeyEx(
        HKEY_CURRENT_USER, L"SOFTWARE\\WinPowerDMS", 0, NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &regKey, NULL
    )) {
        RegSetKeyValue(regKey, L"Battery", L"Width", REG_DWORD, &userPrefs.modeBatt.width, sizeof(userPrefs.modeBatt.width));
        RegSetKeyValue(regKey, L"Battery", L"Height", REG_DWORD, &userPrefs.modeBatt.height, sizeof(userPrefs.modeBatt.height));
        RegSetKeyValue(regKey, L"Battery", L"Refresh Rate", REG_DWORD, &userPrefs.modeBatt.refresh, sizeof(userPrefs.modeBatt.refresh));
        RegSetKeyValue(regKey, L"AC Power", L"Width", REG_DWORD, &userPrefs.modeAC.width, sizeof(userPrefs.modeAC.width));
        RegSetKeyValue(regKey, L"AC Power", L"Height", REG_DWORD, &userPrefs.modeAC.height, sizeof(userPrefs.modeAC.height));
        RegSetKeyValue(regKey, L"AC Power", L"Refresh Rate", REG_DWORD, &userPrefs.modeAC.refresh, sizeof(userPrefs.modeAC.refresh));
        RegCloseKey(regKey);
        saved = TRUE;
    }

    // Create the startup entry
    if (userPrefs.runAtStartup) {
        WCHAR exePath[MAX_PATH];
        DWORD exePathLen = GetModuleFileName(NULL, exePath, sizeof(exePath) / sizeof(exePath[0])) + 1;
        RegSetKeyValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", L"WinPowerDMS", REG_SZ, exePath, exePathLen);
    }
    else { // Delete the startup entry
        RegDeleteKeyValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", L"WinPowerDMS");
    }
    return saved;
}

// Load user preferences from registry.
static BOOL LoadPrefs(void) {
    BOOL loaded = FALSE;

    HKEY regKey;
    if (!RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\WinPowerDMS", 0, KEY_READ, &regKey)) {
        DWORD keySize = sizeof(userPrefs.modeBatt.width);
        RegGetValue(regKey, L"Battery", L"Width", RRF_RT_REG_DWORD, NULL, &userPrefs.modeBatt.width, &keySize);
        keySize = sizeof(userPrefs.modeBatt.height);
        RegGetValue(regKey, L"Battery", L"Height", RRF_RT_REG_DWORD, NULL, &userPrefs.modeBatt.height, &keySize);
        keySize = sizeof(userPrefs.modeBatt.refresh);
        RegGetValue(regKey, L"Battery", L"Refresh Rate", RRF_RT_REG_DWORD, NULL, &userPrefs.modeBatt.refresh, &keySize);
        keySize = sizeof(userPrefs.modeAC.width);
        RegGetValue(regKey, L"AC Power", L"Width", RRF_RT_REG_DWORD, NULL, &userPrefs.modeAC.width, &keySize);
        keySize = sizeof(userPrefs.modeAC.height);
        RegGetValue(regKey, L"AC Power", L"Height", RRF_RT_REG_DWORD, NULL, &userPrefs.modeAC.height, &keySize);
        keySize = sizeof(userPrefs.modeAC.refresh);
        RegGetValue(regKey, L"AC Power", L"Refresh Rate", RRF_RT_REG_DWORD, NULL, &userPrefs.modeAC.refresh, &keySize);
        RegCloseKey(regKey);
        loaded = TRUE;
    }

    // Figure out if application has a startup entry
    if (!RegGetValue(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", L"WinPowerDMS", RRF_RT_ANY, NULL, NULL, NULL))
        userPrefs.runAtStartup = TRUE;
    
    return loaded;
}

static DISPLAY_MODE GetCurrentDisplayMode(void) {
    DEVMODEW currentMode = { .dmSize = sizeof(currentMode) };
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &currentMode);
    return (DISPLAY_MODE) {
        .width = currentMode.dmPelsWidth,
        .height = currentMode.dmPelsHeight,
        .refresh = currentMode.dmDisplayFrequency
    };
}

static INT_PTR CALLBACK PrefsDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    HWND hComboBatt = GetDlgItem(hDlg, IDC_COMBO_BATT);
    HWND hComboAC = GetDlgItem(hDlg, IDC_COMBO_AC);
    HWND hCheckStartup = GetDlgItem(hDlg, IDC_CHECK_STARTUP);

    switch (uMsg) {
    case WM_INITDIALOG: {
        // devMode object that will be enumerated
        DEVMODEW devMode;
        ZeroMemory(&devMode, sizeof(devMode));
        devMode.dmSize = sizeof(devMode);

        size_t modeCount = 0;
        DWORD modeNum = 0;
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
                if (DisplayModeEquals(&userPrefs.modeBatt, &currentMode))
                    SendMessage(hComboBatt, CB_SETCURSEL, modeCount, 0);

                SendMessage(hComboAC, CB_ADDSTRING, 0, (LPARAM)resText);
                if (DisplayModeEquals(&userPrefs.modeAC, &currentMode))
                    SendMessage(hComboAC, CB_SETCURSEL, modeCount, 0);
                
                ++modeCount;
                lastMode = currentMode;
            }
        }

        SendMessage(hCheckStartup, BM_SETCHECK, userPrefs.runAtStartup ? BST_CHECKED : BST_UNCHECKED, 0);
        return TRUE;
    }
                      
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDC_BUTTON_APPLY:
        case IDOK: {
            userPrefs.modeBatt = GetModeFromCB(hComboBatt);
            userPrefs.modeAC = GetModeFromCB(hComboAC);
            userPrefs.runAtStartup = SendMessage(hCheckStartup, BM_GETCHECK, 0, 0) == BST_CHECKED;
            SavePrefs();
            if (LOWORD(wParam) == IDC_BUTTON_APPLY) return TRUE;
        }
        case IDCANCEL: {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        case IDC_BUTTON_TEST_BATT: {
            DISPLAY_MODE mode = GetModeFromCB(hComboBatt);
            TestDisplayMode(hDlg, NULL, &mode);
            return TRUE;
        }
        case IDC_BUTTON_TEST_AC: {
            DISPLAY_MODE mode = GetModeFromCB(hComboAC);
            TestDisplayMode(hDlg, NULL, &mode);
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
static HMENU hMenu;

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd); // Required for menu to disappear correctly

            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_PREFS:
            DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PREFSDIALOG), hWnd, PrefsDialogProc);
            break;
        case ID_TRAY_ABOUT:
            MessageBoxW(hWnd, L"WinPowerDMS\nA utility for switching the resolution of your laptop's display based on the current power state.", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_TRAY_EXIT:
            DestroyWindow(hWnd);
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
            DISPLAY_MODE currentMode = GetCurrentDisplayMode();
            SYSTEM_POWER_STATUS powerStatus;
            // Change display mode only if the current display mode is one of the configured options.
            if (GetSystemPowerStatus(&powerStatus) &&
                (DisplayModeEquals(&currentMode, &userPrefs.modeBatt) || DisplayModeEquals(&currentMode, &userPrefs.modeAC)))
            {
                ChangeDisplayMode(NULL, powerStatus.ACLineStatus ? &userPrefs.modeAC : &userPrefs.modeBatt, CDS_UPDATEREGISTRY);
            }
        }
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    if (!LoadPrefs()) { // set both battery and AC to current display mode if there are no preferences set
        DISPLAY_MODE currentMode = GetCurrentDisplayMode();
        userPrefs.modeAC = currentMode;
        userPrefs.modeBatt = currentMode;
    }

    InitCommonControls(); // Initialize modern controls

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TrayAppClass";

    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(0, L"TrayAppClass", NULL, 0, 0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInstance, NULL);
    RegisterPowerSettingNotification(hWnd, &GUID_ACDC_POWER_SOURCE, DEVICE_NOTIFY_WINDOW_HANDLE);

    // Create context menu
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_PREFS, L"Preferences");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_ABOUT, L"About");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    // Setup tray icon
    HICON hTrayIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hTrayIcon;
    wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(nid.szTip[0]), L"WinPowerDMS");

    Shell_NotifyIcon(NIM_ADD, &nid);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyIcon(hTrayIcon);
    return 0;
}