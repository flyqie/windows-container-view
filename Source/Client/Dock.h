#pragma once

#include <Windows.h>

// The strip of window buttons the client draws along the bottom of the desktop.
//
// A container has no shell: no taskbar, no Alt+Tab, nothing that knows a
// minimized window still exists. The capture pass skips minimized windows
// because there are no pixels to paint, so without this a minimized window is
// gone for good.
//
// It behaves the way a real taskbar does, in both directions:
//
//   * It is drawn over the bottom of the desktop, so the frame is exactly the
//     screen and the host's window is the same size whether the dock is on or
//     not. Dragging a window down slides it behind the dock, not under it.
//   * The client reserves the same rows out of the desktop's work area, so
//     maximized windows and anything else that asks stop above it. The container
//     sees a smaller usable area, not a smaller screen.
//
// The host is not told any of this. The frame is the screen, the desktop still
// starts at its top-left corner, and the work area is the container's own
// business -- so the host's scaling, drawing and pointer mapping all keep
// working unchanged. What needs care is the cursor, which is the client's to
// place; see the dock branch in ApplyInputEvent.
//
// Drawn by the client rather than the host because it can then use GDI's text.
// SDL2 without SDL_ttf has no fonts at all.
#define SANDBOX_DOCK_HEIGHT 26

// Whether the dock is on, from SANDBOX_DOCK. Off unless it is set.
bool DockEnabled();

// The strip's height: SANDBOX_DOCK_HEIGHT when it is on, 0 when it is off.
int DockHeight();

// Draws the dock with its top-left corner at (Left, Top) in hdc, and returns
// how many window buttons were drawn. Buttons that do not fit are left out.
int DrawDock(HDC hdc, int Left, int Top, int Width);

// Handles a click at (X, Y), in coordinates relative to the dock's top-left
// corner. Returns true when a window button was hit, whether or not the click
// changed anything.
bool DockClick(int X, int Y, int Width);
