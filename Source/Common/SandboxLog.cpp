#include "SandboxLog.h"

#include <Windows.h>

#include <iostream>
#include <streambuf>

namespace
{
	// Throws away everything written to it, so the caller's stream expression
	// costs a few virtual calls and no formatting.
	class NullBuffer : public std::streambuf
	{
	protected:
		int overflow(int c) override
		{
			return c;
		}
	};

	class NullWBuffer : public std::wstreambuf
	{
	protected:
		wint_t overflow(wint_t c) override
		{
			return c;
		}
	};

	class NullStream : public std::ostream
	{
	public:
		NullStream() : std::ostream(&Buffer) {}

	private:
		NullBuffer Buffer;
	};

	class NullWStream : public std::wostream
	{
	public:
		NullWStream() : std::wostream(&Buffer) {}

	private:
		NullWBuffer Buffer;
	};

	bool ReadFlag(const char* Name)
	{
		char Value[8] = { 0 };

		if (!GetEnvironmentVariableA(Name, Value, sizeof(Value)))
		{
			return false;
		}

		return Value[0] == '1' || Value[0] == 'y' || Value[0] == 'Y'
			|| Value[0] == 't' || Value[0] == 'T';
	}
}

bool DebugEnabled()
{
	// Read once. The environment cannot change under a running process, and the
	// frame profiler calls this every frame.
	static const bool Enabled = ReadFlag("SANDBOX_DEBUG");

	return Enabled;
}

std::ostream& Debug()
{
	static NullStream Null;

	return DebugEnabled() ? std::cout : Null;
}

std::wostream& DebugW()
{
	static NullWStream Null;

	return DebugEnabled() ? std::wcout : Null;
}
