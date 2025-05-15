#include "Profile.h"

#if WIN32
LARGE_INTEGER s_performanceCounterFrequency;

struct InitializeProfiling
{
	InitializeProfiling()
	{
		QueryPerformanceFrequency(&s_performanceCounterFrequency);
	}
} _initializeProfiling;
#endif