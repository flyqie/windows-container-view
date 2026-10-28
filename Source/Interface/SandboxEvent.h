#pragma once

enum SandboxEventType
{
	SBET_KeyEvent,
	SBET_MouseEvent,
	SBET_WheelEvent
};

struct SandboxMouseEvent
{
	int buttonIndex;
	int bKeyDown;
	int x;
	int y;
};

struct SandboxKeyboardEvent
{
	int keyIndex;
	int bKeyDown;
};

// One notch is WHEEL_DELTA (120) on both axes, which is the unit Windows
// applications are written against.
//
// delta is vertical -- positive away from the user -- and hdelta horizontal,
// positive to the right. Either may be zero. x and y are the cursor position in
// container pixels at the time of the scroll.
struct SandboxWheelEvent
{
	int delta;
	int hdelta;
	int x;
	int y;
};

struct SandboxEvent
{
	int eventType;

	[switch_is(eventType)] union
	{
		[case(0)] struct SandboxKeyboardEvent keyboardEvent;
		[case(1)] struct SandboxMouseEvent mouseEvent;
		[case(2)] struct SandboxWheelEvent wheelEvent;
		[default] ;
	};
};
