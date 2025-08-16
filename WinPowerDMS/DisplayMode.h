#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	DWORD width;
	DWORD height;
	DWORD refresh;
} DISPLAY_MODE;

BOOL DisplayModeEquals(const DISPLAY_MODE* a, const DISPLAY_MODE* b);

DISPLAY_MODE GetModeFromCB(HWND hComboBox);

// returns the result of the ChangeDisplaySettings call that this results in.
LONG ChangeDisplayMode(const DISPLAY_MODE* mode, DWORD dwFlags);

void TestDisplayMode(HWND hDlg, DISPLAY_MODE* mode);