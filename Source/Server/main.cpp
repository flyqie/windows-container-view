#include <Windows.h>

#include "Server.h"
#include "../Common/SandboxLog.h"

// The host side: listens on SANDBOX_PORT and shows whatever the container
// sends. Takes no arguments.
//
// Needs a desktop session -- the display is an SDL window -- so it cannot run
// on Server Core.
int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	Debug() << "Sandbox-Server" << std::endl;

	try
	{
		SandboxInitServer();
		SandboxInitVideo();
	}
	catch (long ErrorCode)
	{
		Debug() << "Failed to init server (error " << ErrorCode << ")" << std::endl;

		return 1;
	}

	return 0;
}
