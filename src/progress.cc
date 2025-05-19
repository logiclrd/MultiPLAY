#include <string>

using namespace std;

#include "progress.h"

namespace MultiPLAY
{
	// struct PlaybackPosition
	PlaybackPosition::PlaybackPosition()
	{
		clear();
	}

	void PlaybackPosition::clear()
	{
		Order = OrderCount = Pattern = PatternCount = Row = RowCount = Offset = OffsetCount = 0;
		FormatString = NULL;
	}

	bool PlaybackPosition::is_changed(const PlaybackPosition &reference, const PlaybackPosition &relevant_fields)
	{
		if (FormatString != reference.FormatString)
			return true;

		if (relevant_fields.Order && (Order != reference.Order))
			return true;
		if (relevant_fields.OrderCount && (OrderCount != reference.OrderCount))
			return true;
		if (relevant_fields.Pattern && (Pattern != reference.Pattern))
			return true;
		if (relevant_fields.PatternCount && (PatternCount != reference.PatternCount))
			return true;
		if (relevant_fields.Row && (Row != reference.Row))
			return true;
		if (relevant_fields.RowCount && (RowCount != reference.RowCount))
			return true;
		if (relevant_fields.Offset && (Offset != reference.Offset))
			return true;
		if (relevant_fields.OffsetCount && (OffsetCount != reference.OffsetCount))
			return true;

		return false;
	}

	int PlaybackPosition::get_field(const string &field_name)
	{
		if (field_name == "Order")
			return Order;
		if (field_name == "OrderCount")
			return OrderCount;
		if (field_name == "Pattern")
			return Pattern;
		if (field_name == "PatternCount")
			return PatternCount;
		if (field_name == "Row")
			return Row;
		if (field_name == "RowCount")
			return RowCount;
		if (field_name == "Offset")
			return Offset;
		if (field_name == "OffsetCount")
			return OffsetCount;

		return -1;
	}

	void PlaybackPosition::set_field(const string &field_name, int value)
	{
		if (field_name == "Order")
			Order = value;
		if (field_name == "OrderCount")
			OrderCount = value;
		if (field_name == "Pattern")
			Pattern = value;
		if (field_name == "PatternCount")
			PatternCount = value;
		if (field_name == "Row")
			Row = value;
		if (field_name == "RowCount")
			RowCount = value;
		if (field_name == "Offset")
			Offset = value;
		if (field_name == "OffsetCount")
			OffsetCount = value;
	}

	// struct PlaybackTime
	void PlaybackTime::update(long long tick_count, long ticks_per_second)
	{
		Second = int(tick_count / ticks_per_second);

		Hour = Second / 3600;
		Second -= Hour * 3600;

		Minute = Second / 60;
		Second -= Minute * 60;
	}

	bool PlaybackTime::is_changed(const PlaybackTime &reference)
	{
		if (Hour != reference.Hour)
			return true;
		if (Minute != reference.Minute)
			return true;
		if (Second != reference.Second)
			return true;

		return false;
	}
}
