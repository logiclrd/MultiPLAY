#include "module.h"

namespace MultiPLAY
{
	// struct module_struct
	void module_struct::speed_change()
	{
		previous_ticks_per_module_row = ticks_per_module_row;
		ticks_per_module_row = speed * 60 * ticks_per_second / (24 * tempo);
		ticks_per_frame = double(ticks_per_module_row) / speed;
/*
		ticks_per_module_row = speed * ticks_per_quarter_note / 24
		ticks_per_quarter_note = seconds_per_quarter_note * ticks_per_second
		seconds_per_quarter_note = seconds_per_minute / quarter_notes_per_minute /* */
	}

	void module_struct::initialize()
	{
		speed_change();
		current_row = -1;
		finished = false;
	}

	module_struct::module_struct()
	{
		use_instruments = false;
		it_module = false;
		it_module_new_effects = false;
		it_module_portamento_link = false;
		it_module_linear_slides = false;
		auto_loop_target = -1;
	}
}
