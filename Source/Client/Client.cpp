#include "Client.h"
#include "Init.h"
#include "../Interface/sandbox_h.h"
#include "../Common/SandboxEnv.h"
#include "../Common/SandboxLog.h"
#include "printwnd.h"
#include <rpc.h>
#include <stdio.h>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>

#include <iostream>

// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is Windows 10 1803+; define it here in
// case the SDK headers predate it.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

using namespace std;

class RpcStream : public IStream
{

public:
	virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, _Out_opt_ ULARGE_INTEGER* plibNewPosition)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE CopyTo(_In_ IStream* pstm, ULARGE_INTEGER cb, _Out_opt_ ULARGE_INTEGER* pcbRead, _Out_opt_ ULARGE_INTEGER* pcbWritten)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Revert(void)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Stat(__RPC__out STATSTG* pstatstg, DWORD grfStatFlag)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Clone(__RPC__deref_out_opt IStream** ppstm)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Read(_Out_writes_bytes_to_(cb, *pcbRead) void* pv, _In_ ULONG cb, _Out_opt_ ULONG* pcbRead)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE Write(_In_reads_bytes_(cb) const void* pv, _In_ ULONG cb, _Out_opt_ ULONG* pcbWritten)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		throw std::logic_error("The method or operation is not implemented.");
	}

	virtual ULONG STDMETHODCALLTYPE AddRef()
	{
		return 0;
	}

	virtual ULONG STDMETHODCALLTYPE Release()
	{
		return 0;
	}
};

BOOL CALLBACK WndCountProc(
	HWND hwnd,
	LPARAM lParam
)
{
	DWORD* pdwCount = (DWORD*)lParam;

	if (IsWindowVisible(hwnd))
	{
		*pdwCount = *pdwCount + 1;
	}

	return TRUE;
}

Gdiplus::Bitmap* DrawFinishedText()
{
	RECT rect;
	GetWindowRect(GetDesktopWindow(), &rect);

	HDC hDC = GetDC(NULL);

	Gdiplus::Graphics* graphics = new Gdiplus::Graphics(hDC);

	Gdiplus::Font font(L"Arial.ttf", 1);
	Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255));

	graphics->DrawString(L"Container has exited.", 21, &font, Gdiplus::PointF(0, 0), &brush);
	Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(rect.right, rect.bottom, graphics);

	ReleaseDC(NULL, hDC);

	return bitmap;
}

// Compares the current frame against the previous one.
//
// The frame is a top-down 32bpp DIB section with no row padding, so it is one
// contiguous block and a single memcmp covers it. Reading it through GDI+
// LockBits, as this used to, copied the whole screen first.
static bool FrameMatches(const unsigned char* Bits, size_t Size, const std::vector<unsigned char>& Previous)
{
	// The first frame, or a size that changed with the screen: report a
	// difference so the frame is sent rather than silently skipped forever.
	if (!Bits || Previous.size() != Size)
	{
		return false;
	}

	return memcmp(Bits, Previous.data(), Size) == 0;
}

static void CopyFrame(const unsigned char* Bits, size_t Size, std::vector<unsigned char>& Out)
{
	Out.assign(Bits, Bits + Size);
}

// Previous frame's pixels, used to skip frames that did not change.
//
// File scope, not local: SandboxInitClient contains an RpcTryExcept (__try)
// block, and MSVC rejects __try in a function that needs object unwinding.
static std::vector<unsigned char> gPreviousFrame;

// The container's cursor, read once per frame. File scope for the same reason.
static CursorInfo gCursor;

// Frame profiler. Silent unless SANDBOX_DEBUG is set.
//
// capture / encode / send are tracked apart because they scale with different
// things: capture with the number of windows, encode with screen area, send
// with encoded size. The report format and the measured numbers are in
// Docs/PERFORMANCE.md.
static const unsigned ProfileSamples = 128;

