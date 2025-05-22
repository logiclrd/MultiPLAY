#ifndef LINT_H
#define LINT_H

#include <csignal>
#include <cmath>

namespace
{
	bool is_crazy(double value)
	{
		if (std::isnan(value))
			return true;
		if (std::isinf(value))
			return true;
		if (std::abs(value) > 1e100)
			return true;

		return false;
	}
}

#ifdef DEBUG
#define LINT_DOUBLE_CHECK(x) is_crazy(x)
#define LINT_DOUBLE(x) do{if (LINT_DOUBLE_CHECK(x)) std::raise(SIGTRAP); }while(0)
#define LINT_POSITIVE_DOUBLE(x) do{if (LINT_DOUBLE_CHECK(x) || (x <= 0)) std::raise(SIGTRAP); }while(0)
#else
#define LINT_DOUBLE_CHECK(x) false
#define LINT_DOUBLE(x) do{}while(0)
#define LINT_POSITIVE_DOUBLE(x) do{}while(0)
#endif

#endif // LINT_H