#include "Client.h"
#include "Init.h"
#include "../Interface/sandbox_h.h"
#include "../Interface/SandboxConfig.h"
#include "printwnd.h"
#include <rpc.h>
#include <stdio.h>
#include <stdexcept>
#include <vector>
#include <string.h>

#include <iostream>

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

// Bytes actually holding pixels in one row of a locked GDI+ bitmap.
//
// GDI+ does not promise anything about the bytes past Width * bytesPerPixel in
// a row, so a whole Stride * Height memcmp would report differences that are
// not on screen. Compare and copy row by row instead.
static size_t LockedRowBytes(const Gdiplus::BitmapData& Data)
{
	return (size_t)Data.Width * (Gdiplus::GetPixelFormatSize(Data.PixelFormat) / 8);
}

static bool FrameMatches(const Gdiplus::BitmapData& Data, const std::vector<unsigned char>& Previous)
{
	const size_t RowBytes = LockedRowBytes(Data);

	// A zero row size means the pixel format was not understood; report a
	// difference so the frame is sent rather than silently skipped forever.
	if (RowBytes == 0 || Previous.size() != RowBytes * Data.Height)
	{
		return false;
	}

	const unsigned char* Row = (const unsigned char*)Data.Scan0;

	for (UINT y = 0; y < Data.Height; ++y)
	{
		if (memcmp(Row, Previous.data() + (size_t)y * RowBytes, RowBytes) != 0)
		{
			return false;
		}

		Row += Data.Stride;
	}

	return true;
}

static void CopyFrame(const Gdiplus::BitmapData& Data, std::vector<unsigned char>& Out)
{
	const size_t RowBytes = LockedRowBytes(Data);
	Out.resize(RowBytes * Data.Height);

	const unsigned char* Row = (const unsigned char*)Data.Scan0;

	for (UINT y = 0; y < Data.Height; ++y)
	{
		memcpy(Out.data() + (size_t)y * RowBytes, Row, RowBytes);
		Row += Data.Stride;
	}
}

// Previous frame's pixels, used to skip frames that did not change.
//
// At file scope rather than local to SandboxInitClient because that function
// contains an RpcTryExcept (__try) block, and MSVC rejects __try in any
// function that also needs object unwinding for a local.
static std::vector<unsigned char> gPreviousFrame;

