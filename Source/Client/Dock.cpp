#include <Windows.h>

#include <algorithm>
#include <vector>

#include "../Common/SandboxEnv.h"
#include "Dock.h"

namespace
{
	// One button per window, in the order the windows first appeared.
	//
	// Deliberately not z-order: that changes every time the user clicks a
	// window, and buttons that reshuffle under the cursor cannot be clicked.
	// A window is appended when it shows up and dropped when it goes away, which
	// is what a taskbar does.
	std::vector<HWND> gButtons;

	const int ButtonWidth = 150;
	const int ButtonGap = 2;

	// A window the dock should offer a button for.
	bool IsDockWindow(HWND hwnd)
	{
		if (!IsWindowVisible(hwnd) || GetWindowTextLengthW(hwnd) <= 0)
		{
			return false;
		}

		// A taskbar does not list tool windows either. This also keeps the
		// desktop's own helper windows out of the strip.
		if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
		{
			return false;
		}

		return true;
	}

	BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lParam)
	{
		std::vector<HWND>* Found = (std::vector<HWND>*)lParam;

		if (IsDockWindow(hwnd))
		{
			Found->push_back(hwnd);
		}

		return TRUE;
	}

	// Brings the button list up to date: drops windows that are gone, appends
	// the ones that appeared, and leaves everything else where it was.
	void Refresh()
	{
		std::vector<HWND> Found;

		EnumWindows(EnumProc, (LPARAM)&Found);

		std::vector<HWND> Ordered;

		for (size_t i = 0; i < gButtons.size(); ++i)
		{
			if (std::find(Found.begin(), Found.end(), gButtons[i]) != Found.end())
			{
				Ordered.push_back(gButtons[i]);
			}
		}

		for (size_t i = 0; i < Found.size(); ++i)
		{
			if (std::find(Ordered.begin(), Ordered.end(), Found[i]) == Ordered.end())
			{
				Ordered.push_back(Found[i]);
			}
		}

		gButtons.swap(Ordered);
	}

	// Brings a window back and gives it the foreground.
	void ActivateWindow(HWND Target)
	{
		// Only when it is actually minimized: SW_RESTORE on a maximized window
		// would undo the maximization instead of raising it.
		if (IsIconic(Target))
		{
			ShowWindow(Target, SW_RESTORE);
		}

		if (SetForegroundWindow(Target))
		{
			return;
		}

		// Windows only lets the process that owns the foreground hand it to
		// another, and inside the container that is the program being viewed,
		// never this one. Attaching this thread to the foreground thread's input
		// queue makes it count as part of that thread, which is the classic
		// shell workaround for the same restriction.
		const HWND Foreground = GetForegroundWindow();
		const DWORD ForegroundThread = Foreground ? GetWindowThreadProcessId(Foreground, NULL) : 0;
		const DWORD ThisThread = GetCurrentThreadId();

		if (ForegroundThread && ForegroundThread != ThisThread)
		{
			AttachThreadInput(ThisThread, ForegroundThread, TRUE);
			SetForegroundWindow(Target);
			AttachThreadInput(ThisThread, ForegroundThread, FALSE);
		}
	}

	// Where button Index starts, and how far apart the buttons are.
	int ButtonLeft(int Index)
	{
		return ButtonGap + Index * (ButtonWidth + ButtonGap);
	}
}

bool DockEnabled()
{
	return SandboxDock();
}

int DockHeight()
{
	return DockEnabled() ? SANDBOX_DOCK_HEIGHT : 0;
}

int DrawDock(HDC hdc, int Left, int Top, int Width)
{
	if (!DockEnabled())
	{
		return 0;
	}

	Refresh();

	// Created once. Three GDI objects per frame, thirty times a second, is
	// churn for no reason.
	static HBRUSH Background = CreateSolidBrush(RGB(32, 32, 32));
	static HBRUSH Edge = CreateSolidBrush(RGB(70, 70, 70));
	static HBRUSH Button = CreateSolidBrush(RGB(58, 58, 58));
	static HBRUSH Active = CreateSolidBrush(RGB(0, 90, 158));
	static HBRUSH Minimized = CreateSolidBrush(RGB(44, 44, 44));
	static HFONT Font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

	POINT OldOrigin;
	SetViewportOrgEx(hdc, Left, Top, &OldOrigin);

	RECT Strip{ 0, 0, Width, SANDBOX_DOCK_HEIGHT };
	FillRect(hdc, &Strip, Background);

	// A line along the top, so the dock reads as chrome rather than as part of
	// whatever window is behind it.
	RECT Line{ 0, 0, Width, 1 };
	FillRect(hdc, &Line, Edge);

	HGDIOBJ OldFont = SelectObject(hdc, Font);
	SetBkMode(hdc, TRANSPARENT);

	const HWND Foreground = GetForegroundWindow();
	int Drawn = 0;

	for (size_t i = 0; i < gButtons.size(); ++i)
	{
		const int x = ButtonLeft((int)i);

		if (x + ButtonWidth > Width)
		{
			break;
		}

		const HWND hwnd = gButtons[i];
		const bool bMinimized = IsIconic(hwnd) != FALSE;

		// A minimized window can still be the foreground window: minimizing it
		// does not necessarily hand the foreground to anything else, and with
		// nothing else to hand it to, the system leaves it where it was. Testing
		// the foreground alone would light up the button of a window that is not
		// on screen at all.
		const bool bActive = !bMinimized && hwnd == Foreground;

		RECT Cell{ x, 2, x + ButtonWidth, SANDBOX_DOCK_HEIGHT - 2 };
		FillRect(hdc, &Cell, bActive ? Active : (bMinimized ? Minimized : Button));

		WCHAR Title[128] = { 0 };
		GetWindowTextW(hwnd, Title, 128);

		SetTextColor(hdc, bActive ? RGB(255, 255, 255)
			: (bMinimized ? RGB(150, 150, 150) : RGB(225, 225, 225)));

		RECT Text = Cell;
		Text.left += 6;
		Text.right -= 6;

		// DT_NOPREFIX: an & in a window title is a literal character here, not
		// an accelerator to underline.
		DrawTextW(hdc, Title, -1, &Text,
			DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);

		++Drawn;
	}

	SelectObject(hdc, OldFont);
	SetViewportOrgEx(hdc, OldOrigin.x, OldOrigin.y, NULL);

	return Drawn;
}

bool DockClick(int X, int Y, int Width)
{
	Refresh();

	const int Pitch = ButtonWidth + ButtonGap;

	if (X < ButtonGap || X >= Width)
	{
		return false;
	}

	// A click in the gap between two buttons belongs to neither of them.
	if ((X - ButtonGap) % Pitch >= ButtonWidth)
	{
		return false;
	}

	const int Index = (X - ButtonGap) / Pitch;

	// Buttons that did not fit were never drawn, so there is nothing there to
	// have clicked on.
	if (Index < 0 || (size_t)Index >= gButtons.size())
	{
		return false;
	}

	const HWND Target = gButtons[Index];

	if (!IsWindow(Target))
	{
		return false;
	}

	if (IsIconic(Target))
	{
		ActivateWindow(Target);
	}
	else if (Target == GetForegroundWindow())
	{
		// The toggle a taskbar button has. It is also the only way to minimize
		// a window whose title bar is off-screen.
		ShowWindow(Target, SW_MINIMIZE);
	}
	else
	{
		ActivateWindow(Target);
	}

	return true;
}
