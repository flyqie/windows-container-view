// OCR_NORMAL, for the cursor fallback below. It has to be defined before
// Windows.h is first included, and Desktop.h includes it.
#define OEMRESOURCE

#include "Desktop.h"

#include <Windows.h>
#include <AclAPI.h>
#include <shellapi.h>

#include <iostream>
#include <string>
#include <vector>

#include "../Common/SandboxEnv.h"
#include "../Common/SandboxLog.h"

using namespace std;

namespace
{
	// --- the window station -------------------------------------------------
	//
	// A container's processes start on the service window station
	// (Service-0x0-xxx$), where the access rights are too low to create windows
	// or switch desktop at all, and where PrintWindow produces black. The desktop
	// has to be moved to WinSta0\Default first, and that needs the window station
	// and the desktop opened up to everyone.

	bool AdjustSingleTokenPrivilege(HANDLE TokenHandle, LPCTSTR lpName, DWORD dwAttributes)
	{
		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = dwAttributes;

		if (!LookupPrivilegeValue(NULL, lpName, &(tp.Privileges[0].Luid)))
		{
			return false;
		}

		if (!AdjustTokenPrivileges(TokenHandle, FALSE, &tp, 0, NULL, NULL))
		{
			return false;
		}

		return true;
	}

	void TweakWindowStation()
	{
		HWINSTA hWinSta = OpenWindowStation(TEXT("WinSta0"), FALSE, GENERIC_EXECUTE | WRITE_DAC);

		if (!hWinSta)
		{
			Debug() << "Opening WinSta0 failed (err 0x" << hex << GetLastError() << ")" << endl;
			return;
		}

		HWINSTA hOldWinsta = GetProcessWindowStation();
		SetProcessWindowStation(hWinSta);

		HDESK hDesktop = OpenDesktop(TEXT("Default"), 0, FALSE, GENERIC_READ | WRITE_DAC);

		if (!hDesktop)
		{
			Debug() << "Opening Default desktop failed (err 0x" << hex << GetLastError() << ")" << endl;
			return;
		}

		SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;

		PSID pEveryoneSID = NULL;
		AllocateAndInitializeSid(&SIDAuthWorld, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pEveryoneSID);

		EXPLICIT_ACCESS ExplicitAccess;
		memset(&ExplicitAccess, 0, sizeof(ExplicitAccess));
		ExplicitAccess.grfAccessPermissions = GENERIC_ALL;
		ExplicitAccess.grfAccessMode = GRANT_ACCESS;
		ExplicitAccess.grfInheritance = NO_INHERITANCE;
		ExplicitAccess.Trustee.pMultipleTrustee = NULL;
		ExplicitAccess.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
		ExplicitAccess.Trustee.ptstrName = (TCHAR*)pEveryoneSID;

		PACL Dacl;
		DWORD Status = SetEntriesInAcl(1, &ExplicitAccess, NULL, &Dacl);

		if (Status == ERROR_SUCCESS)
		{
			HANDLE hToken;
			OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);

			if (AdjustSingleTokenPrivilege(hToken, SE_SECURITY_NAME, SE_PRIVILEGE_ENABLED))
			{
				Status = SetSecurityInfo(hWinSta, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
					NULL, NULL, Dacl, NULL);

				if (Status)
				{
					Debug() << "SetSecurityInfo returned " << hex << Status << endl;
				}

				Status = SetSecurityInfo(hDesktop, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
					NULL, NULL, Dacl, NULL);

				if (Status)
				{
					Debug() << "SetSecurityInfo returned " << hex << Status << endl;
				}
			}
			else
			{
				Debug() << "AdjustTokenPrivileges failed (err 0x" << hex << GetLastError() << ")" << endl;
			}
		}
		else
		{
			Debug() << "SetEntriesInAcl returned " << hex << Status << endl;
		}

		LocalFree(Dacl);

