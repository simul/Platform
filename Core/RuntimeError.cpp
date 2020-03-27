
#include "Platform/Core/RuntimeError.h"

#if SIMUL_INTERNAL_CHECKS
bool simul::base::SimulInternalChecks = true;
#else
bool simul::base::SimulInternalChecks = false;
#endif
