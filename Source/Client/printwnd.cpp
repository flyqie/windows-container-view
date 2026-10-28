#define OEMRESOURCE
#include <Windows.h>

#include <gdiplus.h>
#include <ole2.h>
#include <olectl.h>
#include <vector>

#include <iostream>

#include "../Common/SandboxLog.h"
#include "Dock.h"
#include "printwnd.h"

using namespace std;

HWND hDesktopWnd = NULL;
HDC hDesktopDC = NULL;

using namespace Gdiplus;

BOOL CALLBACK EnumWindowsReverseProc(
	_In_ HWND   hwnd,
	_In_ LPARAM lParam
)
{
	std::vector<HWND>* hwndList = (std::vector<HWND>*)lParam;
	hwndList->push_back(hwnd);

	return TRUE;
}

#if 0
void DrawWindowInternal(HWND hwnd, HDC hdc)
{
	WINDOWINFO WindowInfo;
	GetWindowInfo(hwnd, &WindowInfo);

	HWND hwndParent = GetAncestor(hwnd, GA_ROOT);

	WINDOWINFO OwnerWindowInfo;
	GetWindowInfo(hwndParent, &OwnerWindowInfo);

	if (WindowInfo.dwStyle & WS_VISIBLE)
	{
		BOOL b = FALSE;
		RECT rect;

		rect.left = WindowInfo.rcWindow.left - OwnerWindowInfo.rcWindow.left;
		rect.top = WindowInfo.rcWindow.top - OwnerWindowInfo.rcWindow.top;
		rect.right = WindowInfo.rcWindow.right - OwnerWindowInfo.rcWindow.left;
		rect.bottom = WindowInfo.rcWindow.bottom - OwnerWindowInfo.rcWindow.top;

		//FillRect( hdc, &rect, ( HBRUSH )( COLOR_WINDOW + 1 ) );
		//DrawFrameControl( hdc, &rect, DFC_BUTTON, DFCS_BUTTON3STATE );

		if (WindowInfo.dwStyle & WS_CAPTION)
		{
			RECT frame;

			frame.top = rect.top;
			frame.right = rect.right;
			frame.bottom = frame.top + 16;
			frame.left = frame.right - 32;
			DrawFrameControl(hdc, &frame, DFC_CAPTION, DFCS_CAPTIONCLOSE);

			frame.left -= 32;
			frame.right -= 32;
			DrawFrameControl(hdc, &frame, DFC_CAPTION, DFCS_CAPTIONMAX);

			frame.left -= 32;
			frame.right -= 32;
			DrawFrameControl(hdc, &frame, DFC_CAPTION, DFCS_CAPTIONMIN);

			frame.top = rect.top;
			frame.left = rect.left;
			frame.right = rect.right;
			frame.bottom = 50;
			//b = DrawCaption( hwnd, hdc, &frame, DC_BUTTONS | DC_ICON | DC_TEXT );

			//UserDrawCaption( hwnd, hdc, &frame, NULL, NULL, DC_BUTTONS | DC_ICON | DC_TEXT );
			UserDrawCaptionBar(hwnd, hdc, 0);
		}
		else
		{
			Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
		}

		TCHAR WindowText[256];
		GetWindowText(hwnd, WindowText, sizeof(WindowText));

		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkMode(hdc, TRANSPARENT);
		rect.top += 4;
		rect.left += 4;

		//DrawText( hdc, WindowText, -1, &rect, DT_SINGLELINE );
	}
}

void DrawWindow(HWND hwnd, HDC hdc)
{
	std::vector<HWND> hwndList;

	RECT rect;
	GetWindowRect(hwnd, &rect);

	EnumChildWindows(hwnd, EnumWindowsReverseProc, (LPARAM)& hwndList);
	hwndList.push_back(hwnd);

	for (vector<HWND>::reverse_iterator rit = hwndList.rbegin(); rit != hwndList.rend(); rit++)
	{
		DrawWindowInternal(*rit, hdc);
	}

	/*
	TCHAR WindowText[ 256 ];
	GetWindowText( WindowText, sizeof( WindowText ) );

	SetTextColor( hdc, RGB( 0, 0, 0 ) );
	SetBkMode( hdc, TRANSPARENT );
	rect.top += 4;
	rect.left += 4;

	DrawText( hdc, WindowText, -1, &rect, DT_SINGLELINE );
	*/
}
#endif

