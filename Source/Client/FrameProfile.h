#pragma once

#include <Windows.h>

// The per-second frame profile, printed when SANDBOX_DEBUG is set.
//
// Part of the remote display's toolkit rather than a part of its own: it measures
// the frame loop, and nothing else uses it. What the numbers mean, and what they
// were measured at, is in Docs/PERFORMANCE.md.
//
// Every entry point returns immediately when debugging is off, so the loop pays
// nothing for a profiler that is switched off.
namespace FrameProfile
{
	// One frame's numbers, in microseconds, and all zero when profiling is off.
	//
	// Only what the loop itself knows: the capture's own breakdown is read from
	// Desktop, which is the only thing that can measure it.
	struct Frame
	{
		ULONGLONG Capture;      // Desktop::Capture and Desktop::Finish together
		ULONGLONG Dock;         // Dock::Draw
		ULONGLONG Cursor;       // Desktop::ReadCursor
		ULONGLONG Encode;       // PNG
		ULONGLONG Send;         // the RpcSendFrame round trip
		bool      Sent;         // not skipped as unchanged
		bool      Dropped;      // the host refused it: the last one is not presented
	};

	void Add(const Frame& Sample);

	// Prints and resets, if a full window of samples has built up or two seconds
	// have gone by.
	void Flush(DWORD FrameIntervalMs);
}
