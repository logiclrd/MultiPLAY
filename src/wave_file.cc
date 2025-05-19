#include <string>

using namespace std;

#include "wave_file.h"

#include "MultiPLAY.h"

#include "math.h"

using namespace MultiPLAY;

namespace WaveFile
{
	namespace WAV
	{
		// union chunk
		string chunk::chunkID()
		{
			return string((char *)raw.chunkID, 4);
		}
	}

	namespace AIFF
	{
		// union chunk
		string chunk::chunkID()
		{
			return string((char *)raw.chunkID, 4);
		}
	}

	// struct ieee_extended
	double ieee_extended::toDouble()
	{
		int sign = data[0] & 128;
		double mantissa = 0.0;
		double add = 1.0;

		int exponent = ((data[1]) | ((data[0] & 0x7F) << 8)) - 16383;

		for (int i=2; i<10; i++)
		{
			for (int j=128; j >= 1; j >>= 1)
			{
				if (data[i] & j)
					mantissa += add;
				add /= 2.0;
			}
		}

		if (sign)
			return mantissa * -p2(exponent);
		else
			return mantissa * p2(exponent);
	}
}