typedef struct {
	int ScreenWidth;
	int ScreenHeight;
	HDC hdcMemory;
	HDC hdcSource;
} EnumWindowParam;

HBITMAP ghTempBitmap = NULL;
static int gTempBitmapWidth = 0;
static int gTempBitmapHeight = 0;

// The full-screen target, kept across frames.
//
// A DIB section rather than a CreateCompatibleBitmap, because its bits are
// directly addressable: the frame can be handed to GDI+ and compared against the
// previous one without either of the two full-screen copies a device-dependent
// bitmap would need. Measured, those copies were 2.4-3.7 ms of an 8.6 ms idle
// frame. Reused until the screen size changes.
static HBITMAP gTargetBitmap = NULL;
static void* gTargetBits = NULL;
static int gTargetWidth = 0;
static int gTargetHeight = 0;

// The frame row the dock starts at, and the frame's width.
//
// The dock is drawn over the bottom of the desktop rather than appended below
// it, so the frame is exactly the screen and the host's window is the same size
// whether the dock is on or not. DockTop is then the screen's height minus the
// dock's, and equals the frame's own height when the dock is off.
static int gFrameWidth = 0;
static int gDockTop = 0;

// PrintWindow() calls made since the last PrintScreen() reset.
static unsigned gPrintWindowCalls = 0;

// Profiling is off unless SANDBOX_DEBUG is set, and when it is off nothing below
// reads the clock.
//
// Measured on the development machine, a QueryPerformanceCounter pair plus the
// conversion costs 31.7 ns, and a frame takes 9 + (one per window) of them --
// about 0.3 us idle and 0.8 us with a dialog open, or 0.003% of a 33 ms frame. It
// is not a real cost; it is gated because there is no reason for it not to be.
static const bool gProfile = DebugEnabled();

// Breakdown of the last PrintScreen() pass, and the clock its parts are timed
// against.
static CaptureBreakdown gBreakdown = { 0 };
static LARGE_INTEGER gQpcFrequency = { 0 };

// The two halves of the window pass, folded into gBreakdown when it ends.
static ULONGLONG gTopLevelPrintUs = 0;
static ULONGLONG gChildReprintUs = 0;
static unsigned gTopLevelCalls = 0;
static unsigned gChildCalls = 0;

// Starts a timer, or leaves it zero when profiling is off.
static void Begin(LARGE_INTEGER& Start)
{
	if (gProfile)
	{
		QueryPerformanceCounter(&Start);
	}
}

// Microseconds since Begin, or 0 when profiling is off.
static ULONGLONG Since(const LARGE_INTEGER& Start)
{
	if (!gProfile)
	{
		return 0;
	}

	LARGE_INTEGER Now;
	QueryPerformanceCounter(&Now);

	return (ULONGLONG)((Now.QuadPart - Start.QuadPart) * 1000000LL / gQpcFrequency.QuadPart);
}

// Paints hwnd into hdc and counts the call.
//
// There used to be a variant here that sent WM_PRINT directly instead of calling
// PrintWindow, to avoid the PRF_CHILDREN recursion. It does not work: an HDC is
// only meaningful in the calling process, so the frame came back empty. See
// Docs/PERFORMANCE.md, experiment B.
static void PaintWindow(HWND hwnd, HDC hdc)
{
	PrintWindow(hwnd, hdc, 0);

	++gPrintWindowCalls;
}

