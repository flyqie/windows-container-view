#pragma once

#include <Windows.h>
#include <gdiplus.h>

#include <vector>

// Part one of the client: the container's desktop.
//
// The window station, the screen, the cursor, and the input that goes into it --
// the container's own graphical environment, the equivalent of an X server and a
// window manager. All of it would still make sense with nothing attached to it,
// which is the point of the split: the remote display that carries it elsewhere
// is Remote.h, and the strip of window buttons along its bottom is Dock.h.
//
// Nothing here knows about RPC, PNG or the host.
namespace Desktop
{
	// Brings up the desktop: opens WinSta0\Default to everyone, attaches this
	// process to it, and applies SANDBOX_RESOLUTION if it is set.
	//
	// Returns false if the desktop could not be reached, in which case nothing
	// else here is usable.
	bool Open();

	// Starts Command on the desktop; empty means cmd.exe.
	//
	// Separate from Open so that whatever has to be in place before the program
	// runs -- the dock's work area, in particular -- can be.
	void StartProgram(const wchar_t* Command);

	// True while the desktop still has a window worth showing. The session has
	// nothing left to do once it does not.
	bool HasVisibleWindows();

	// --- frames -------------------------------------------------------------

	// One screen, captured and open for drawing until it is finished.
	//
	// Capture paints the desktop into it and Finish closes it out; anything that
	// belongs on the picture goes in between, which is where Remote hands it to
	// Dock::Draw. A frame is only valid between the two calls, and only until the
	// next Capture.
	struct Frame
	{
		HDC Dc = NULL;              // draw here, in screen coordinates
		HGDIOBJ Previous = NULL;    // what Dc had selected before the frame went in
		const unsigned char* Bits = nullptr;
		int Stride = 0;
		int Width = 0;
		int Height = 0;
	};

	bool Capture(Frame& Out);

	// The frame as an image, ready to encode. Null if it could not be wrapped.
	//
	// The image does not own the pixels, so it must not outlive the next Capture.
	Gdiplus::Bitmap* Finish(const Frame& In);

	// --- the cursor ---------------------------------------------------------

	// The container's cursor. Read, never drawn: it travels to the host on its
	// own so that moving it does not dirty the pixels underneath it.
	struct Cursor
	{
		long X = 0;                    // screen position of the cursor, not the hotspot
		long Y = 0;
		bool Visible = false;
		bool ShapeChanged = false;     // the bitmap differs from the previous call

		// Only meaningful when ShapeChanged.
		unsigned long Width = 0;
		unsigned long Height = 0;
		unsigned long HotX = 0;
		unsigned long HotY = 0;
		std::vector<unsigned char> Shape;   // 32bpp BGRA, top-down, straight alpha
	};

	// Written through an out parameter rather than returned by value: Cursor
	// holds a vector, and a returned temporary would need object unwinding, which
	// MSVC rejects in callers that also use RpcTryExcept.
	void ReadCursor(Cursor& Out);

	// --- input --------------------------------------------------------------

	// Injected into the desktop. Each one prints why it failed, and only under
	// SANDBOX_DEBUG; none of them reports anything else.
	void PointerMove(int X, int Y);
	void PointerButton(int Button, bool bDown);       // 1 left, 2 middle, 3 right
	void Wheel(int X, int Y, int Delta, int HDelta);  // notches, positive away and right
	void Key(int ScanCode, bool bDown);

	// --- profiling ----------------------------------------------------------

	// Where the last capture spent its time, in microseconds, and how many
	// PrintWindow calls it made.
	//
	// The parts are kept apart because they scale with different things:
	// WindowPass with the number of windows and controls, the rest with screen
	// area. See Docs/PERFORMANCE.md.
	struct CaptureBreakdown
	{
		ULONGLONG BitmapAlloc;    // CreateCompatibleBitmap for the full-screen target
		ULONGLONG Fills;          // clearing the target to the desktop colour
		ULONGLONG WindowPass;     // EnumDesktopWindows + every PrintWindow and BitBlt
		ULONGLONG TopLevelPrint;  //   of which: PrintWindow on each top-level window
		ULONGLONG ChildReprint;   //   of which: the extra pass over descendants
		unsigned  TopLevelCalls;  //   of which: how many of each
		unsigned  ChildCalls;
		ULONGLONG ToGdiPlus;      // the alpha pass and the wrap into a GDI+ bitmap
		ULONGLONG Total;          // the whole capture, for cross-checking the parts
	};

	const CaptureBreakdown& LastCaptureBreakdown();
	unsigned LastPrintWindowCalls();
}
