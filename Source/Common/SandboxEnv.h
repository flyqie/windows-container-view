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
