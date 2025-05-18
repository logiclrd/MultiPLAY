#include "notes.h"

#include <cmath>

using namespace std;

#include "math.h"

namespace MultiPLAY
{
	extern int znote_from_snote(int snote)
	{
		// middle C is z=39
		// snote middle C is high nybble = 4, low nybble = 0 -> 64
		return (12 * (snote >> 4)) + (snote & 15) - 11;
	}

	extern int snote_from_znote(int znote)
	{
		int octave = (znote + 9) / 12;
		int note = (znote + 9) - 12 * octave;

		return (octave << 4) | note;
	}

	extern int snote_from_period(int period)
	{
		double frequency = 3579264.0 / period;
		int qnote = int(floor(12 * lg(frequency)));

		return (qnote % 12) | (((qnote / 12) - 9) << 4);
	}

	extern int snote_from_inote(int inote)
	{
		if (inote < 120)
		{
			int octave = inote / 12;
			int note = inote % 12;

			return (octave << 4) | note;
		}

		if (inote == 254)
			return -2;

		if (inote == 255)
			return -3;

		return -1; // oh well, just ignore it, I guess
	}
}