		SetProcessWindowStation(hOldWinsta);
		CloseHandle(hWinSta);
		CloseHandle(hDesktop);
	}

	// Moves the process onto WinSta0\Default. False if the desktop could not be
	// reached, in which case there is nothing to show and nothing to send input
	// to, and the caller should give up rather than carry on blind.
	bool AssignWindowStation()
	{
		HWINSTA hWinSta = CreateWindowStation(TEXT("WinSta0"), 0, GENERIC_ALL, NULL);

		if (!hWinSta)
		{
			Debug() << "Opening WinSta0 failed (err 0x" << hex << GetLastError() << ")" << endl;
			return false;
		}

		HWINSTA hOldWinsta = GetProcessWindowStation();

		if (!SetProcessWindowStation(hWinSta))
		{
			Debug() << "SetProcessWindowStation failed (err 0x" << hex << GetLastError() << ")" << endl;
			return false;
		}

		HDESK hDesktop = CreateDesktop(TEXT("Default"), NULL, NULL, 0, GENERIC_ALL, NULL);

		if (!hDesktop)
		{
			Debug() << "Opening hDesktop failed (err 0x" << hex << GetLastError() << ")" << endl;
			return false;
		}

		HDESK hOldDesktop = GetThreadDesktop(GetCurrentThreadId());

		if (!SetThreadDesktop(hDesktop))
		{
			Debug() << "SetThreadDesktop failed (err 0x" << hex << GetLastError() << ")" << endl;
			return false;
		}

		if (hOldWinsta)
		{
			CloseHandle(hOldWinsta);
		}

		if (hOldDesktop)
		{
			CloseHandle(hOldDesktop);
		}

		USEROBJECTFLAGS Flags{ 0 };
		Flags.dwFlags = WSF_VISIBLE;

		if (!SetUserObjectInformation(hWinSta, UOI_FLAGS, &Flags, sizeof(Flags)))
		{
			Debug() << "SetUserObjectInformation failed (err 0x" << hex << GetLastError() << ")" << endl;
		}

		if (!SwitchDesktop(hDesktop))
		{
			Debug() << "SwitchDesktop failed (err 0x" << hex << GetLastError() << ")" << endl;
		}

		return true;
	}

	// The container has no shell to paint a background, so the client picks one.
	void SetDesktopColour()
	{
		INT Elements[] = { COLOR_DESKTOP };
		COLORREF RgbValues[] = { RGB(8, 110, 165) };

		SetSysColors(1, Elements, RgbValues);
	}

	// --- the display mode ---------------------------------------------------

	bool SetDisplayMode(int Width, int Height)
	{
		DEVMODEW Mode;
		memset(&Mode, 0, sizeof(Mode));

		Mode.dmSize = sizeof(Mode);

		// All four fields are required, not just the size.
		//
		// The stub display driver validates a requested mode by looking for it in
		// its mode table with the bit depth and refresh rate attached, and falls
		// back to the default when any of them is left unset. See
		// Docs/IMAGE-ANALYSIS.md.
		Mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
		Mode.dmPelsWidth = (DWORD)Width;
		Mode.dmPelsHeight = (DWORD)Height;
		Mode.dmBitsPerPel = 32;
		Mode.dmDisplayFrequency = 60;

		const LONG Result = ChangeDisplaySettingsW(&Mode, 0);

		if (Result != DISP_CHANGE_SUCCESSFUL)
		{
			Debug() << "ChangeDisplaySettings to " << Width << "x" << Height
			        << " rejected (code " << Result << ")" << endl;

			return false;
		}

		Debug() << "Display mode set to " << Width << "x" << Height << endl;

		return true;
	}

	// --- input --------------------------------------------------------------

	void Send(INPUT& Input, const char* What)
	{
		if (!SendInput(1, &Input, sizeof(INPUT)))
		{
			Debug() << "SendInput for " << What << " returned false with error "
			        << hex << GetLastError() << endl;
		}
	}

	// A window the desktop should be counted by.
	BOOL CALLBACK CountProc(HWND hwnd, LPARAM lParam)
	{
		DWORD* Count = (DWORD*)lParam;

		if (IsWindowVisible(hwnd))
		{
			*Count = *Count + 1;
		}

		return TRUE;
	}
}

bool Desktop::Open()
{
	Debug() << "Configuring window stations..." << endl;
	TweakWindowStation();

	Debug() << "Assigning current process to the default window station..." << endl;

	if (!AssignWindowStation())
	{
		return false;
	}

	SetDesktopColour();

	// Before the program starts, so it opens at the right size.
	//
	// The frame path adapts on its own: the capture reads the screen size every
	// frame and rebuilds its buffers when it changes, and the host rebuilds its
	// texture when the frame size changes.
	int ModeWidth = 0;
	int ModeHeight = 0;

	if (SandboxResolution(ModeWidth, ModeHeight))
	{
		SetDisplayMode(ModeWidth, ModeHeight);
	}

	return true;
}

