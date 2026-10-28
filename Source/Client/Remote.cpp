#include "Remote.h"

#include <Windows.h>
#include <gdiplus.h>
#include <ole2.h>
#include <rpc.h>

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <vector>

#include "../Common/SandboxEnv.h"
#include "../Common/SandboxLog.h"
#include "../Interface/sandbox_h.h"
#include "Desktop.h"
#include "Dock.h"
#include "FrameProfile.h"

using namespace std;

// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION is Windows 10 1803+; define it here in
// case the SDK headers predate it.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace
{
	// --- timing -------------------------------------------------------------

	// Profiling is off unless SANDBOX_DEBUG is set, and when it is off nothing
	// below reads the clock. Measured, a QueryPerformanceCounter pair plus the
	// conversion costs 31.7 ns; it is gated because there is no reason for it not
	// to be, not because it was a cost.
	const bool gProfiling = DebugEnabled();
	LARGE_INTEGER gFrequency;

	// Starts a timer, or leaves it zero when profiling is off.
	void BeginTimer(LARGE_INTEGER& Start)
	{
		if (gProfiling)
		{
			QueryPerformanceCounter(&Start);
		}
	}

	// Microseconds since BeginTimer, or 0 when profiling is off.
	ULONGLONG MicrosecondsSince(const LARGE_INTEGER& Start)
	{
		if (!gProfiling)
		{
			return 0;
		}

		LARGE_INTEGER Now;
		QueryPerformanceCounter(&Now);

		return (ULONGLONG)((Now.QuadPart - Start.QuadPart) * 1000000LL / gFrequency.QuadPart);
	}

	// --- the frame ----------------------------------------------------------

	// The previous frame's pixels, used to skip frames that did not change.
	//
	// File scope, not local: the loop contains an RpcTryExcept (__try) block, and
	// MSVC rejects __try in a function that needs object unwinding.
	std::vector<unsigned char> gPreviousFrame;

	// The container's cursor, read once per frame. File scope for the same reason.
	Desktop::Cursor gCursor;

	// Where the host's pointer last was, in frame coordinates, and whether that
	// was over the dock.
	//
	// The dock belongs to the client but the pointer belongs to the host, so this
	// is the only record of where it is. It is what lets the cursor keep following
	// the pointer over the dock without the host being told the dock exists.
	int gPointerX = 0;
	int gPointerY = 0;
	bool gPointerOnDock = false;

	// Whether the left button was pressed on the dock, so that the matching
	// release is swallowed instead of being injected into the container.
	bool gDockPress = false;

	// Compares the current frame against the previous one.
	//
	// The frame is a top-down 32bpp DIB section with no row padding, so it is one
	// contiguous block and a single memcmp covers it. Reading it through GDI+
	// LockBits, as this used to, copied the whole screen first.
	bool FrameMatches(const unsigned char* Bits, size_t Size, const std::vector<unsigned char>& Previous)
	{
		// The first frame, or a size that changed with the screen: report a
		// difference so the frame is sent rather than silently skipped forever.
		if (!Bits || Previous.size() != Size)
		{
			return false;
		}

		return memcmp(Bits, Previous.data(), Size) == 0;
	}

	void CopyFrame(const unsigned char* Bits, size_t Size, std::vector<unsigned char>& Out)
	{
		Out.assign(Bits, Bits + Size);
	}

	// --- input --------------------------------------------------------------

	// Input events fetched per frame.
	//
	// The host queues whatever arrived since the last frame, which at 30 fps is a
	// handful even during fast typing; this only bounds the reply buffer.
	const unsigned MaxEventsPerFrame = 32;

	// NDR marshals a conformant array as a [ref] pointer, so the "no frame this
	// time" case still needs a valid address even though nothing reads it.
	const unsigned char gNoFrame = 0;

	// Injects one input event, or hands it to the dock.
	//
	// Kept out of the frame loop so that loop's RpcTryExcept (__try) block stays
	// small; MSVC rejects __try in a function that also needs object unwinding.
	void DispatchEvent(const SandboxEvent& Event, int FrameWidth, int FrameHeight)
	{
		switch (Event.eventType)
		{
		case SBET_MouseEvent:
			gPointerX = Event.mouseEvent.x;
			gPointerY = Event.mouseEvent.y;
			gPointerOnDock = Dock::Contains(Event.mouseEvent.y, FrameHeight);

			// The dock is the client's own, not part of the container's desktop,
			// so anything landing on it is handled here and never reaches the
			// container. Motion included: moving the cursor down there would drag
			// it behind the dock.
			if (gPointerOnDock)
			{
				if (Event.mouseEvent.buttonIndex == 1)
				{
					if (Event.mouseEvent.bKeyDown)
					{
						gDockPress = true;

						Dock::Press(Event.mouseEvent.x, Event.mouseEvent.y, FrameWidth, FrameHeight);
					}
					else if (gDockPress)
					{
						gDockPress = false;
					}
					else
					{
						// Pressed on the desktop, released over the dock --
						// dragging a window past the bottom edge does this. The
						// container is still holding the button down, so the
						// release has to reach it or the drag never ends.
						Desktop::PointerButton(1, false);
					}
				}

				break;
			}

			Desktop::PointerMove(Event.mouseEvent.x, Event.mouseEvent.y);

			if (Event.mouseEvent.buttonIndex != -1)
			{
				Desktop::PointerButton(Event.mouseEvent.buttonIndex, Event.mouseEvent.bKeyDown != 0);
			}
			break;

		case SBET_WheelEvent:
			// Nothing in the dock scrolls, and the container's cursor has no
			// business behind it.
			if (!Dock::Contains(Event.wheelEvent.y, FrameHeight))
			{
				Desktop::Wheel(Event.wheelEvent.x, Event.wheelEvent.y,
					Event.wheelEvent.delta, Event.wheelEvent.hdelta);
			}
			break;

		case SBET_KeyEvent:
			Desktop::Key(Event.keyboardEvent.keyIndex, Event.keyboardEvent.bKeyDown != 0);
			break;
		}
	}

	// --- encoding -----------------------------------------------------------

	int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
	{
		UINT num = 0;          // number of image encoders
		UINT size = 0;         // size of the image encoder array in bytes

		Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

		Gdiplus::GetImageEncodersSize(&num, &size);

		if (size == 0)
		{
			return -1;  // Failure
		}

		pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));

		if (pImageCodecInfo == NULL)
		{
			return -1;  // Failure
		}

		Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

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
}