struct FrameProfile
{
	ULONGLONG Capture[ProfileSamples];
	ULONGLONG Encode[ProfileSamples];
	ULONGLONG Send[ProfileSamples];
	unsigned  Calls[ProfileSamples];   // PrintWindow calls made by that frame
	ULONGLONG CaptureTotal;
	ULONGLONG EncodeTotal;
	ULONGLONG SendTotal;

	// The capture pass broken down, accumulated so the report can show means.
	ULONGLONG AllocTotal;
	ULONGLONG FillsTotal;
	ULONGLONG PassTotal;
	ULONGLONG TopPrintTotal;
	ULONGLONG ChildPrintTotal;
	unsigned  TopPrintCalls;
	unsigned  ChildPrintCalls;
	ULONGLONG CursorTotal;
	ULONGLONG GdiPlusTotal;

	unsigned  Count;
	unsigned  FramesSent;
	unsigned  FramesDropped;   // server refused: previous frame not presented
	ULONGLONG WindowStart;
};

static FrameProfile gProfile;
static LARGE_INTEGER gFrequency;

// Profiling is off unless SANDBOX_DEBUG is set, and when it is off nothing below
// reads the clock. Measured on the development machine, a QueryPerformanceCounter
// pair plus the conversion costs 31.7 ns, so four of them per frame is 127 ns --
// 0.0004% of a 33 ms frame. It is gated because there is no reason for it not to
// be, not because it was a cost.
static const bool gProfiling = DebugEnabled();

// Starts a timer, or leaves it zero when profiling is off.
static void BeginTimer(LARGE_INTEGER& Start)
{
	if (gProfiling)
	{
		QueryPerformanceCounter(&Start);
	}
}

// Microseconds since BeginTimer, or 0 when profiling is off.
static ULONGLONG MicrosecondsSince(const LARGE_INTEGER& Start)
{
	if (!gProfiling)
	{
		return 0;
	}

	LARGE_INTEGER Now;
	QueryPerformanceCounter(&Now);

	return (ULONGLONG)((Now.QuadPart - Start.QuadPart) * 1000000LL / gFrequency.QuadPart);
}

static ULONGLONG Percentile(const ULONGLONG* Values, unsigned Count, unsigned Pct)
{
	if (!Count)
	{
		return 0;
	}

	std::vector<ULONGLONG> Sorted(Values, Values + Count);
	std::sort(Sorted.begin(), Sorted.end());

	return Sorted[(size_t)Pct * (Count - 1) / 100];
}

static ULONGLONG MaxOf(const ULONGLONG* Values, unsigned Count)
{
	ULONGLONG Max = 0;

	for (unsigned i = 0; i < Count; ++i)
	{
		if (Values[i] > Max)
		{
			Max = Values[i];
		}
	}

	return Max;
}

static unsigned MaxOf(const unsigned* Values, unsigned Count)
{
	unsigned Max = 0;

	for (unsigned i = 0; i < Count; ++i)
	{
		if (Values[i] > Max)
		{
			Max = Values[i];
		}
	}

	return Max;
}