void Desktop::StartProgram(const wchar_t* Command)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));

	si.cb = sizeof(si);

	HANDLE hDesktop = GetThreadDesktop(GetCurrentThreadId());
	HANDLE hWinSta = GetProcessWindowStation();

	DWORD TotalStringSize = 0;

	DWORD nWinStaNameSize = 0;
	GetUserObjectInformation(hWinSta, UOI_NAME, NULL, 0, &nWinStaNameSize);
	TotalStringSize += nWinStaNameSize;

	DWORD nDesktopNameSize = 0;
	GetUserObjectInformation(hDesktop, UOI_NAME, NULL, 0, &nDesktopNameSize);
	TotalStringSize += nDesktopNameSize;

	TCHAR* lpDesktop = new TCHAR[TotalStringSize / sizeof(TCHAR)];

	const DWORD nWinStaNameLength = nWinStaNameSize / sizeof(TCHAR);
	GetUserObjectInformation(hWinSta, UOI_NAME, lpDesktop, nWinStaNameSize, &nWinStaNameSize);
	lpDesktop[nWinStaNameLength - 1] = '\\';

	GetUserObjectInformation(hDesktop, UOI_NAME, lpDesktop + nWinStaNameLength, nDesktopNameSize, &nDesktopNameSize);

	DebugW() << L"Desktop: " << lpDesktop << endl;

	si.lpDesktop = lpDesktop;

	// cmd.exe unless the caller named something else.
	const wchar_t* ToRun = (Command && *Command) ? Command : L"cmd.exe";

	// CreateProcess may modify the command line, so it needs a writable copy.
	std::vector<wchar_t> CommandLine(ToRun, ToRun + wcslen(ToRun) + 1);

	BOOL Result = CreateProcess(
		NULL,
		CommandLine.data(),
		NULL,
		NULL,
		FALSE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi
	);

	delete[] lpDesktop;

	if (!Result)
	{
		Debug() << "Process creation failed: error " << GetLastError() << endl;
		return;
	}

	Debug() << "Waiting for input..." << endl;
	WaitForInputIdle(pi.hProcess, INFINITE);

	// Wait until a window is drawn.
	//
	// A top-level window is not enough: a window is created before it is shown,
	// and GetTopWindow is satisfied by one that is still hidden. The session
	// below counts visible windows, so it would see an empty desktop and end on
	// its first frame, before the program had drawn anything at all.
	DWORD dwCount = 0;
	while (!HasVisibleWindows() && dwCount < 10000)
	{
		dwCount += 100;
		Sleep(100);
	}

	Debug() << "Done" << endl;
}

bool Desktop::HasVisibleWindows()
{
	DWORD Count = 0;

	EnumWindows(CountProc, (LPARAM)&Count);

	return Count != 0;
}

