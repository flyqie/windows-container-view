#pragma once

#include <queue>
#include <mutex>
#include "../Interface/sandbox_h.h"

class EventManager
{
private:
	std::queue<struct SandboxEvent> m_eventQueue;
	std::mutex m_mutex;

	// Scroll not yet worth a wheel event. See EmitWheelEvent.
	float m_wheelAccumulator = 0.f;
	float m_hwheelAccumulator = 0.f;

public:
	void EmitMouseMotionEvent(int x, int y);
	void EmitMouseKeyEvent(int buttonIndex, int x, int y, bool bKeyDown);
	void EmitKeyboardEvent(int keyIndex, bool bKeyDown);

	// Delta and HDelta are in notches, positive away from the user and to the
	// right respectively.
	void EmitWheelEvent(float Delta, float HDelta, int x, int y);

	// Moves up to Max events out of the queue and returns how many were written.
	//
	// Draining in one call rather than one call per event is the point: the
	// client used to spend a round trip per event, and a round trip is a full
	// RPC call -- ~1.6 ms measured on loopback, plus the link's RTT on a real
	// network.
	unsigned PopEvents(struct SandboxEvent* Events, unsigned Max);
};

extern EventManager gEventManager;