static void ReportFrameProfile(DWORD FrameIntervalMs)
{
	const unsigned N = gProfile.Count;

	if (!N)
	{
		return;
	}

	const ULONGLONG ElapsedMs = GetTickCount64() - gProfile.WindowStart;

	Debug() << "--- capture profile: " << N << " frames in " << ElapsedMs
	     << " ms, " << gProfile.FramesSent << " sent, "
	     << gProfile.FramesDropped << " dropped by server ---" << endl;

	Debug() << "  capture us   mean " << (gProfile.CaptureTotal / N)
	     << "  p50 " << Percentile(gProfile.Capture, N, 50)
	     << "  p95 " << Percentile(gProfile.Capture, N, 95)
	     << "  max " << MaxOf(gProfile.Capture, N) << endl;

	Debug() << "  encode  us   mean " << (gProfile.EncodeTotal / N)
	     << "  p50 " << Percentile(gProfile.Encode, N, 50)
	     << "  p95 " << Percentile(gProfile.Encode, N, 95)
	     << "  max " << MaxOf(gProfile.Encode, N) << endl;

	Debug() << "  send    us   mean " << (gProfile.SendTotal / N)
	     << "  p50 " << Percentile(gProfile.Send, N, 50)
	     << "  p95 " << Percentile(gProfile.Send, N, 95)
	     << "  max " << MaxOf(gProfile.Send, N) << endl;

	Debug() << "  PrintWindow calls per frame   max " << MaxOf(gProfile.Calls, N) << endl;

	// The part worth optimising: it does not shrink with better encoding.
	Debug() << "  capture split  us   alloc " << (gProfile.AllocTotal / N)
	     << "  fills " << (gProfile.FillsTotal / N)
	     << "  windows " << (gProfile.PassTotal / N)
	     << "  toGdi+ " << (gProfile.GdiPlusTotal / N) << endl;

	Debug() << "  cursor read    us   " << (gProfile.CursorTotal / N) << endl;

	// Per-call costs, because a top-level window and a child button differ by
	// an order of magnitude.
	Debug() << "    of which windows   toplevel " << (gProfile.TopPrintTotal / N) << " us";
	if (gProfile.TopPrintCalls)
	{
		Debug() << " x" << gProfile.TopPrintCalls << " => "
		     << (gProfile.TopPrintTotal / gProfile.TopPrintCalls) << " us/call";
	}
	Debug() << endl;

	Debug() << "                       child    " << (gProfile.ChildPrintTotal / N) << " us";
	if (gProfile.ChildPrintCalls)
	{
		Debug() << " x" << gProfile.ChildPrintCalls << " => "
		     << (gProfile.ChildPrintTotal / gProfile.ChildPrintCalls) << " us/call";
	}
	Debug() << endl;

	// The capacity answer: a frame's cost as a share of the pacing interval is
	// the fraction of one core one container needs, and its inverse is how many
	// fit. Counted over all three phases, not capture alone.
	const ULONGLONG MeanTotal =
		(gProfile.CaptureTotal + gProfile.EncodeTotal + gProfile.SendTotal) / N;

	ULONGLONG WorstTotal = 0;
	unsigned WorstIndex = 0;

	for (unsigned i = 0; i < N; ++i)
	{
		const ULONGLONG Total = gProfile.Capture[i] + gProfile.Encode[i] + gProfile.Send[i];

		if (Total > WorstTotal)
		{
			WorstTotal = Total;
			WorstIndex = i;
		}
	}

	const ULONGLONG BudgetUs = (ULONGLONG)FrameIntervalMs * 1000;

	if (MeanTotal && BudgetUs)
	{
		Debug() << "  mean frame " << MeanTotal << " us = " << (MeanTotal * 100 / BudgetUs)
		     << "% of the " << FrameIntervalMs << " ms budget  =>  ~"
		     << (1000ULL * FrameIntervalMs / MeanTotal) << " containers per core" << endl;

		Debug() << "  worst frame " << WorstTotal << " us (capture " << gProfile.Capture[WorstIndex]
		     << " + encode " << gProfile.Encode[WorstIndex]
		     << " + send " << gProfile.Send[WorstIndex] << ") = "
		     << (WorstTotal * 100 / BudgetUs) << "% of budget  =>  ~"
		     << (1000ULL * FrameIntervalMs / WorstTotal) << " containers per core" << endl;
	}

	gProfile.Count = 0;
	gProfile.FramesSent = 0;
	gProfile.FramesDropped = 0;
	gProfile.CaptureTotal = 0;
	gProfile.EncodeTotal = 0;
	gProfile.SendTotal = 0;
	gProfile.AllocTotal = 0;
	gProfile.FillsTotal = 0;
	gProfile.PassTotal = 0;
	gProfile.TopPrintTotal = 0;
	gProfile.ChildPrintTotal = 0;
	gProfile.TopPrintCalls = 0;
	gProfile.ChildPrintCalls = 0;
	gProfile.CursorTotal = 0;
	gProfile.GdiPlusTotal = 0;
	gProfile.WindowStart = GetTickCount64();
}

