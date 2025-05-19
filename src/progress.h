#ifndef PROGRESS_H
#define PROGRESS_H

#include <string>

namespace MultiPLAY
{
	struct PlaybackPosition
	{
		int Order, OrderCount;
		int Pattern, PatternCount;
		int Row, RowCount;
		long Offset, OffsetCount;
		const char *FormatString;

		PlaybackPosition();

		void clear();
		bool is_changed(const PlaybackPosition &reference, const PlaybackPosition &relevant_fields);
		int get_field(const std::string &field_name);
		void set_field(const std::string &field_name, int value);
	};

	struct PlaybackTime
	{
		int Hour, Minute, Second;

		void update(long long tick_count, long ticks_per_second);
		bool is_changed(const PlaybackTime &reference);
	};
}

#endif // PROGRESS_H
