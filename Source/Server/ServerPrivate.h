#pragma once

// Decodes one PNG frame and hands it to the renderer. Returns false when the
// frame was dropped because the previous one has not been presented yet.
bool ReceiveFrame(unsigned long Width, unsigned long Height, const unsigned char* Data, unsigned long Size);

// Cursor position and visibility, which change every frame. Kept out of
// ReceiveFrame so the cursor keeps tracking even when a frame is dropped.
void SetCursorState(long X, long Y, bool Visible);

// The cursor bitmap, which changes a few times a minute. The host composites
// the cursor at presentation time rather than having it painted into the frame.
//
// Pixels is 32bpp BGRA, top-down, straight alpha.
void SetCursorShape(unsigned long Width, unsigned long Height, unsigned long HotX,
	unsigned long HotY, const unsigned char* Pixels, unsigned long Size);

void InitVideo();