void Desktop::ReadCursor(Cursor& Out)
{
	Out = Cursor();

	CURSORINFO cursor = { sizeof(cursor) };
	if (!GetCursorInfo(&cursor))
	{
		return;
	}

	Out.X = cursor.ptScreenPos.x;
	Out.Y = cursor.ptScreenPos.y;

	// CURSOR_SUPPRESSED is the only state that means "do not draw this".
	//
	// Testing for CURSOR_SHOWING instead -- the obvious reading of the flag --
	// reports the cursor as hidden on this desktop. The container's window
	// station is created by the client rather than by winlogon, and it reports
	// flags 0 while still having a cursor worth drawing.
	Out.Visible = cursor.flags != CURSOR_SUPPRESSED;

	// The bitmap only changes a few times a minute, so it is only read when the
	// handle changes. A shared system cursor keeps one handle for its lifetime,
	// and these are the system's own cursors.
	static HCURSOR LastCursor = NULL;

	if (cursor.hCursor == LastCursor)
	{
		return;
	}

	HCURSOR hCursor = cursor.hCursor;

	if (!hCursor)
	{
		// No cursor handle yet; fall back to the system arrow so the host has
		// something to draw.
		hCursor = (HCURSOR)LoadImage(NULL, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0,
			LR_DEFAULTSIZE | LR_SHARED);
	}

	if (!hCursor)
	{
		return;
	}

	ICONINFO info = { sizeof(info) };
	if (!GetIconInfo(hCursor, &info))
	{
		return;
	}

	// hbmColor is null for the legacy mask-only cursors. Those are rare enough
	// that the host just keeps the cursor it already has rather than growing a
	// second render path here.
	if (!info.hbmColor)
	{
		DeleteObject(info.hbmMask);
		return;
	}

	BITMAP bm = { 0 };
	GetObject(info.hbmColor, sizeof(bm), &bm);

	const unsigned long W = (unsigned long)bm.bmWidth;
	const unsigned long H = (unsigned long)bm.bmHeight;

	// Ask GetDIBits for BGRA rather than drawing through DrawIconEx: GDI does
	// not maintain the alpha channel, and a 32bpp cursor's alpha is what makes
	// it composite correctly over the frame.
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (LONG)W;
	bmi.bmiHeader.biHeight = -(LONG)H;   // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	const unsigned long HotX = (unsigned long)info.xHotspot;
	const unsigned long HotY = (unsigned long)info.yHotspot;

	std::vector<unsigned char> Pixels((size_t)W * H * 4);

	HDC hdc = CreateCompatibleDC(NULL);
	const int Got = GetDIBits(hdc, info.hbmColor, 0, H, Pixels.data(), &bmi, DIB_RGB_COLORS);
	DeleteDC(hdc);

	DeleteObject(info.hbmColor);
	DeleteObject(info.hbmMask);

	if (!Got)
	{
		return;
	}

	LastCursor = cursor.hCursor;

	Out.ShapeChanged = true;
	Out.Width = W;
	Out.Height = H;
	Out.HotX = HotX;
	Out.HotY = HotY;
	Out.Shape.swap(Pixels);
}

void Desktop::PointerMove(int X, int Y)
{
	// This is what positions the cursor. The dx/dy SendInput takes are ignored,
	// because MOUSEEVENTF_ABSOLUTE is not set.
	SetCursorPos(X, Y);
}

void Desktop::PointerButton(int Button, bool bDown)
{
	INPUT input = { 0 };
	input.type = INPUT_MOUSE;

	switch (Button)
	{
	case 1:
		input.mi.dwFlags = bDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
		break;
	case 2:
		input.mi.dwFlags = bDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
		break;
	case 3:
		input.mi.dwFlags = bDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
		break;
	default:
		return;
	}

	Send(input, "mouse");
}

void Desktop::Wheel(int X, int Y, int Delta, int HDelta)
{
	// The system delivers the wheel to the window with keyboard focus, but the
	// message carries the cursor position, so the cursor has to be where the user
	// was pointing.
	SetCursorPos(X, Y);

	INPUT input = { 0 };
	input.type = INPUT_MOUSE;

	// One notch is WHEEL_DELTA, and the sign carries over from the host: positive
	// is away from the user vertically, to the right horizontally. mouseData is a
	// DWORD holding a signed delta, so the cast is intended.
	//
	// Two calls, because one INPUT carries one axis.
	if (Delta)
	{
		input.mi.dwFlags = MOUSEEVENTF_WHEEL;
		input.mi.mouseData = (DWORD)Delta;

		Send(input, "wheel");
	}

	if (HDelta)
	{
		input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
		input.mi.mouseData = (DWORD)HDelta;

		Send(input, "horizontal wheel");
	}
}

void Desktop::Key(int ScanCode, bool bDown)
{
	WORD scan = (WORD)ScanCode;

	INPUT input = { 0 };
	input.type = INPUT_KEYBOARD;
	input.ki.dwFlags = bDown ? 0 : KEYEVENTF_KEYUP;
	input.ki.dwFlags |= KEYEVENTF_SCANCODE;

	// The host's SDL-to-DIK table stores extended keys with bit 0x80 set (arrows,
	// Home/End, PgUp/PgDn, Insert/Delete, numpad Enter and Divide, right
	// Ctrl/Alt, Win/Menu). Strip the marker and flag the key as extended,
	// otherwise the system does not recognise the scan code and drops the key.
	if (scan & 0x80)
	{
		scan &= 0x7F;
		input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
	}

	input.ki.wScan = scan;
	input.ki.wVk = (WORD)MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);

	Send(input, "key");
}
