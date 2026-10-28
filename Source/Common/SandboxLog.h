#pragma once

#include <ostream>

// Console output, enabled by SANDBOX_DEBUG=1.
//
// Both programs are normally driven by something else -- a script, a container
// runtime, a service supervisor -- so everything they print goes through here
// and is silent unless asked for. Failures a caller has to act on are reported
// through return values and exit codes instead, which do not depend on the
// environment.
std::ostream& Debug();

// Same, for the few places that have wide strings to print.
std::wostream& DebugW();

// True when SANDBOX_DEBUG is set to 1, y or t (any case).
bool DebugEnabled();
