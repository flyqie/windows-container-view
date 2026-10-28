#include <Windows.h>
#include "Server.h"
#include "ServerPrivate.h"
#include "Event.h"

// The single entry point for everything the container sends: one frame (or
// none), the cursor's position, and the input accumulated since the last call.
//
// Return value: 0 if the frame was accepted for presentation, 1 if it was
// dropped because the previous one has not been presented yet.
extern "C" unsigned long RpcSendFrame(
	handle_t IDL_handle,
	unsigned long Width,
	unsigned long Height,
	unsigned long Size,
	const unsigned char* BitmapData,
	long CursorX,
	long CursorY,
	unsigned long CursorVisible,
	unsigned long* EventCount,
	struct SandboxEvent* Events
)
{
	unsigned long Status = 0;

	// The cursor keeps tracking even when the frame is dropped: a dropped frame
	// means "the host has not presented the last one yet", not "the mouse
	// stopped moving".
	SetCursorState(CursorX, CursorY, CursorVisible != 0);

	// Size == 0 is the "no frame this time" case: the caller still wants its
	// input, which is what keeps a static screen responsive.
	if (Size)
	{
		Status = ReceiveFrame(Width, Height, BitmapData, Size) ? 0 : 1;
	}

	// EventCount arrives as the caller's buffer capacity and leaves as the
	// number actually written.
	*EventCount = gEventManager.PopEvents(Events, *EventCount);

	return Status;
}

// The cursor bitmap, which the client only sends when the shape changes.
extern "C" void RpcSetCursorShape(
	handle_t IDL_handle,
	unsigned long Width,
	unsigned long Height,
	unsigned long HotX,
	unsigned long HotY,
	unsigned long Size,
	const unsigned char* Pixels
)
{
	SetCursorShape(Width, Height, HotX, HotY, Pixels, Size);
}
