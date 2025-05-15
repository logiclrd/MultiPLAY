#include "Profile.h"

#if WIN32
struct InitializeProfiling
{
	InitializeProfiling()
	{
		QueryPerformanceFrequency(&s_performanceCounterFrequency);
	}
} _initializeProfiling;
#endif