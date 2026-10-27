#pragma once

// RPC endpoint (TCP port). Client and server must use the same value.
//
// The original implementation used RpcServerUseProtseq + RpcEpRegister, which
// registers a dynamic endpoint; clients then had to query the host's endpoint
// mapper on TCP 135 to discover the actual port. With a fixed endpoint:
//   - clients no longer need the endpoint mapper, so TCP 135 stays closed
//   - the firewall needs a single inbound rule for this one port
//   - the port is known ahead of time, which a service (no interactive
//     session, no firewall prompt) requires
//
// Changing this value requires rebuilding both Sandbox-Client.dll and
// Sandbox-Server.dll.
#define SANDBOX_RPC_ENDPOINT L"5001"
