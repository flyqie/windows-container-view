#pragma once

void TweakWindowStation();
void AssignWindowStation();

// Asks for a display mode. Returns false if the display driver rejected it.
//
// Only modes the container's stub display driver advertises are accepted, at
// 32bpp and 60 Hz; see Docs/IMAGE-ANALYSIS.md for the list.
bool SetDisplayMode(int Width, int Height);

// Starts Command on the container's desktop. Empty means cmd.exe.
void StartProgram(const wchar_t* Command);
