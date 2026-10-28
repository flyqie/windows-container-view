#include "FrameProfile.h"

#include <Windows.h>

#include <algorithm>
#include <iostream>
#include <vector>

#include "../Common/SandboxLog.h"
#include "Desktop.h"

using namespace std;

namespace
{
	// How many frames a report covers before it is flushed on sample count alone.
	const unsigned Samples = 128;

	// The profiler is off unless SANDBOX_DEBUG is set. It reads no clock itself --
	// the loop does the timing -- but the report is worth nothing if it is only
	// ever printed into a discarded stream.
	const bool gProfiling = DebugEnabled();

	struct Profile
	{
		ULONGLONG Capture[Samples];
		ULONGLONG Dock[Samples];
		ULONGLONG Cursor[Samples];
		ULONGLONG Encode[Samples];
		ULONGLONG Send[Samples];
		unsigned  Calls[Samples];   // PrintWindow calls made by that frame

		ULONGLONG CaptureTotal;
		ULONGLONG DockTotal;
		ULONGLONG CursorTotal;
		ULONGLONG EncodeTotal;
		ULONGLONG SendTotal;

		// The capture's own breakdown, accumulated so the report can show means.
		// See Desktop::CaptureBreakdown.
		ULONGLONG AllocTotal;
		ULONGLONG FillsTotal;
		ULONGLONG PassTotal;
		ULONGLONG TopPrintTotal;
		ULONGLONG ChildPrintTotal;
		unsigned  TopPrintCalls;
		unsigned  ChildPrintCalls;
		ULONGLONG GdiPlusTotal;

		unsigned  Count;
		unsigned  FramesSent;
		unsigned  FramesDropped;   // the host refused: the previous one is not presented
		ULONGLONG WindowStart;
	};

	Profile gProfile;

	ULONGLONG Percentile(const ULONGLONG* Values, unsigned Count, unsigned Pct)
	{
		if (!Count)
		{
			return 0;
		}

		std::vector<ULONGLONG> Sorted(Values, Values + Count);
		std::sort(Sorted.begin(), Sorted.end());

		return Sorted[(size_t)Pct * (Count - 1) / 100];
	}

	ULONGLONG MaxOf(const ULONGLONG* Values, unsigned Count)
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

	unsigned MaxOf(const unsigned* Values, unsigned Count)
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

	void Line(const char* Name, const ULONGLONG* Values, ULONGLONG Total, unsigned N)
	{
		Debug() << "  " << Name << " us   mean " << (Total / N)
		     << "  p50 " << Percentile(Values, N, 50)
		     << "  p95 " << Percentile(Values, N, 95)
		     << "  max " << MaxOf(Values, N) << endl;
	}

	void Report(DWORD FrameIntervalMs)
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

		Line("capture", gProfile.Capture, gProfile.CaptureTotal, N);
		Line("dock   ", gProfile.Dock, gProfile.DockTotal, N);
		Line("cursor ", gProfile.Cursor, gProfile.CursorTotal, N);
		Line("encode ", gProfile.Encode, gProfile.EncodeTotal, N);
		Line("send   ", gProfile.Send, gProfile.SendTotal, N);

		Debug() << "  PrintWindow calls per frame   max " << MaxOf(gProfile.Calls, N) << endl;

		// The part worth optimising: it does not shrink with better encoding.
		Debug() << "  capture split  us   alloc " << (gProfile.AllocTotal / N)
		     << "  fills " << (gProfile.FillsTotal / N)
		     << "  windows " << (gProfile.PassTotal / N)
		     << "  toGdi+ " << (gProfile.GdiPlusTotal / N) << endl;

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
		// fit. Counted over every phase, not capture alone.
		const ULONGLONG BudgetUs = (ULONGLONG)FrameIntervalMs * 1000;

