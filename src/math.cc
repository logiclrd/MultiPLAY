#include <cmath>

using namespace std;

#include "math.h"

#define LOG2 0.6931471805599453

namespace MultiPLAY
{
	extern double lg(double x)
	{
		return log(x) * (1.0 / LOG2);
	}

	extern double p2(double x)
	{
		return exp(LOG2 * x);
	}

	extern int req_digits(int max)
	{
		if (max < 10)
			return 1;
		if (max < 100)
			return 2;
		return 3;
	}
}
