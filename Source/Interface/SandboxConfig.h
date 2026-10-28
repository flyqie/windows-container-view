#pragma once

// Default RPC endpoint (TCP port). Overridden at runtime by SANDBOX_PORT; see
// Source/Common/SandboxEnv.h.
//
// A fixed endpoint rather than a dynamic one, so the client connects directly
// and the host's endpoint mapper (TCP 135) is never involved. See
// Docs/BUILD.md, "Why a fixed port".
#define SANDBOX_RPC_ENDPOINT L"5001"