// Re-prints every descendant of hwnd into hdc.
//
// The container has no DWM, so WM_PRINT is the only way to get pixels at all,
// and its PRF_CHILDREN pass misses themed common controls; printing each
// descendant again on top fixes that.
//
// GetWindow(GW_CHILD)/GW_HWNDNEXT walks from the topmost child down, so the
// children are iterated in reverse to leave the topmost painted last.
// OriginX/OriginY are the screen coordinates of hdc's origin.
static void PrintWindowTree(HWND hwnd, HDC hdc, int OriginX, int OriginY)
{
	std::vector<HWND> Children;

	for (HWND hChild = GetWindow(hwnd, GW_CHILD); hChild; hChild = GetWindow(hChild, GW_HWNDNEXT))
	{
		Children.push_back(hChild);
	}

	for (std::vector<HWND>::reverse_iterator it = Children.rbegin(); it != Children.rend(); ++it)
	{
		if (!IsWindowVisible(*it))
		{
			continue;
		}

		RECT r;
		GetWindowRect(*it, &r);

		POINT OldOrigin;
		SetViewportOrgEx(hdc, r.left - OriginX, r.top - OriginY, &OldOrigin);

		LARGE_INTEGER PrintStart;
		Begin(PrintStart);

		PaintWindow(*it, hdc);
		++gChildCalls;

		gChildReprintUs += Since(PrintStart);

		PrintWindowTree(*it, hdc, OriginX, OriginY);

		SetViewportOrgEx(hdc, OldOrigin.x, OldOrigin.y, NULL);
	}
}

BOOL CALLBACK EnumWindowsProc(
	_In_ HWND   hwnd,
	_In_ LPARAM lParam
)
{
	EnumWindowParam* Param = (EnumWindowParam*)lParam;
	WINDOWINFO WindowInfo;

	GetWindowInfo(hwnd, &WindowInfo);

	if (!(WindowInfo.dwStyle & WS_VISIBLE) || IsIconic(hwnd))
	{
		return TRUE;
	}

	RECT r;
	GetWindowRect(hwnd, &r);

	// Where the window's rect starts, which is not where it is drawn.
	//
	// A maximized window's rect runs past the screen by the width of the
	// invisible resize border Windows gives it, so GetWindowRect returns a rect
	// larger than the display -- (-8, -8, 1032, 750) for a 1024x768 screen. That
	// is what the descendants below are positioned against, so it is kept.
	const int OriginX = r.left;
	const int OriginY = r.top;

	// What is actually drawn has to be clamped to the screen.
	//
	// BitBlt clips its destination, so a window drawn at a negative origin loses
	// the matching strip at the far edge: the copy runs off the target and the
	// last few columns are never written, leaving bare desktop where the
	// window's right edge should be. It is the same at the bottom, which is
	// normally hidden by the dock.
	int Left = r.left;
	int Top = r.top;
	int Right = r.right;
	int Bottom = r.bottom;

	if (Left < 0) Left = 0;
	if (Top < 0) Top = 0;
	if (Right > Param->ScreenWidth) Right = Param->ScreenWidth;
	if (Bottom > Param->ScreenHeight) Bottom = Param->ScreenHeight;

	const int x = Left;
	const int y = Top;
	const int w = Right - Left;
	const int h = Bottom - Top;

	if (w <= 0 || h <= 0)
	{
		return TRUE;
	}

	// Capture through PrintWindow, then re-print the descendants.
	//
	// BitBlt from the window's own DC is not an option: this container has no
	// DWM, so there is no composited screen surface to read and the result is
	// unusable. WM_PRINT is the only way to obtain pixels, and its child pass
	// is the part that needs the extra pass below.
	HDC hdcWindow = GetDCEx(hwnd, NULL, DCX_CACHE | DCX_WINDOW);
	HDC hdcWindowMemory = CreateCompatibleDC(hdcWindow);

	SelectObject(hdcWindowMemory, ghTempBitmap);

	// ghTempBitmap is one scratch bitmap shared by every window and every
	// frame, and no capture pass is obliged to paint all of it. Fill with the
	// desktop colour first so anything left unpainted blends into the
	// background instead of showing the previously captured window's pixels.
	//
	// Only the part that will be copied out, which starts where the window's
	// rect does rather than at the scratch's own corner.
	const int SrcX = x - OriginX;
	const int SrcY = y - OriginY;

	RECT Scratch{ SrcX, SrcY, SrcX + w, SrcY + h };
	FillRect(hdcWindowMemory, &Scratch, GetSysColorBrush(COLOR_DESKTOP));

	LARGE_INTEGER PrintStart;
	Begin(PrintStart);

	PaintWindow(hwnd, hdcWindowMemory);
	++gTopLevelCalls;

	gTopLevelPrintUs += Since(PrintStart);

	// Re-print every visible descendant. PrintWindow reaches children through
	// its PRF_CHILDREN pass, and that pass does not reliably draw themed common
	// controls -- a status bar comes out with its background but no parts.
	PrintWindowTree(hwnd, hdcWindowMemory, OriginX, OriginY);

	BitBlt(Param->hdcMemory, x, y, w, h, hdcWindowMemory, SrcX, SrcY, SRCCOPY);

	DeleteDC(hdcWindowMemory);
	ReleaseDC(hwnd, hdcWindow);

	return TRUE;
}

