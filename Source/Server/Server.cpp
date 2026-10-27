#include "Server.h"
#include "ServerPrivate.h"
#include <rpc.h>
#include "../Interface/sandbox_h.h"
#include "../Interface/SandboxConfig.h"

void SandboxInitServer()
{
	RPC_STATUS Status;

	// Fixed endpoint instead of a dynamic one. The client names the same port
	// in its string binding and connects directly, so no endpoint mapper
	// lookup (TCP 135) is needed.
	Status = RpcServerUseProtseqEp(
		(RPC_WSTR)TEXT("ncacn_ip_tcp"),
		RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
		(RPC_WSTR)SANDBOX_RPC_ENDPOINT,
		NULL
	);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	Status = RpcServerRegisterIfEx(
		Sandbox_v1_0_s_ifspec,
		NULL,
		NULL,
		RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH,
		10,
		NULL
	);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}

	// RpcServerInqBindings + RpcEpRegister used to live here, publishing the
	// dynamic endpoint to the endpoint mapper. With a fixed endpoint the
	// client connects directly and neither call is needed, so the server no
	// longer touches TCP 135.

	Status = RpcServerListen(
		1,
		10,
		1
	);

	if (Status != RPC_S_OK)
	{
		throw Status;
	}
}

void SANDBOXSERVER_API SandboxInitVideo()
{
	InitVideo();
}

/*
void SANDBOXSERVER_API SandboxSetBeginImageTransmission(OnBeginImageTransmission Function)
{
	BeginImageTransmission = Function;
}

void SANDBOXSERVER_API SandboxSetReceivedImageData(OnReceivedImageData Function)
{
	ReceivedImageData = Function;
}

void SANDBOXSERVER_API SandboxSetEndImageTransmission(OnEndImageTransmission Function)
{
	EndImageTransmission = Function;
}
*/
