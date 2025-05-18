#ifndef PATTERN_H
#define PATTERN_H

#include <vector>

#include "effect.h"
#include "sample.h"

namespace MultiPLAY
{
	struct row
	{
		int znote; // linear representation, difference of 1 is always a semitone, 12 is always an octavie, 39 == middle C
		int snote; // packed representation with the octave in the top 4 bits, and the index into the octave (starting with C) in the lower 4 bits
		int volume;
		sample *instrument;
		effect_struct effect, secondary_effect;

		row();
	};

	struct pattern
	{
		int index;
		const bool is_skip_marker;
		const bool is_end_marker;
		std::vector<std::vector<row>> row_list;

		pattern(int index);
		pattern(const pattern &other);
		pattern &operator =(const pattern &other);
	private:
		pattern(bool is_skip_marker, bool is_end_marker);
	public:
		static pattern skip_marker;
		static pattern end_marker;
	};

	struct pattern_loop_type
	{
		int row, repetitions;
		bool need_repetitions;

		pattern_loop_type();
	};
}

#endif // PATTERN_H
