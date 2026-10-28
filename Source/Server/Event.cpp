#include <Windows.h>

#include "Event.h"
#include "../Interface/sandbox_h.h"

EventManager gEventManager;

void EventManager::EmitMouseMotionEvent(int x, int y)
{
	SandboxEvent event;
	event.eventType = SBET_MouseEvent;
	event.mouseEvent.bKeyDown = false;
	event.mouseEvent.buttonIndex = -1;
	event.mouseEvent.x = x;
	event.mouseEvent.y = y;
	std::lock_guard<std::mutex> lock(m_mutex);
	m_eventQueue.push(event);
}

void EventManager::EmitMouseKeyEvent(int buttonIndex, int x, int y, bool bKeyDown)
{
	SandboxEvent event;
	event.eventType = SBET_MouseEvent;
	event.mouseEvent.bKeyDown = bKeyDown;
	event.mouseEvent.buttonIndex = buttonIndex;
	event.mouseEvent.x = x;
	event.mouseEvent.y = y;
	std::lock_guard<std::mutex> lock(m_mutex);
	m_eventQueue.push(event);
}

void EventManager::EmitKeyboardEvent(int keyIndex, bool bKeyDown)
{
	SandboxEvent event;
	event.eventType = SBET_KeyEvent;
	event.keyboardEvent.bKeyDown = bKeyDown;
	event.keyboardEvent.keyIndex = keyIndex;
	std::lock_guard<std::mutex> lock(m_mutex);
	m_eventQueue.push(event);
}

void EventManager::EmitWheelEvent(float Delta, float HDelta, int x, int y)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// SDL reports the scroll as a float. A mouse's wheel gives whole notches,
	// but a precision touchpad gives a stream of fractions that would each
	// truncate to zero on their own, so the remainder is kept until it adds up.
	m_wheelAccumulator += Delta;
	m_hwheelAccumulator += HDelta;

	const int Steps = (int)m_wheelAccumulator;
	const int HSteps = (int)m_hwheelAccumulator;

	if (!Steps && !HSteps)
	{
		return;
	}

	m_wheelAccumulator -= (float)Steps;
	m_hwheelAccumulator -= (float)HSteps;

	SandboxEvent event;
	event.eventType = SBET_WheelEvent;
	event.wheelEvent.delta = Steps * WHEEL_DELTA;
	event.wheelEvent.hdelta = HSteps * WHEEL_DELTA;
	event.wheelEvent.x = x;
	event.wheelEvent.y = y;
	m_eventQueue.push(event);
}

unsigned EventManager::PopEvents(struct SandboxEvent* Events, unsigned Max)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	unsigned Count = 0;

	while (Count < Max && !m_eventQueue.empty())
	{
		Events[Count++] = m_eventQueue.front();
		m_eventQueue.pop();
	}

	return Count;
}
