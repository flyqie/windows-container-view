#pragma once

// Starts the container-side client and does not return until the container's
// windows are gone or the server goes away.
//
// Host is the machine running Sandbox-Server. Command is the command line to
// start on the container's desktop; empty means cmd.exe.
void SandboxInitClient(const wchar_t* Host, const wchar_t* Command);
