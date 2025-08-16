#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "DisplayMode.h"

BOOL DisplayModeEquals(const DISPLAY_MODE* a, const DISPLAY_MODE* b) {
    return a->width == b->width && a->height == b->height && a->refresh == b->refresh;
}

// I hate parsing the string here, but the alternative requires extra memory management.
DISPLAY_MODE GetModeFromCB(HWND hComboBox) {
    DISPLAY_MODE mode = { 0 };
    LRESULT selectedIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
    if (selectedIndex != CB_ERR) {
        LRESULT len = SendMessage(hComboBox, CB_GETLBTEXTLEN, selectedIndex, 0);
        if (len != CB_ERR) {
            LPWSTR text = HeapAlloc(GetProcessHeap(), 0, len * sizeof(*text));
            if (text) {
                SendMessage(hComboBox, CB_GETLBTEXT, selectedIndex, (LPARAM)text);
                swscanf_s(text, L"%dx%d @ %d Hz", &mode.width, &mode.height, &mode.refresh);
                HeapFree(GetProcessHeap(), 0, text);
            }
        }
    }

    return mode;
}

// returns the result of the ChangeDisplaySettings call that this results in.
LONG ChangeDisplayMode(LPCWSTR displayName, const DISPLAY_MODE* mode, DWORD dwFlags) {
    DEVMODE devMode = {
        .dmSize = sizeof(devMode),
        .dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY,
        .dmPelsWidth = mode->width,
        .dmPelsHeight = mode->height,
        .dmDisplayFrequency = mode->refresh
    };
    if (displayName) wcscpy_s(devMode.dmDeviceName, sizeof(devMode.dmDeviceName) / sizeof(devMode.dmDeviceName[0]), displayName);

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
    WCHAR displayName[32];
    DISPLAY_MODE mode;
};

static DWORD WINAPI TestDisplayModeThread(LPVOID lpParam) {
    struct TestDisplayModeParams* params = lpParam;
    DEVMODE originalMode = { .dmSize = sizeof(originalMode) };
    EnumDisplaySettings(params->displayName, ENUM_CURRENT_SETTINGS, &originalMode);
    ChangeDisplayMode(params->displayName, &params->mode, CDS_FULLSCREEN);

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

void TestDisplayMode(HWND hDlg, LPCWSTR displayName, DISPLAY_MODE* mode) {
    struct TestDisplayModeParams* tdmParams = HeapAlloc(GetProcessHeap(), 0, sizeof(*tdmParams));
    if (tdmParams) {
        tdmParams->hDlg = hDlg;
        if (tdmParams->displayName) wcscpy_s(tdmParams->displayName, sizeof(tdmParams->displayName) / sizeof(tdmParams->displayName[0]), displayName);
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