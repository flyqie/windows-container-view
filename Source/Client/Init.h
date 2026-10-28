#pragma once

void TweakWindowStation();
void AssignWindowStation();

// Asks for a display mode. Returns false if the display driver rejected it.
//
// Only modes the container's stub display driver advertises are accepted, at
// 32bpp and 60 Hz; see Docs/IMAGE-ANALYSIS.md for the list.
bool SetDisplayMode(int Width, int Height);

// Narrows the desktop's work area to leave the bottom Rows to something else,
// the way a taskbar reserves the space it occupies: maximized windows, and
// anything else that asks how much room it has, then stop above that line.
//
// Returns false when the system refused, in which case the work area is
// unchanged and windows will extend under whatever is down there. The screen
// itself is not affected -- this changes what programs are told they can use,
// not what the display is.
bool ReserveScreenBottom(int Rows);

// Starts Command on the container's desktop. Empty means cmd.exe.
void StartProgram(const wchar_t* Command);
