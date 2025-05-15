#ifndef CONVERSION_H
#define CONVERSION_H

namespace MultiPLAY
{
	extern int from_lsb2(unsigned char in[2]);
	extern unsigned int from_lsb2_u(unsigned char in[2]);
	extern int from_lsb4(unsigned char in[4]);
	extern long from_lsb4_l(unsigned char in[4]);
	extern unsigned long from_lsb4_lu(unsigned char in[4]);
	extern int from_msb2(unsigned char in[2]);
	extern unsigned int from_msb2_u(unsigned char in[2]);
	extern int from_msb4(unsigned char in[4]);
}

#endif // CONVERSION_H
