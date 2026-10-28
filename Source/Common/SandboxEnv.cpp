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
