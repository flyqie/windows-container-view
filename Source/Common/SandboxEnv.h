#pragma once

// Configuration that differs between deployments. Both are optional; with
// nothing set, the defaults on the two sides match.
//
// Returned as pointers to process-lifetime storage rather than std::wstring by
// value, because a returned temporary needs object unwinding and MSVC rejects
// that in callers that also use RpcTryExcept.
//
// See Docs/BUILD.md, "Port and address".

// RPC endpoint (TCP port). SANDBOX_PORT overrides the compiled default.
const wchar_t* SandboxPort();

// Host to connect to. SANDBOX_HOST; the client needs it, the server ignores it.
const wchar_t* SandboxHost();

// Requested display mode, from SANDBOX_RESOLUTION as "WIDTHxHEIGHT". Returns
// false when it is unset or malformed, in which case the container's default
// mode is left alone.
//
// The container has no display adapter, so win32k falls back to a stub driver
// that advertises a fixed set of 18 modes. A value outside that set is rejected
// and logged; see Docs/IMAGE-ANALYSIS.md.
bool SandboxResolution(int& Width, int& Height);

// Whether the client draws its dock below the desktop. SANDBOX_DOCK; off unless
// it is set. See Source/Client/Dock.h.
bool SandboxDock();

// A boolean environment variable: "1", "y" or "t" in either case is true, and
// anything else -- including unset -- is false.
bool SandboxFlag(const char* Name);
