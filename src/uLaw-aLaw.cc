#include "uLaw-aLaw.h"

#include <cstdlib>

using namespace std;

namespace Telephony
{
	namespace
	{
		char ulaw_alaw_segment_end[256];
		short ulaw_code_value[256];
		short alaw_code_value[256];

		void init_ulaw_alaw_segment_end()
		{
			for (int i=0, j=1, k=0; i<256; i++)
			{
				ulaw_alaw_segment_end[i] = char(k);
				if (i + 1 == j)
					k++, j += j;
			}
		}

		void init_ulaw_code_value()
		{
			for (int i=0; i<256; i++)
			{
				int inv = 0xFF & ~i;

				int noise = ((inv & 0xF) << 3) + 0x84;
				int scale = (inv & 0x70) >> 4;
				int sign = (inv & 0x80);

				ulaw_code_value[i] = (short)(
					sign
					? 0x84 - (noise << scale)
					: (noise << scale) - 0x84);
			}
		}

		void init_alaw_code_value()
		{
			for (int i=0; i<256; i++)
			{
				int code = 0x55 ^ i;
				int value = ((code & 15) << 4) + 8;
				int segment = (code >> 4) & 7;
				int sign = (code & 0x80);

				if (segment > 0)
					value += 256;

				value <<= (segment - 1);

				alaw_code_value[i] = short(sign ? -value : value);
			}
		}
	}

	extern void init_ulaw_alaw()
	{
		init_ulaw_alaw_segment_end();
		init_ulaw_code_value();
		init_alaw_code_value();
	}

	extern unsigned char ulaw_encode_sample(short sample)
	{
		unsigned short asample = (unsigned short)abs(sample);
		unsigned char uhigh = ((unsigned short)sample) >> 8;

		if (asample > 0x7F7B)
			asample = 0x7F7B;

		sample = 0x84 + asample; // bias the magnitude, this has the effect of amplifying low amplitudes more than high ones

		int segment = ulaw_alaw_segment_end[sample >> 8];
		int drop_bits = segment + 3;

		return (unsigned char)(0xFF ^ (uhigh & 0x80) ^ ((segment << 4) | ((sample >> drop_bits) & 0xF)));
	}

	extern unsigned char alaw_encode_sample(short sample)
	{
		unsigned char uhigh = ((unsigned short)sample) >> 8;

		if (sample < 0)
		{
			sample = -sample - 8;
			if (sample < 0)
				sample = 0;
		}

		int segment = ulaw_alaw_segment_end[sample >> 8];
		int drop_bits = segment + 3;

		if (drop_bits < 4)
			drop_bits = 4;

		return (unsigned char)(0x55 ^ (uhigh & 0x80) ^ ((segment << 4) | ((sample >> drop_bits) & 0xF)));
	}

	extern short ulaw_decode_sample(unsigned char sample)
	{
		return ulaw_code_value[sample];
	}

	extern short alaw_decode_sample(unsigned char sample)
	{
		return alaw_code_value[sample];
	}
}