void ReadCursor(CursorInfo& Out)
{
	Out = CursorInfo();

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
	// flags 0 while still having a cursor worth drawing. The original DrawCursor
	// tested for SUPPRESSED too, which is why the cursor used to appear.
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

// Returns the full-screen DIB, rebuilding it only when the size changes, and
// hands back a pointer to its bits.
static HBITMAP EnsureTargetBitmap(HDC hdcSource, int Width, int Height, void** Bits)
{
	if (gTargetBitmap && gTargetWidth == Width && gTargetHeight == Height)
	{
		*Bits = gTargetBits;
		return gTargetBitmap;
	}

	if (gTargetBitmap)
	{
		DeleteObject(gTargetBitmap);
		gTargetBitmap = NULL;
		gTargetBits = NULL;
	}

	BITMAPINFO Info = { 0 };
	Info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	Info.bmiHeader.biWidth = Width;
	Info.bmiHeader.biHeight = -Height;   // top-down, so the bits start at the top row
	Info.bmiHeader.biPlanes = 1;
	Info.bmiHeader.biBitCount = 32;
	Info.bmiHeader.biCompression = BI_RGB;

	gTargetBitmap = CreateDIBSection(hdcSource, &Info, DIB_RGB_COLORS, Bits, NULL, 0);

	if (!gTargetBitmap)
	{
		return NULL;
	}

	gTargetBits = *Bits;
	gTargetWidth = Width;
	gTargetHeight = Height;

	return gTargetBitmap;
}

Bitmap* PrintScreen(HDESK hDesktop)
{
	// Reset on entry so the caller can read this frame's count once we return.
	gPrintWindowCalls = 0;
	gTopLevelPrintUs = 0;
	gChildReprintUs = 0;
	gTopLevelCalls = 0;
	gChildCalls = 0;

	if (gProfile && !gQpcFrequency.QuadPart)
	{
		QueryPerformanceFrequency(&gQpcFrequency);
	}

	LARGE_INTEGER TotalStart;
	Begin(TotalStart);

	// Get the display DC
	HDC hdcSource = GetDC(NULL);
	if (!hdcSource)
	{
		Debug() << "DC could not be retrieved" << endl;
		return NULL;
	}

	// Display resolution
	int capX = GetDeviceCaps(hdcSource, HORZRES);
	int capY = GetDeviceCaps(hdcSource, VERTRES);

	// The dock lies over the bottom of the screen, not below it, so the frame is
	// exactly the screen and the host's window does not change size when the
	// dock is switched on. What gives way instead is the desktop's work area,
	// which the client reserves to match; see ReserveScreenBottom.
	const int FrameHeight = capY;

	// Where the dock begins, and so what the input path measures dock clicks
	// against. Recorded here rather than at the end, so a capture that fails
	// partway through still leaves the input path with a usable answer.
	gFrameWidth = capX;
	gDockTop = capY - DockHeight();

	HDC hdcMemory = CreateCompatibleDC(hdcSource);

	// One scratch bitmap shared by every window and every frame, so it is not
	// reallocated per window. Rebuilt only when the screen size changes, which
	// it can: see SANDBOX_RESOLUTION.
	//
	// Bigger than the screen by the widest resize border the system hands out,
	// and it has to be. A maximized window's rect runs past the screen by that
	// much on every side -- (-8, -8, 1032, 750) on a 1024x768 display -- and
	// PrintWindow paints the window into the DC by its rect, so the part that is
	// actually on screen starts at the border offset rather than at the DC's
	// corner. A scratch the size of the screen clips the far edge away before
	// the copy out, which is what left a strip of bare desktop down the right of
	// a maximized window.
	const int Border = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
	const int ScratchWidth = capX + Border * 2;
	const int ScratchHeight = capY + Border * 2;

	if (!ghTempBitmap || gTempBitmapWidth != ScratchWidth || gTempBitmapHeight != ScratchHeight)
	{
		if (ghTempBitmap)
		{
			DeleteObject(ghTempBitmap);
		}

		ghTempBitmap = CreateCompatibleBitmap(hdcSource, ScratchWidth, ScratchHeight);
		gTempBitmapWidth = ScratchWidth;
		gTempBitmapHeight = ScratchHeight;
	}

	// The full-screen target, reused across frames. See gTargetBitmap.
	LARGE_INTEGER AllocStart;
	Begin(AllocStart);

	void* Bits = NULL;
	HBITMAP hBitmap = EnsureTargetBitmap(hdcSource, capX, FrameHeight, &Bits);

	gBreakdown.BitmapAlloc = Since(AllocStart);

	if (!hBitmap)
	{
		DeleteDC(hdcMemory);
		ReleaseDC(NULL, hdcSource);

		return NULL;
	}

	HGDIOBJ hOldTarget = SelectObject(hdcMemory, hBitmap);

	EnumWindowParam EnumParam;
	EnumParam.ScreenWidth = capX;
	EnumParam.ScreenHeight = capY;
	EnumParam.hdcMemory = hdcMemory;
	EnumParam.hdcSource = hdcSource;

	// Fill background with the desktop color.
	//
	// FillRect covers the whole DC, so the Rectangle() that used to precede it
	// -- which filled the same area with the DC's default white brush -- was
	// overwritten in full and only bought a second full-screen pass.
	LARGE_INTEGER FillStart;
	Begin(FillStart);

	RECT r{ 0, 0, capX, capY };
	FillRect(hdcMemory, &r, GetSysColorBrush(COLOR_DESKTOP));

	gBreakdown.Fills = Since(FillStart);

	std::vector<HWND> hwndList;

	LARGE_INTEGER PassStart;
	Begin(PassStart);

	// Gather list of windows, so they can be drawn in reverse order
	// otherwise the back windows will appear first
	EnumDesktopWindows(hDesktop, EnumWindowsReverseProc, (LPARAM)& hwndList);

	for (std::vector<HWND>::reverse_iterator rit = hwndList.rbegin(); rit != hwndList.rend(); rit++)
	{
		EnumWindowsProc(*rit, (LPARAM)& EnumParam);
	}

	gBreakdown.WindowPass = Since(PassStart);
	gBreakdown.TopLevelPrint = gTopLevelPrintUs;
	gBreakdown.ChildReprint = gChildReprintUs;
	gBreakdown.TopLevelCalls = gTopLevelCalls;
	gBreakdown.ChildCalls = gChildCalls;

	// The dock goes on last, so it covers the windows behind it -- which is what
	// a taskbar does, and why dragging a window down makes it slide behind the
	// dock rather than under the pointer. It is drawn rather than composited by
	// the host so it can use GDI's text; see Dock.h.
	LARGE_INTEGER DockStart;
	Begin(DockStart);

	DrawDock(hdcMemory, 0, capY - DockHeight(), capX);

	gBreakdown.Dock = Since(DockStart);

	// If you uncomment this, it will draw the entire screen but it will look very messy, and there will be no console windows
	//BitBlt(hdcMemory, 0, 0, capX, capY, hdcSource, 0, 0, SRCCOPY);

	// The cursor is deliberately not drawn here. See ReadCursor.

	// Deselect before the DC goes away: the bitmap outlives it now.
	SelectObject(hdcMemory, hOldTarget);
	DeleteDC(hdcMemory);
	ReleaseDC(NULL, hdcSource);

	LARGE_INTEGER GdiPlusStart;
	Begin(GdiPlusStart);

	// GDI leaves a DIB section's alpha byte at zero, so set it.
	//
	// The wrap below is PixelFormat32bppARGB and the host's texture is RGBA32,
	// so the PNG has to stay 32bpp; an untouched alpha would encode a fully
	// transparent frame. One pass over the screen, against the full-screen copy
	// this replaces.
	{
		DWORD* Pixel = (DWORD*)Bits;
		const size_t Count = (size_t)capX * FrameHeight;

		for (size_t i = 0; i < Count; ++i)
		{
			Pixel[i] |= 0xFF000000u;
		}
	}

	// Wrap the DIB bits rather than copying them into a GDI+ bitmap, which is
	// what Bitmap::FromHBITMAP does.
	//
	// The returned bitmap does not own the pixels, and must not outlive the next
	// PrintScreen call, which redraws the same buffer.
	Bitmap* bmp = new Bitmap(capX, FrameHeight, capX * 4, PixelFormat32bppARGB, (BYTE*)Bits);

	if (bmp && bmp->GetLastStatus() != Ok)
	{
		delete bmp;
		bmp = NULL;
	}

	gBreakdown.ToGdiPlus = Since(GdiPlusStart);

	gBreakdown.Total = Since(TotalStart);

	return bmp;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT dwMessage, WPARAM wParam, LPARAM lParam)
{
	switch (dwMessage)
	{
	case WM_ACTIVATE:
		if (wParam)
		{
			SetForegroundWindow((HWND)lParam);
		}
		return 0;
	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		if (BeginPaint(hWnd, &Paint))
		{
			EndPaint(hWnd, &Paint);
		}
	}
		return 0;
	}

	return DefWindowProc(hWnd, dwMessage, wParam, lParam);
}