// Input events fetched per frame.
//
// The host queues whatever arrived since the last frame, which at 30 fps is a
// handful even during fast typing; this only bounds the reply buffer.
static const unsigned MaxEventsPerFrame = 32;

// NDR marshals a conformant array as a [ref] pointer, so the "no frame this
// time" case still needs a valid address even though nothing reads it.
static const unsigned char gNoFrame = 0;

// Injects one input event into the container's desktop.
//
// Kept out of the frame loop so that loop's RpcTryExcept (__try) block stays
// small; MSVC rejects __try in a function that also needs object unwinding.
static void ApplyInputEvent(const SandboxEvent& event)
{
	switch (event.eventType)
	{
	case SBET_MouseEvent:
		// Position comes from SetCursorPos; the dx/dy below are ignored
		// because MOUSEEVENTF_ABSOLUTE is not set.
		SetCursorPos(event.mouseEvent.x, event.mouseEvent.y);

		if (event.mouseEvent.buttonIndex != -1)
		{
			INPUT input = { 0 };
			input.type = INPUT_MOUSE;
			input.mi.dx = event.mouseEvent.x;
			input.mi.dy = event.mouseEvent.y;

			switch (event.mouseEvent.buttonIndex)
			{
			case 1:
				input.mi.dwFlags = event.mouseEvent.bKeyDown ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
				break;
			case 2:
				input.mi.dwFlags = event.mouseEvent.bKeyDown ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
				break;
			case 3:
				input.mi.dwFlags = event.mouseEvent.bKeyDown ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
				break;
			}

			if (!SendInput(1, &input, sizeof(INPUT)))
			{
				Debug() << "SendInput for mouse returned false with error " << hex << GetLastError() << endl;
			}
		}
		break;

	case SBET_WheelEvent:
	{
		// The system delivers the wheel to the window with keyboard focus, but
		// the message carries the cursor position, so the cursor has to be where
		// the user was pointing.
		SetCursorPos(event.wheelEvent.x, event.wheelEvent.y);

		INPUT input = { 0 };
		input.type = INPUT_MOUSE;

		// One notch is WHEEL_DELTA, and the sign carries over from the host:
		// positive is away from the user vertically, to the right horizontally.
		// mouseData is a DWORD holding a signed delta, so the cast is intended.
		if (event.wheelEvent.delta)
		{
			input.mi.dwFlags = MOUSEEVENTF_WHEEL;
			input.mi.mouseData = (DWORD)event.wheelEvent.delta;

			if (!SendInput(1, &input, sizeof(INPUT)))
			{
				Debug() << "SendInput for wheel returned false with error " << hex << GetLastError() << endl;
			}
		}

		if (event.wheelEvent.hdelta)
		{
			input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
			input.mi.mouseData = (DWORD)event.wheelEvent.hdelta;

			if (!SendInput(1, &input, sizeof(INPUT)))
			{
				Debug() << "SendInput for horizontal wheel returned false with error " << hex << GetLastError() << endl;
			}
		}
	}
		break;

	case SBET_KeyEvent:
	{
		WORD scan = (WORD)event.keyboardEvent.keyIndex;

		INPUT input = { 0 };
		input.type = INPUT_KEYBOARD;
		input.ki.dwFlags = event.keyboardEvent.bKeyDown ? 0 : KEYEVENTF_KEYUP;

		input.ki.dwFlags |= KEYEVENTF_SCANCODE;

		// The host's SDL-to-DIK table stores extended keys with bit 0x80 set
		// (arrows, Home/End, PgUp/PgDn, Insert/Delete, numpad Enter and Divide,
		// right Ctrl/Alt, Win/Menu). Strip the marker and flag the key as
		// extended, otherwise the system does not recognise the scan code and
		// the key is dropped.
		if (scan & 0x80)
		{
			scan &= 0x7F;
			input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		}

		input.ki.wScan = scan;
		input.ki.wVk = (WORD)MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);

		if (!SendInput(1, &input, sizeof(INPUT)))
		{
			Debug() << "SendInput returned false with error " << hex << GetLastError() << endl;
		}
	}
		break;
	}
}

