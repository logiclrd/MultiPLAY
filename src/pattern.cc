#include "pattern.h"

#include <cstring>

using namespace std;

namespace MultiPLAY
{
	// struct row
	row::row()
	{
		snote = -1;
		volume = -1;
		instrument = NULL;
	}

	// struct pattern
	pattern::pattern(int index)
		: is_skip_marker(false), is_end_marker(false)
	{
		this->index = index;
	}

	pattern::pattern(const pattern &other)
		: index(other.index),
			is_skip_marker(other.is_skip_marker),
			is_end_marker(other.is_end_marker),
			row_list(other.row_list)
	{
	}

	pattern &pattern::operator =(const pattern &other)
	{
		memcpy((void *)this, (const void *)&other, sizeof(*this));

		return *this;
	}

	//private:
	pattern::pattern(bool is_skip_marker, bool is_end_marker)
		: is_skip_marker(is_skip_marker), is_end_marker(is_end_marker)
	{
	}

	pattern pattern::skip_marker(true, false);
	pattern pattern::end_marker(false, true);

	// struct pattern_loop_type
	pattern_loop_type::pattern_loop_type()
	{
		row = 0;
		repetitions = 0;
		need_repetitions = true;
	}
}
