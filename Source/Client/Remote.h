#pragma once

// Part three of the client: the remote display.
//
// Carries the desktop to the host and the host's input back -- the x11vnc half,
// as opposed to the desktop itself (Desktop.h) and the strip of window buttons
// along its bottom (Dock.h).
//
// It is the only part that knows about RPC, about PNG, or that a host exists at
// all. It runs until the container's windows are gone or the host goes away, and
// returns false if the host could not be reached in the first place.
bool RemoteRun(const wchar_t* Host);