		if (BudgetUs)
		{
			ULONGLONG WorstTotal = 0;
			unsigned WorstIndex = 0;

			for (unsigned i = 0; i < N; ++i)
			{
				const ULONGLONG Total = gProfile.Capture[i] + gProfile.Dock[i]
					+ gProfile.Encode[i] + gProfile.Send[i];

				if (Total > WorstTotal)
				{
					WorstTotal = Total;
					WorstIndex = i;
				}
			}

			const ULONGLONG MeanTotal = (gProfile.CaptureTotal + gProfile.DockTotal
				+ gProfile.EncodeTotal + gProfile.SendTotal) / N;

			if (MeanTotal)
			{
				Debug() << "  mean frame " << MeanTotal << " us = " << (MeanTotal * 100 / BudgetUs)
				     << "% of the " << FrameIntervalMs << " ms budget  =>  ~"
				     << (1000ULL * FrameIntervalMs / MeanTotal) << " containers per core" << endl;
			}

			if (WorstTotal)
			{
				Debug() << "  worst frame " << WorstTotal << " us (capture " << gProfile.Capture[WorstIndex]
				     << " + dock " << gProfile.Dock[WorstIndex]
				     << " + encode " << gProfile.Encode[WorstIndex]
				     << " + send " << gProfile.Send[WorstIndex] << ") = "
				     << (WorstTotal * 100 / BudgetUs) << "% of budget  =>  ~"
				     << (1000ULL * FrameIntervalMs / WorstTotal) << " containers per core" << endl;
			}
		}

		gProfile.Count = 0;
		gProfile.FramesSent = 0;
		gProfile.FramesDropped = 0;
		gProfile.CaptureTotal = 0;
		gProfile.DockTotal = 0;
		gProfile.CursorTotal = 0;
		gProfile.EncodeTotal = 0;
		gProfile.SendTotal = 0;
		gProfile.AllocTotal = 0;
		gProfile.FillsTotal = 0;
		gProfile.PassTotal = 0;
		gProfile.TopPrintTotal = 0;
		gProfile.ChildPrintTotal = 0;
		gProfile.TopPrintCalls = 0;
		gProfile.ChildPrintCalls = 0;
		gProfile.GdiPlusTotal = 0;
		gProfile.WindowStart = GetTickCount64();
	}
}

void FrameProfile::Add(const Frame& Sample)
{
	if (!gProfiling)
	{
		return;
	}

	// The report is flushed on either a full window of samples or a time limit,
	// so the first frame of a window starts the clock.
	if (!gProfile.Count)
	{
		gProfile.WindowStart = GetTickCount64();
	}

	if (gProfile.Count >= Samples)
	{
		return;
	}

	const unsigned i = gProfile.Count;

	gProfile.Capture[i] = Sample.Capture;
	gProfile.Dock[i] = Sample.Dock;
	gProfile.Cursor[i] = Sample.Cursor;
	gProfile.Encode[i] = Sample.Encode;
	gProfile.Send[i] = Sample.Send;
	gProfile.Calls[i] = Desktop::LastPrintWindowCalls();

	gProfile.CaptureTotal += Sample.Capture;
	gProfile.DockTotal += Sample.Dock;
	gProfile.CursorTotal += Sample.Cursor;
	gProfile.EncodeTotal += Sample.Encode;
	gProfile.SendTotal += Sample.Send;

	const Desktop::CaptureBreakdown& B = Desktop::LastCaptureBreakdown();
	gProfile.AllocTotal += B.BitmapAlloc;
	gProfile.FillsTotal += B.Fills;
	gProfile.PassTotal += B.WindowPass;
	gProfile.TopPrintTotal += B.TopLevelPrint;
	gProfile.ChildPrintTotal += B.ChildReprint;
	gProfile.TopPrintCalls += B.TopLevelCalls;
	gProfile.ChildPrintCalls += B.ChildCalls;
	gProfile.GdiPlusTotal += B.ToGdiPlus;

	++gProfile.Count;

	if (Sample.Sent)
	{
		++gProfile.FramesSent;

		if (Sample.Dropped)
		{
			++gProfile.FramesDropped;
		}
	}
}

void FrameProfile::Flush(DWORD FrameIntervalMs)
{
	if (!gProfiling)
	{
		return;
	}

	// On a full window of samples, or on time if frames are slow enough that a
	// full window would take too long to fill.
	if (gProfile.Count >= Samples
		|| (gProfile.Count && GetTickCount64() - gProfile.WindowStart >= 2000))
	{
		Report(FrameIntervalMs);
	}
}
