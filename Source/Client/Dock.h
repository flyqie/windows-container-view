#pragma once

#include <Windows.h>

// Part two of the client: the dock.
//
// A container has no shell: no taskbar, no Alt+Tab, nothing that knows a
// minimized window still exists. The capture skips minimized windows because
// they have no pixels to paint, so without this a minimized window is gone for
// good. The dock is the strip of window buttons that brings them back.
//
// It behaves the way a real taskbar does, in both directions:
//
//   * It is drawn over the bottom of the desktop, so the frame is exactly the
//     screen and the host's window is the same size whether the dock is on or
//     not. Dragging a window down slides it behind the dock, not under it.
//   * It reserves the same rows out of the desktop's work area, so maximized
//     windows and anything else that asks stop above it. The container sees a
//     smaller usable area, not a smaller screen.
//
// **It is optional, and the option is contained here.** With SANDBOX_DOCK unset
// every function below does nothing and Contains is always false, so no caller
// needs a branch of its own.
//
// It is drawn by the client rather than the host because it can then use GDI's
// text; SDL2 without SDL_ttf has no fonts at all.
#define SANDBOX_DOCK_HEIGHT 26

namespace Dock
{
	// Brings the dock up, if SANDBOX_DOCK asked for it.
	//
	// Call it after the display mode is settled and before the program starts: it
	// reserves the bottom of the work area, and the program should be born
	// knowing how much room it has.
	void Open();

	// Draws the dock along the bottom of a frame. Nothing at all when it is off.
	void Draw(HDC Dc, int Width, int Height);

	// True when Y, in frame coordinates, is over the dock. Always false when it
	// is off, which is what lets the caller treat every click the same way.
	bool Contains(int Y, int FrameHeight);

	// Handles a button press at (X, Y) in frame coordinates.
	void Press(int X, int Y, int Width, int FrameHeight);
}
