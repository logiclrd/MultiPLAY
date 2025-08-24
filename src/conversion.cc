#include "conversion.h"

#include <functional>
#include <algorithm>
#include <cwctype>
#include <string>

using namespace std;

namespace MultiPLAY
{
	extern short from_lsb2(unsigned char in[2])
	{
		return in[0] | (static_cast<signed char>(in[1]) << 8);
	}

	extern unsigned short from_lsb2_u(unsigned char in[2])
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
		return (unsigned long)(in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24));
	}

	extern short from_msb2(unsigned char in[2])
	{
		return in[1] | (static_cast<signed char>(in[0]) << 8);
	}

	extern unsigned short from_msb2_u(unsigned char in[2])
	{
		return in[1] | (in[0] << 8);
	}

	extern int from_msb4(unsigned char in[4])
	{
		return in[3] | (in[2] << 8) | (in[1] << 16) | (in[0] << 24);
	}

	extern wstring &make_lowercase(wstring &s)
	{
		transform(s.begin(), s.end(), s.begin(), std::function<int(int)>(::towlower));
		return s;
	}

	extern wstring trim(wstring in)
	{
		auto start = in.find_first_not_of(L" \t\n");

		if (start == wstring::npos)
			return L"";
		else
		{
			auto end = in.find_last_not_of(L" \t\n");

			if (end == wstring::npos)
				return in.substr(start);
			else
				return in.substr(start, end - start + 1);
		}
	}
}
