#include "notes.h"

#include <cmath>

using namespace std;

#include "math.h"

namespace MultiPLAY
{
	extern int inote_from_znote(int znote)
	{
		int inote = znote + 11;

		if ((inote >= 0) && (inote < 120))
			return inote;
		else
			return -1;
	}

	extern int znote_from_snote(int snote)
	{
		if (snote < 0)
			return -1;
		else
		{
			// middle C is z=39
			// snote middle C is high nybble = 4, low nybble = 0 -> 64
			return (12 * (snote >> 4)) + (snote & 15) - 11;
		}
	}

	extern int snote_from_znote(int znote)
	{
		if (znote < 0)
			return -1;
		else
		{
			int octave = (znote + 9) / 12;
			int note = (znote + 9) - 12 * octave;

			return (octave << 4) | note;
		}
	}

	extern int snote_from_period(int period)
	{
		double frequency = 3579264.0 / period;
		int qnote = int(floor(12 * lg(frequency)));

		return (qnote % 12) | (((qnote / 12) - 9) << 4);
	}
}