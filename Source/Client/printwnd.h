#pragma once

#include <gdiplus.h>
#include <vector>

Gdiplus::Bitmap* PrintScreen(HDESK hDesktop);
void RegisterDesktopWindow();
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

// The container's cursor.
struct CursorInfo
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

// Reads the container's cursor without drawing it; the host composites it. The
// bitmap is only read when it differs from the previous call.
//
// Written through an out parameter rather than returned by value: CursorInfo
// holds a vector, and a returned temporary would need object unwinding, which
// MSVC rejects in the caller alongside its RpcTryExcept block.
void ReadCursor(CursorInfo& Out);

// Number of PrintWindow() calls the last PrintScreen() pass made. Each one is a
// cross-process synchronous WM_PRINT, so this count -- not the pixel count -- is
// what the capture pass costs.
unsigned GetLastPrintWindowCallCount();

// The last captured frame's pixels, and the distance between rows.
//
// The frame is a DIB section, so its bits can be read directly rather than
// through GDI+ LockBits, which measured about 1.2 ms for a full screen.
//
// Valid until the next PrintScreen call. Row padding is not included in the row
// length; use the stride to walk.
const unsigned char* GetLastFrameBits();
int GetLastFrameStride();

// Where the last PrintScreen() pass spent its time, in microseconds. The parts
// scale with different things: WindowPass with the number of controls, the rest
// with screen area.
struct CaptureBreakdown
{
	ULONGLONG BitmapAlloc;    // CreateCompatibleBitmap for the full-screen target
	ULONGLONG Fills;          // clearing the target to the desktop colour
	ULONGLONG WindowPass;     // EnumDesktopWindows + every PrintWindow and BitBlt
	ULONGLONG TopLevelPrint;  //   of which: PrintWindow on each top-level window
	ULONGLONG ChildReprint;   //   of which: the extra pass over descendants
	unsigned  TopLevelCalls;  //   of which: how many of each
	unsigned  ChildCalls;
	ULONGLONG ToGdiPlus;      // Bitmap::FromHBITMAP
	ULONGLONG Total;          // whole call, for cross-checking the parts
};

const CaptureBreakdown& GetLastCaptureBreakdown();