void SandboxInitClient(const wchar_t* Host, const wchar_t* Command)
{
	RPC_WSTR StringBinding;

	RPC_BINDING_HANDLE BindingHandle;

	Debug() << "Configuring window stations..." << endl;
	// Set window station and desktop access rights
	TweakWindowStation();

	Debug() << "Assigning current process to the default window station..." << endl;
	// Assign this program to WinSta0 and the default desktop
	AssignWindowStation();

	Debug() << "Registering desktop window..." << endl;
	// Set some colors
	RegisterDesktopWindow();

	// Before the program starts, so it opens at the right size.
	//
	// The frame path adapts on its own: the capture reads the screen size every
	// frame and rebuilds its buffers when it changes, and the server rebuilds
	// its texture when the frame size changes.
	{
		int ModeWidth = 0;
		int ModeHeight = 0;

		if (SandboxResolution(ModeWidth, ModeHeight))
		{
			SetDisplayMode(ModeWidth, ModeHeight);
		}
	}

	Debug() << "Starting " << (Command && *Command ? "the requested program" : "CMD") << "..." << endl;
	StartProgram(Command);

	// The container's display mode is changeable but not changed here; see
	// Docs/IMAGE-ANALYSIS.md, "Display resolution".

	Debug() << "Composing...";

	// Port from SANDBOX_PORT, defaulting to the compiled value; see SandboxEnv.
	RPC_STATUS Status = RpcStringBindingCompose(
		NULL,
		(RPC_WSTR)TEXT("ncacn_ip_tcp"),
		(RPC_WSTR)Host,
		(RPC_WSTR)SandboxPort(),
		NULL,
		&StringBinding
	);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	Debug() << "Getting binding..." << endl;
	Status = RpcBindingFromStringBinding(StringBinding, &BindingHandle);
	RpcStringFree(&StringBinding);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	Debug() << "Done." << endl;

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Capture rate. PrintWindow is a synchronous WM_PRINT and forces a repaint
	// of every window, so an uncapped loop starves controls and burns CPU on
	// frames the host drops.
	//
	// SANDBOX_FPS overrides it. The capture cost is roughly fixed per frame, so
	// this is the knob that trades container CPU for smoothness: 30 fps costs
	// about 30% of a core, 15 fps about half that. See Docs/PERFORMANCE.md.
	DWORD FrameIntervalMs = 33;   // ~30 fps

	{
		char Value[16] = { 0 };

		if (GetEnvironmentVariableA("SANDBOX_FPS", Value, sizeof(Value)))
		{
			const int Fps = atoi(Value);

			if (Fps >= 1 && Fps <= 240)
			{
				FrameIntervalMs = 1000 / (DWORD)Fps;
			}
		}
	}

	ULONGLONG LastFrameTick = GetTickCount64();

	// A frame identical to the previous one is not worth encoding and sending.
	// Measured, that is the large majority of frames; see Docs/PERFORMANCE.md.
	const bool bSkipUnchangedFrames = true;

	// The desktop this thread is on, which is the container's. Fetched once.
	HDESK hDesktop = GetThreadDesktop(GetCurrentThreadId());

	// How long an unchanged screen may go without a frame being sent anyway.
	// The server drops a frame silently when the previous one has not been
	// presented yet, so a frame skipped here plus one dropped there would leave
	// the host on an old image with nothing left to trigger a repaint.
	const ULONGLONG ResyncIntervalMs = 1000;

	// When the last frame was actually put on the wire.
	ULONGLONG LastSentTick = GetTickCount64();

	// Report where each frame's time goes. Off unless SANDBOX_DEBUG is set, in
	// which case nothing below is even sampled.
	const bool bProfileFrames = gProfiling;

	if (gProfiling)
	{
		QueryPerformanceFrequency(&gFrequency);
		gProfile.WindowStart = GetTickCount64();
	}

	// Frame pacing. Sleep() is quantised to the 15.6 ms system tick, which made
	// a 33 ms budget come out at 41-46 ms per frame; a high-resolution waitable
	// timer waits for the interval actually asked for.
	HANDLE hFrameTimer = CreateWaitableTimerExW(
		NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

	if (!hFrameTimer)
	{
		Debug() << "High-resolution timer unavailable (err " << hex << GetLastError()
		     << "), falling back to Sleep" << endl;
	}

	bool bContinue = true;
	while(bContinue)
	{
		DWORD dwCount = 0;
		EnumWindows(WndCountProc, (LPARAM)&dwCount);

		Gdiplus::Bitmap* Bitmap = nullptr;

		if (!dwCount)
		{
			// Break when there are no more windows
			Debug() << "No more visible windows, exiting." << endl;
			bContinue = false;
		}

		ULONGLONG Now = GetTickCount64();
		ULONGLONG CaptureUs = 0;

		LARGE_INTEGER CaptureStart;
		BeginTimer(CaptureStart);

		Bitmap = PrintScreen(hDesktop);

		CaptureUs = MicrosecondsSince(CaptureStart);

		// The cursor is read, not painted into the frame; the host composites
		// it. The bitmap is only read when the shape changes.
		LARGE_INTEGER CursorStart;
		BeginTimer(CursorStart);

		ReadCursor(gCursor);

		const ULONGLONG CursorUs = MicrosecondsSince(CursorStart);

		// Decide whether this frame is worth the encode/transfer/decode trip.
		bool bSendFrame = !bSkipUnchangedFrames || (Now - LastSentTick) >= ResyncIntervalMs;

		if (!bSendFrame && Bitmap)
		{
			const size_t FrameSize = (size_t)GetLastFrameStride() * Bitmap->GetHeight();

			bSendFrame = !FrameMatches(GetLastFrameBits(), FrameSize, gPreviousFrame);

			if (bSendFrame)
			{
				CopyFrame(GetLastFrameBits(), FrameSize, gPreviousFrame);
			}
		}

		// One frame's pixels and the input that comes back with them, in one
		// RPC call. See Source/Interface/sandbox.idl.
		unsigned long Size = 0;
		unsigned long FrameWidth = 0;
		unsigned long FrameHeight = 0;
		IStream* Stream = nullptr;
		HGLOBAL hGlobalMem = NULL;
		LPVOID lpGlobalMem = NULL;

		// Declared out here because the send happens inside RpcTryExcept.
		ULONGLONG EncodeUs = 0;
		ULONGLONG SendUs = 0;
		unsigned long EventCount = MaxEventsPerFrame;
		unsigned long Status = 0;
		SandboxEvent Events[MaxEventsPerFrame];

		if (bSendFrame)
		{
			LARGE_INTEGER EncodeStart;
			BeginTimer(EncodeStart);

			CLSID sid;
			GetEncoderClsid(L"image/png", &sid);

			CreateStreamOnHGlobal(NULL, TRUE, &Stream);

			Bitmap->Save(Stream, &sid, NULL);

			GetHGlobalFromStream(Stream, &hGlobalMem);

			Size = (unsigned long)GlobalSize(hGlobalMem);
			FrameWidth = Bitmap->GetWidth();
			FrameHeight = Bitmap->GetHeight();

			lpGlobalMem = GlobalLock(hGlobalMem);

			EncodeUs = MicrosecondsSince(EncodeStart);
		}

		const unsigned char* FrameData = Size ? (const unsigned char*)lpGlobalMem : &gNoFrame;

		RpcTryExcept
		{
			LARGE_INTEGER SendStart;
			BeginTimer(SendStart);

			if (bSendFrame)
			{
				LastSentTick = GetTickCount64();
			}

			Status = RpcSendFrame(
				BindingHandle,
				FrameWidth,
				FrameHeight,
				Size,
				FrameData,
				gCursor.X,
				gCursor.Y,
				gCursor.Visible ? 1 : 0,
				&EventCount,
				Events
			);

			// The shape changes a few times a minute, so it does not get a slot
			// in every frame.
			if (gCursor.ShapeChanged && !gCursor.Shape.empty())
			{
				RpcSetCursorShape(
					BindingHandle,
					gCursor.Width,
					gCursor.Height,
					gCursor.HotX,
					gCursor.HotY,
					(unsigned long)gCursor.Shape.size(),
					gCursor.Shape.data()
				);
			}

			SendUs = MicrosecondsSince(SendStart);

			// Input comes back with the frame, so a static screen still responds
			// to the keyboard and mouse even when no frame was sent (Size == 0).
			for (unsigned long i = 0; i < EventCount; ++i)
			{
				ApplyInputEvent(Events[i]);
			}
		}
		RpcExcept(1)
		{
			Debug() << "Exception " << hex << RpcExceptionCode() << " encountered while sending data" << endl;

			if (RpcExceptionCode() == EPT_S_NOT_REGISTERED
				|| RpcExceptionCode() == RPC_S_SERVER_UNAVAILABLE)
			{
				bContinue = false;
			}
		}
		RpcEndExcept

		if (lpGlobalMem)
		{
			GlobalUnlock(hGlobalMem);
		}

		if (Stream)
		{
			Stream->Release();
		}

		delete Bitmap;

		// Record this frame. Frames skipped by the unchanged-pixels check are
		// sampled too: they still pay for a full capture, and capture is the
		// cost under investigation.
		if (bProfileFrames && gProfile.Count < ProfileSamples)
		{
			gProfile.Capture[gProfile.Count] = CaptureUs;
			gProfile.Encode[gProfile.Count] = EncodeUs;
			gProfile.Send[gProfile.Count] = SendUs;
			gProfile.Calls[gProfile.Count] = GetLastPrintWindowCallCount();
			gProfile.CaptureTotal += CaptureUs;
			gProfile.EncodeTotal += EncodeUs;
			gProfile.SendTotal += SendUs;

			const CaptureBreakdown& B = GetLastCaptureBreakdown();
			gProfile.AllocTotal += B.BitmapAlloc;
			gProfile.FillsTotal += B.Fills;
			gProfile.PassTotal += B.WindowPass;
			gProfile.TopPrintTotal += B.TopLevelPrint;
			gProfile.ChildPrintTotal += B.ChildReprint;
			gProfile.TopPrintCalls += B.TopLevelCalls;
			gProfile.ChildPrintCalls += B.ChildCalls;
			gProfile.CursorTotal += CursorUs;
			gProfile.GdiPlusTotal += B.ToGdiPlus;

			++gProfile.Count;

			if (bSendFrame)
			{
				++gProfile.FramesSent;

				// The server says so now instead of dropping silently.
				if (Status)
				{
					++gProfile.FramesDropped;
				}
			}
		}

		// Flush on a full window of samples, or on time if frames are slow
		// enough that a full window would take too long to fill.
		if (bProfileFrames
			&& (gProfile.Count >= ProfileSamples || GetTickCount64() - gProfile.WindowStart >= 2000))
		{
			ReportFrameProfile(FrameIntervalMs);
		}

		// See FrameIntervalMs above.
		ULONGLONG FrameEnd = GetTickCount64();
		const ULONGLONG Elapsed = FrameEnd - LastFrameTick;

		if (Elapsed < FrameIntervalMs)
		{
			const DWORD Remaining = (DWORD)(FrameIntervalMs - Elapsed);

			// Negative means relative, in 100 ns units.
			LARGE_INTEGER Due;
			Due.QuadPart = -(LONGLONG)Remaining * 10000;

			if (hFrameTimer && SetWaitableTimer(hFrameTimer, &Due, 0, NULL, NULL, FALSE))
			{
				// Bounded so a timer that never fires cannot wedge the loop.
				WaitForSingleObject(hFrameTimer, Remaining + 50);
			}
			else
			{
				Sleep(Remaining);
			}

			FrameEnd = GetTickCount64();
		}
		LastFrameTick = FrameEnd;
	}

	if (hFrameTimer)
	{
		CloseHandle(hFrameTimer);
	}
}