void RegisterDesktopWindow()
{
	INT Elements[] = { COLOR_DESKTOP };
	COLORREF RgbValues[] = { RGB(8, 110, 165) };
	SetSysColors(1, Elements, RgbValues);

	/*
	HBRUSH Brush = CreateSolidBrush(RGB(0, 255, 255));

	WNDCLASSEX wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.style = CS_DBLCLKS;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.lpszClassName = TEXT("DesktopWnd");
	wndClass.lpfnWndProc = WindowProc;
	wndClass.hbrBackground = Brush;

	RegisterClassEx(&wndClass);

	HDC hDeviceDC = GetDC(NULL);

	hDesktopWnd = CreateWindowEx(
		WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		TEXT("DesktopWnd"),
		NULL,
		WS_VISIBLE | WS_CLIPSIBLINGS,
		0,
		0,
		GetDeviceCaps(hDeviceDC, HORZRES),
		GetDeviceCaps(hDeviceDC, VERTRES),
		NULL,
		NULL,
		NULL,
		NULL
	);

	ReleaseDC(NULL, hDeviceDC);


	hDesktopDC = GetWindowDC(hDesktopWnd);
	*/
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0;          // number of image encoders
	UINT size = 0;         // size of the image encoder array in bytes

	ImageCodecInfo* pImageCodecInfo = NULL;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
	{
		return -1;  // Failure
	}

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
	{
		return -1;  // Failure
	}

	GetImageEncoders(num, size, pImageCodecInfo);

	for (UINT j = 0; j < num; ++j)
	{
		if (_wcsicmp(pImageCodecInfo[j].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

unsigned GetLastPrintWindowCallCount()
{
	return gPrintWindowCalls;
}

const unsigned char* GetLastFrameBits()
{
	return (const unsigned char*)gTargetBits;
}

int GetLastFrameStride()
{
	// Top-down 32bpp DIB: rows are contiguous, no padding.
	return gTargetWidth * 4;
}

int GetLastFrameWidth()
{
	return gFrameWidth;
}

int GetLastDockTop()
{
	return gDockTop;
}

const CaptureBreakdown& GetLastCaptureBreakdown()
{
	return gBreakdown;
}
