#include "conversion.h"

#include <functional>
#include <algorithm>
#include <string>

using namespace std;

namespace MultiPLAY
{
	extern int from_lsb2(unsigned char in[2])
	{
		return in[0] | (static_cast<signed char>(in[1]) << 8);
	}

	extern unsigned int from_lsb2_u(unsigned char in[2])
	{
		return in[0] | (in[1] << 8);
	}

	extern int from_lsb4(unsigned char in[4])
	{
		return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
	}

	extern long from_lsb4_l(unsigned char in[4])
	{
		return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
	}

	extern unsigned long from_lsb4_lu(unsigned char in[4])
	{
		return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
	}

	extern int from_msb2(unsigned char in[2])
	{
		return in[1] | (static_cast<signed char>(in[0]) << 8);
	}

	extern unsigned int from_msb2_u(unsigned char in[2])
	{
		return in[1] | (in[0] << 8);
	}

	extern int from_msb4(unsigned char in[4])
	{
		return in[3] | (in[2] << 8) | (in[1] << 16) | (in[0] << 24);
	}

	extern string &make_lowercase(string &s)
	{
		transform(s.begin(), s.end(), s.begin(), std::function<int(int)>(::tolower));
		return s;
	}

	extern string trim(string in)
	{
		auto start = in.find_first_not_of(" \t\n");

		if (start == string::npos)
			return "";
		else
		{
			auto end = in.find_last_not_of(" \t\n");

			if (end == string::npos)
				return in.substr(start);
			else
				return in.substr(start, end - start + 1);
		}
	}
}
