#include "formatting.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#include "module.h"

namespace MultiPLAY
{
	void format_pattern_note(ostream &d, const row &row)
	{
		//                                     .- escaped for trigraph avoidance
		d << string("C-C#D-D#E-F-F#G-G#A-A#B-?\?==^^--").substr((row.snote & 15) * 2, 2)
			<< char((row.snote >= 0) ? (48 + (row.snote >> 4)) : '-') << " ";

		if (row.instrument == NULL)
			d << "-- ";
		else
			d << setfill('0') << setw(2) << row.instrument->index << " ";
		if (row.volume >= 0)
		{
			if (row.volume > 64)
				d << "vXX";
			else
				d << "v" << setfill('0') << setw(2) << row.volume;
			if (row.secondary_effect.present)
				d << "*";
			else
				d << " ";
		}
		else if (row.secondary_effect.present)
		{
			d << char(row.secondary_effect.command)
				<< hex << uppercase << setfill('0') << setw(2) << int(row.secondary_effect.info.data) << nouppercase << dec
				<< " ";
		}
		else
			d << "--- ";
		if (row.effect.present)
			d << char(row.effect.command)
				<< hex << uppercase << setfill('0') << setw(2) << int(row.effect.info.data) << nouppercase << dec;
		else
			d << "---";
	}

	void format_pattern_row(ostream &d, unsigned int row_number, const vector<row> &row_data, bool endl/* = true*/)
	{
		d << setw(3) << row_number << setw(0) << " | ";

		for (unsigned channel = 0; channel < row_data.size(); channel++)
		{
			format_pattern_note(d, row_data[channel]);
			d << "    ";
		}

		d << endl;
	}

	void format_pattern(ostream &d, const pattern *pattern_ptr)
	{
		if (pattern_ptr == NULL)
		{
			d << "NULL POINTER" << endl;
			return;
		}

		const pattern &pattern = *pattern_ptr;

		if (pattern.is_skip_marker)
			d << "+++" << endl;
		else if (pattern.is_end_marker)
			d << "---" << endl;
		else
		{
			for (unsigned row_number = 0; row_number < pattern.row_list.size(); row_number++)
				format_pattern_row(d, row_number, pattern.row_list[row_number]);
		}
	}

	extern void format_sample(ostream &d, const sample *sample)
	{
		if (sample == NULL)
			d << "NULL POINTER" << endl;
		else
			d << sample->num_samples << " @" << sample->samples_per_second << "Hz" << endl;
	}

	extern void format_module(ostream &d, const module_struct *mod)
	{
		if (mod == NULL)
			d << "NULL" << endl;
		else
		{
			d << "filename: " << mod->filename << endl;
			d << "name: " << mod->name << endl;

			d << "pattern: " << mod->patterns.size() << endl;
			for (unsigned i = 0, l = mod->patterns.size(); i < l; i++)
			{
				d << "[" << i << "]:" << endl;
				format_pattern(d, &mod->patterns[i]);
			}

			d << "pattern list: " << mod->pattern_list_length << endl;
			for (unsigned i = 0, l  = mod->pattern_list_length; i < l; i++)
			{
				d << "[" << i << "]: ";

				bool found = false;

				for (unsigned j = 0, k = mod->patterns.size(); j < k; j++)
				{
					if (mod->pattern_list[i] == &mod->patterns[j])
					{
						d << "&pattern[" << j << "]" << endl;
						found = true;
						break;
					}
				}

				if (!found)
					d << "UNKNOWN POINTER" << endl;
			}

			d << "samples: " << mod->samples.size() << endl;
			for (unsigned i = 0, l = mod->samples.size(); i < l; i++)
			{
				d << "[" << i << "]: ";
				format_sample(d, mod->samples[i]);
			}

			d << "sample_info: " << mod->sample_info.size() << endl;
			for (unsigned i = 0, l = mod->sample_info.size(); i < l; i++)
				d << "[" << i << "]: vibrato_speed == " << mod->sample_info[i].vibrato_speed << ", vibrato_depth == " << mod->sample_info[i].vibrato_depth << endl;

			d << "channel_enabled: ";
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << int(mod->channel_enabled[i]);
			d << endl;

			d << "channel_map:" << endl;
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << "[" << i << "]: " << mod->channel_map[i] << endl;

			d << "pattern_loop:" << endl;
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << "[" << i << "]: " << mod->pattern_loop[i].need_repetitions << " at " << mod->pattern_loop[i].row << " with count " << mod->pattern_loop[i].repetitions << endl;
		}
	}

	namespace
	{
		int dump_file_index = 0;
	}

	extern void dump_module_to_next_file(module_struct *mod)
	{
		stringstream ss;

		ss << "dump_" << dump_file_index++ << ".txt";

		ofstream d(ss.str());

		format_module(d, mod);
	}
}
