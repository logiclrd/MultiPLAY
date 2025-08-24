#ifndef CONVERSION_H
#define CONVERSION_H

#include <string>

namespace MultiPLAY
{
	extern short from_lsb2(unsigned char in[2]);
	extern unsigned short from_lsb2_u(unsigned char in[2]);
	extern int from_lsb4(unsigned char in[4]);
	extern long from_lsb4_l(unsigned char in[4]);
	extern unsigned long from_lsb4_lu(unsigned char in[4]);
	extern short from_msb2(unsigned char in[2]);
	extern unsigned short from_msb2_u(unsigned char in[2]);
	extern int from_msb4(unsigned char in[4]);

	extern std::wstring &make_lowercase(std::wstring &s);
	extern std::wstring trim(std::wstring in);
}

#endif // CONVERSION_H
