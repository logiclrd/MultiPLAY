#ifndef MODULE_H
#define MODULE_H

#include <string>
#include <vector>

#include "MultiPLAY.h"

#include "pattern.h"

namespace MultiPLAY
{
	#define MAX_MODULE_CHANNELS 64

	struct module_sample_info
	{
		char vibrato_depth, vibrato_speed;
	};

	struct module_struct
	{
		std::string filename, name;
		std::vector<std::string> information_text;
		std::vector<pattern> patterns;
		unsigned int pattern_list_length; // Populated after load by the main driver.
		std::vector<pattern *> pattern_list;
		std::vector<sample *> samples;
		std::vector<module_sample_info> sample_info;
		bool channel_enabled[MAX_MODULE_CHANNELS];
		unsigned int channel_map[MAX_MODULE_CHANNELS];
		pattern_loop_type pattern_loop[MAX_MODULE_CHANNELS];
		pan_value initial_panning[MAX_MODULE_CHANNELS];
		int base_pan[MAX_MODULE_CHANNELS];
		int ticks_per_module_row, previous_ticks_per_module_row;
		double ticks_per_frame;
		int speed, tempo;
		unsigned int current_pattern;
		int current_row;
		int override_next_row;
		unsigned int num_channels;
		int auto_loop_target;
		bool stereo, use_instruments;
		bool amiga_panning, linear_slides;
		bool it_module, it_module_new_effects, it_module_portamento_link;
		bool xm_module;
		bool channel_remember_note;
		bool finished;

		module_struct();

		void speed_change();
		void initialize();
	};
}

#endif // MODULE_H