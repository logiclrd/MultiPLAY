#include "Profile.h"

#ifdef WIN32
LARGE_INTEGER s_performanceCounterFrequency;

struct InitializeProfiling
{
	InitializeProfiling()
	{
		QueryPerformanceFrequency(&s_performanceCounterFrequency);
	}
} _initializeProfiling;
#endif