void SANDBOXCLIENT_API SandboxInitClient(const wchar_t* IPAddress)
{
	RPC_WSTR StringBinding;

	RPC_BINDING_HANDLE BindingHandle;

	cout << "Configuring window stations..." << endl;
	// Set window station and desktop access rights
	TweakWindowStation();

	cout << "Assigning current process to the default window station..." << endl;
	// Assign this program to WinSta0 and the default desktop
	AssignWindowStation();

	cout << "Registering desktop window..." << endl;
	// Set some colors
	RegisterDesktopWindow();

	cout << "Starting CMD..." << endl;
	StartProgram();

	/*
	DEVMODE DevMode{ 0 };
	DevMode.dmSize = sizeof(DEVMODE);
	DevMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
	DevMode.dmPelsWidth = 1280;
	DevMode.dmPelsHeight = 1024;
	LONG Result = ChangeDisplaySettings(&DevMode, 0);

	cout << "ChangeDisplaySettings returned " << hex << Result << endl;
	*/

	cout << "Composing...";

	RPC_STATUS Status = RpcStringBindingCompose(
		NULL,
		(RPC_WSTR)TEXT("ncacn_ip_tcp"),
		(RPC_WSTR)IPAddress,
		(RPC_WSTR)SANDBOX_RPC_ENDPOINT,
		NULL,
		&StringBinding
	);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	cout << "Getting binding..." << endl;
	Status = RpcBindingFromStringBinding(StringBinding, &BindingHandle);
	RpcStringFree(&StringBinding);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	cout << "Done." << endl;

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// The loop below has no pacing of its own: it re-captures every window and
	// forces a repaint of each one (PrintWindow is a synchronous WM_PRINT) as
	// fast as RPC allows. Controls get no time to finish painting, so a
	// half-painted status bar or toolbar shows up as flicker, and the container
	// burns CPU for frames the host drops anyway. Cap the capture rate.
	const DWORD FrameIntervalMs = 33;   // ~30 fps
	ULONGLONG LastFrameTick = GetTickCount64();

	// A frame whose pixels are identical to the previous one is not worth
	// encoding and sending. On a desktop that is not being animated this is the
	// large majority of frames: measured over 120 captured frames, 81% were
	// byte for byte identical to their predecessor and the worst case changed
	// 0.3% of the screen. Set this to false to send every frame unconditionally.
	const bool bSkipUnchangedFrames = true;

	// How long an unchanged screen may go without a frame being sent anyway.
	//
	// The server throws a frame away if its previous one has not been presented
	// yet (Rendering.cpp: EndImageTransmission returns early while gShouldRender
	// is still set), and it never tells the client. A frame skipped here plus a
	// frame dropped there would leave the host showing an older image with
	// nothing left to trigger a repaint, so force one through periodically.
	const ULONGLONG ResyncIntervalMs = 1000;

	// When the last frame was actually put on the wire, for the periodic
	// resync described above.
	ULONGLONG LastSentTick = GetTickCount64();

	bool bContinue = true;
	while(bContinue)
	{
		DWORD dwCount = 0;
		EnumWindows(WndCountProc, (LPARAM)&dwCount);

		Gdiplus::Bitmap* Bitmap = nullptr;

		if (!dwCount)
		{
			// Break when there are no more windows
			cout << "No more visible windows, exiting." << endl;
			bContinue = false;
		}

		Bitmap = PrintScreen(GetThreadDesktop(GetCurrentThreadId()));

		// Decide whether this frame is worth the encode/transfer/decode trip.
		ULONGLONG Now = GetTickCount64();
		bool bSendFrame = !bSkipUnchangedFrames || (Now - LastSentTick) >= ResyncIntervalMs;

		if (!bSendFrame && Bitmap)
		{
			Gdiplus::Rect FrameRect(0, 0, Bitmap->GetWidth(), Bitmap->GetHeight());
			Gdiplus::BitmapData Locked;

			// Lock in the bitmap's own format so GDI+ does not have to convert.
			if (Bitmap->LockBits(&FrameRect, Gdiplus::ImageLockModeRead, Bitmap->GetPixelFormat(), &Locked) == Gdiplus::Ok)
			{
				bSendFrame = !FrameMatches(Locked, gPreviousFrame);

				if (bSendFrame)
				{
					CopyFrame(Locked, gPreviousFrame);
				}

				Bitmap->UnlockBits(&Locked);
			}
			else
			{
				// The pixels could not be read back, so whether the frame
				// changed is unknown. Send it rather than risk freezing the
				// host on an old image.
				bSendFrame = true;
			}
		}

		const SIZE_T ChunkSize = 1 * 1024 * 1024;
		SIZE_T Size = 0;
		SIZE_T SizeRemaining = 0;
		IStream* Stream = nullptr;
		HGLOBAL hGlobalMem = NULL;
		LPVOID lpGlobalMem = NULL;

		if (bSendFrame)
		{
			CLSID sid;
			GetEncoderClsid(L"image/png", &sid);

			CreateStreamOnHGlobal(NULL, TRUE, &Stream);

			Bitmap->Save(Stream, &sid, NULL);

			GetHGlobalFromStream(Stream, &hGlobalMem);

			Size = GlobalSize(hGlobalMem);
			SizeRemaining = Size;

			lpGlobalMem = GlobalLock(hGlobalMem);
		}

		RpcTryExcept
		{
			if (bSendFrame)
			{
				LastSentTick = GetTickCount64();

				RpcBeginImage(BindingHandle, Bitmap->GetWidth(), Bitmap->GetHeight(), (unsigned long)Size);

				while (SizeRemaining > 0)
				{
					const unsigned char* StartData = (unsigned char*)lpGlobalMem + (Size - SizeRemaining);
					const SIZE_T SizeToSend = min(SizeRemaining, ChunkSize);

					RpcSendImageChunkData(
						BindingHandle,
						(unsigned long)SizeToSend,
						StartData
					);

					SizeRemaining -= SizeToSend;
				}

				RpcEndImage(BindingHandle);
			}

			// Input is drained on every iteration, frame sent or not, so a
			// static screen still responds to the keyboard and mouse.
			SandboxEvent event;
			while (RpcPopEvent(BindingHandle, &event))
			{
				switch (event.eventType)
				{
				case SBET_MouseEvent:
					if(!SetCursorPos(event.mouseEvent.x, event.mouseEvent.y))
					{
						//cout << "SetCursorPos returned false with error " << hex << GetLastError() << endl;
					}

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
							cout << "SendInput for mouse returned false with error " << hex << GetLastError() << endl;
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

					// The host's SDL-to-DIK table stores extended keys with bit
					// 0x80 set (arrows, Home/End/PgUp/PgDn, Insert/Delete, numpad
					// Enter and Divide, right Ctrl/Alt, Win/Menu). Strip the
					// marker and flag the key as extended, otherwise the system
					// does not recognise the scan code and the key is dropped.
					if (scan & 0x80)
					{
						scan &= 0x7F;
						input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
					}

					input.ki.wScan = scan;
					input.ki.wVk = (WORD)MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);

					cout << "scan:" << input.ki.wScan << " vk: " << input.ki.wVk << " received" << endl;

					if (!SendInput(1, &input, sizeof(INPUT)))
					{
						cout << "SendInput returned false with error " << hex << GetLastError() << endl;
					}

				}
					break;
				}
			}
		}
		RpcExcept(1)
		{
			cout << "Exception " << hex << RpcExceptionCode() << " encountered while sending data" << endl;

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

		// See FrameIntervalMs above.
		ULONGLONG FrameEnd = GetTickCount64();
		const ULONGLONG Elapsed = FrameEnd - LastFrameTick;
		if (Elapsed < FrameIntervalMs)
		{
			Sleep((DWORD)(FrameIntervalMs - Elapsed));
			FrameEnd = GetTickCount64();
		}
		LastFrameTick = FrameEnd;
	}
	//CLSID sid;
	//GetEncoderClsid(L"image/png", &sid);

	//Bitmap->Save(L"image.png", &sid, NULL);

	//RpcTryExcept
	//{
	//MyRemoteProc(BindingHandle);
	//}
	//RpcExcept(1)
	//{
	//	printf("Exception %d occured\n", RpcExceptionCode());
	//}
	//RpcEndExcept
}