bool RemoteRun(const wchar_t* Host)
{
	RPC_WSTR StringBinding;
	RPC_BINDING_HANDLE BindingHandle;

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
		Debug() << "RpcStringBindingCompose failed (err " << hex << Status << ")" << endl;

		return false;
	}

	Debug() << "Getting binding..." << endl;
	Status = RpcBindingFromStringBinding(StringBinding, &BindingHandle);
	RpcStringFree(&StringBinding);

	if (Status != RPC_S_OK)
	{
		Debug() << "RpcBindingFromStringBinding failed (err " << hex << Status << ")" << endl;

		return false;
	}

	Debug() << "Done." << endl;

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Capture rate. PrintWindow is a synchronous WM_PRINT and forces a repaint of
	// every window, so an uncapped loop starves controls and burns CPU on frames
	// the host drops.
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

	// How long an unchanged screen may go without a frame being sent anyway.
	//
	// The host drops a frame silently when the previous one has not been presented
	// yet, so a frame skipped here plus one dropped there would leave the host on
	// an old image with nothing left to trigger a repaint.
	const ULONGLONG ResyncIntervalMs = 1000;

	// When the last frame was actually put on the wire.
	ULONGLONG LastSentTick = GetTickCount64();

	if (gProfiling)
	{
		QueryPerformanceFrequency(&gFrequency);
	}

	// Frame pacing. Sleep() is quantised to the 15.6 ms system tick, which made a
	// 33 ms budget come out at 41-46 ms per frame; a high-resolution waitable
	// timer waits for the interval actually asked for.
	HANDLE hFrameTimer = CreateWaitableTimerExW(
		NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

	if (!hFrameTimer)
	{
		Debug() << "High-resolution timer unavailable (err " << hex << GetLastError()
		     << "), falling back to Sleep" << endl;
	}

	// How long the desktop may be empty before the session gives up on it.
	//
	// A window is created before it is shown, so one empty sample is not proof
	// that the program is gone -- and acting on it ends the session on its first
	// frame, before anything has been drawn.
	const ULONGLONG EmptyGraceMs = 2000;

	ULONGLONG EmptySince = 0;

	bool bContinue = true;

	while (bContinue)
	{
		const ULONGLONG Now = GetTickCount64();

		if (Desktop::HasVisibleWindows())
		{
			EmptySince = 0;
		}
		else if (!EmptySince)
		{
			EmptySince = Now;
		}
		else if (Now - EmptySince >= EmptyGraceMs)
		{
			// Nothing left on the desktop to show.
			Debug() << "No more visible windows, exiting." << endl;
			bContinue = false;
		}

		// Capture, and let the dock draw itself onto the frame before it is
		// finished -- GDI clears the alpha of every pixel it touches, so the
		// dock's goes in before the alpha pass and not after.
		Desktop::Frame Screen;

		LARGE_INTEGER CaptureStart;
		BeginTimer(CaptureStart);

		const bool bCaptured = Desktop::Capture(Screen);

		ULONGLONG CaptureUs = MicrosecondsSince(CaptureStart);

		LARGE_INTEGER DockStart;
		BeginTimer(DockStart);

		if (bCaptured)
		{
			Dock::Draw(Screen.Dc, Screen.Width, Screen.Height);
		}

		const ULONGLONG DockUs = MicrosecondsSince(DockStart);

		LARGE_INTEGER FinishStart;
		BeginTimer(FinishStart);

		Gdiplus::Bitmap* Bitmap = bCaptured ? Desktop::Finish(Screen) : nullptr;

		CaptureUs += MicrosecondsSince(FinishStart);

		// The cursor is read, not painted into the frame; the host composites it.
		// The bitmap is only read when the shape changes.
		LARGE_INTEGER CursorStart;
		BeginTimer(CursorStart);

		Desktop::ReadCursor(gCursor);

		const ULONGLONG CursorUs = MicrosecondsSince(CursorStart);

		// Decide whether this frame is worth the encode/transfer/decode trip. An
		// unchanged screen is the large majority of them; see Docs/PERFORMANCE.md.
		bool bSendFrame = (Now - LastSentTick) >= ResyncIntervalMs;

		if (!bSendFrame && Bitmap)
		{
			const size_t FrameSize = (size_t)Screen.Stride * Screen.Height;

			bSendFrame = !FrameMatches(Screen.Bits, FrameSize, gPreviousFrame);

			if (bSendFrame)
			{
				CopyFrame(Screen.Bits, FrameSize, gPreviousFrame);
			}
		}

		// One frame's pixels and the input that comes back with them, in one RPC
		// call. See Source/Interface/sandbox.idl.
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
		else
		{
			// No frame, but the input still has to travel, and the hit tests
			// below need to know how big the frame is.
			FrameWidth = (unsigned long)Screen.Width;
			FrameHeight = (unsigned long)Screen.Height;
		}

		const unsigned char* FrameData = Size ? (const unsigned char*)lpGlobalMem : &gNoFrame;

		// Over the dock, the container's cursor is the wrong thing to report: it
		// was never moved there, because a dock click never reaches the container.
		// Reporting where the host's pointer actually is makes the host draw the
		// cursor there instead, and it needs to know nothing about the dock to do
		// that -- the position is in frame coordinates either way.
		const long CursorX = gPointerOnDock ? gPointerX : gCursor.X;
		const long CursorY = gPointerOnDock ? gPointerY : gCursor.Y;
		const unsigned long CursorVisible = (gPointerOnDock || gCursor.Visible) ? 1 : 0;

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
				CursorX,
				CursorY,
				CursorVisible,
				&EventCount,
				Events
			);

			// The shape changes a few times a minute, so it does not get a slot in
			// every frame.
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
				DispatchEvent(Events[i], (int)FrameWidth, (int)FrameHeight);
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
		// sampled too: they still pay for a full capture, and capture is the cost
		// under investigation.
		FrameProfile::Frame Sample;
		Sample.Capture = CaptureUs;
		Sample.Dock = DockUs;
		Sample.Cursor = CursorUs;
		Sample.Encode = EncodeUs;
		Sample.Send = SendUs;
		Sample.Sent = bSendFrame;
		Sample.Dropped = Status != 0;

		FrameProfile::Add(Sample);
		FrameProfile::Flush(FrameIntervalMs);

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

	return true;
}
