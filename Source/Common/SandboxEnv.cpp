#include "SandboxEnv.h"

#include <Windows.h>

#include <string>

#include "../Interface/SandboxConfig.h"

namespace
{
	std::wstring Read(const wchar_t* Name)
	{
		// GetEnvironmentVariableW returns 0 both for "not set" and for "set to
		// empty", which are the same thing here.
		const DWORD Length = GetEnvironmentVariableW(Name, NULL, 0);

		if (Length == 0)
		{
			return std::wstring();
		}

		std::wstring Value(Length, L'\0');

		const DWORD Written = GetEnvironmentVariableW(Name, &Value[0], Length);

		if (Written == 0 || Written >= Length)
		{
			return std::wstring();
		}

		Value.resize(Written);

		return Value;
	}
}

const wchar_t* SandboxPort()
{
	static const std::wstring Value = []
	{
		const std::wstring FromEnv = Read(L"SANDBOX_PORT");

		return FromEnv.empty() ? std::wstring(SANDBOX_RPC_ENDPOINT) : FromEnv;
	}();

	return Value.c_str();
}

const wchar_t* SandboxHost()
{
	static const std::wstring Value = Read(L"SANDBOX_HOST");

	return Value.c_str();
}

bool SandboxResolution(int& Width, int& Height)
{
	static const std::wstring Value = Read(L"SANDBOX_RESOLUTION");

	if (Value.empty())
	{
		return false;
	}

	const size_t Separator = Value.find_first_of(L"xX*,");

	if (Separator == std::wstring::npos)
	{
		return false;
	}

	const long W = wcstol(Value.c_str(), NULL, 10);
	const long H = wcstol(Value.c_str() + Separator + 1, NULL, 10);

	// A sanity range, not a mode list: what the display driver actually accepts
	// is checked by the driver itself.
	if (W < 100 || H < 100 || W > 16384 || H > 16384)
	{
		return false;
	}

	Width = (int)W;
	Height = (int)H;

	return true;
}
