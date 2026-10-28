#include <Windows.h>
#include <shellapi.h>

#include <string>

#include "Desktop.h"
#include "Dock.h"
#include "Remote.h"
#include "../Common/SandboxEnv.h"
#include "../Common/SandboxLog.h"

// The container side.
//
//     Sandbox-Client.exe [command line]
//
// The optional command line is what gets started on the container's desktop;
// with none, cmd.exe is started. The host to connect to comes from SANDBOX_HOST
// and the port from SANDBOX_PORT. Neither is a command-line argument, because
// the command line belongs to the program being launched -- this is a launcher,
// and what it launches is the only thing worth varying per deployment.
//
// Three parts, brought up in order. The desktop first, then whatever belongs on
// top of it, then the program -- which starts last so that it opens at the right
// size and knows how much room it has. Only then does anything talk to the host.
// See Desktop.h, Dock.h and Remote.h.

namespace
{
	// Re-quotes one argument so it survives the trip through CreateProcess.
	std::wstring Quote(const std::wstring& Argument)
	{
		if (Argument.find_first_of(L" \t") == std::wstring::npos)
		{
			return Argument;
		}

		std::wstring Quoted = L"\"";

		for (size_t i = 0; i < Argument.size(); ++i)
		{
			if (Argument[i] == L'"')
			{
				Quoted += L'\\';
			}

			Quoted += Argument[i];
		}

		return Quoted + L"\"";
	}
}

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	int ArgumentCount = 0;
	LPWSTR* Arguments = CommandLineToArgvW(GetCommandLineW(), &ArgumentCount);

	// Everything after the program name is the command line to run on the
	// container's desktop. CommandLineToArgvW has already split and unquoted it,
	// so it is put back together the way CreateProcess wants it.
	std::wstring Command;

	for (int i = 1; Arguments && i < ArgumentCount; ++i)
	{
		if (!Command.empty())
		{
			Command += L' ';
		}

		Command += Quote(Arguments[i]);
	}

	if (Arguments)
	{
		LocalFree(Arguments);
	}

	const wchar_t* Host = SandboxHost();

	if (!Host || !*Host)
	{
		// The one failure a caller can fix, and the exit code is how they find
		// out -- not a printed line they may never see.
		Debug() << "SANDBOX_HOST is not set" << std::endl;

		return 2;
	}

	// Nothing below throws: every failure that a caller can act on comes back as
	// a return value, which is what the exit code is made of.
	if (!Desktop::Open())
	{
		return 1;
	}

	// Before the program starts, so it is born knowing how much room it has.
	Dock::Open();

	Desktop::StartProgram(Command.c_str());

	if (!RemoteRun(Host))
	{
		return 1;
	}

	return 0;
}
