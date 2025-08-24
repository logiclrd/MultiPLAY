#include "formatting.h"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#include "module.h"

#include "charset/UTF8.h"

using namespace CharSet;

namespace MultiPLAY
{
	void format_pattern_note(wostream &d, const row &row)
	{
		//                                     .- escaped for trigraph avoidance
		d << wstring(L"C-C#D-D#E-F-F#G-G#A-A#B-?\?==^^--").substr((row.snote & 15) * 2, 2)
			<< wchar_t((row.snote >= 0) ? (48 + (row.snote >> 4)) : L'-') << L" ";

		if (row.instrument == nullptr)
			d << L"-- ";
		else
			d << setfill(L'0') << setw(2) << row.instrument->index << L" ";
		if (row.volume >= 0)
		{
			if (row.volume > 64)
				d << L"vXX";
			else
				d << L"v" << setfill(L'0') << setw(2) << row.volume;
			if (row.secondary_effect.present)
				d << L"*";
			else
				d << L" ";
		}
		else if (row.secondary_effect.present)
		{
			d << char(row.secondary_effect.command)
				<< hex << uppercase << setfill(L'0') << setw(2) << int(row.secondary_effect.info.data) << nouppercase << dec
				<< L" ";
		}
		else
			d << L"--- ";
		if (row.effect.present)
			d << char(row.effect.command)
				<< hex << uppercase << setfill(L'0') << setw(2) << int(row.effect.info.data) << nouppercase << dec;
		else
			d << L"---";
	}

	void format_pattern_row(wostream &d, unsigned int row_number, const vector<row> &row_data, bool endl/* = true*/)
	{
		d << setw(3) << row_number << setw(0) << L" | ";

		for (unsigned channel = 0; channel < row_data.size(); channel++)
		{
			format_pattern_note(d, row_data[channel]);
			d << L"    ";
		}

		d << endl;
	}

	void format_pattern(wostream &d, const pattern *pattern_ptr)
	{
		if (pattern_ptr == nullptr)
		{
			d << L"NULL POINTER" << endl;
			return;
		}

		const pattern &pattern = *pattern_ptr;

		if (pattern.is_skip_marker)
			d << L"+++" << endl;
		else if (pattern.is_end_marker)
			d << L"---" << endl;
		else
		{
			for (unsigned row_number = 0; row_number < pattern.row_list.size(); row_number++)
				format_pattern_row(d, row_number, pattern.row_list[row_number]);
		}
	}

	extern void format_sample(wostream &d, const sample *sample)
	{
		if (sample == nullptr)
			d << L"NULL POINTER" << endl;
		else
			d << sample->num_samples << L" @" << sample->samples_per_second << L"Hz" << endl;
	}

	extern void format_module(wostream &d, const module_struct *mod)
	{
		if (mod == nullptr)
			d << L"NUL" << endl;
		else
		{
			d << L"filename: " << mod->filename << endl;
			d << L"name: " << mod->name << endl;

			d << L"pattern: " << mod->patterns.size() << endl;
			for (size_t i = 0, l = mod->patterns.size(); i < l; i++)
			{
				d << L"[" << i << L"]:" << endl;
				format_pattern(d, &mod->patterns[i]);
			}

			d << L"pattern list: " << mod->pattern_list_length << endl;
			for (size_t i = 0, l  = unsigned(mod->pattern_list_length); i < l; i++)
			{
				d << L"[" << i << L"]: ";

				bool found = false;

				for (size_t j = 0, k = mod->patterns.size(); j < k; j++)
				{
					if (mod->pattern_list[i] == &mod->patterns[j])
					{
						d << L"&pattern[" << j << L"]" << endl;
						found = true;
						break;
					}
				}

				if (!found)
					d << L"UNKNOWN POINTER" << endl;
			}

			d << L"samples: " << mod->samples.size() << endl;
			for (size_t i = 0, l = mod->samples.size(); i < l; i++)
			{
				d << L"[" << i << L"]: ";
				format_sample(d, mod->samples[i]);
			}

			d << L"sample_info: " << mod->sample_info.size() << endl;
			for (size_t i = 0, l = mod->sample_info.size(); i < l; i++)
				d << L"[" << i << L"]: vibrato_speed == " << mod->sample_info[i].vibrato_speed << L", vibrato_depth == " << mod->sample_info[i].vibrato_depth << endl;

			d << L"channel_enabled: ";
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << int(mod->channel_enabled[i]);
			d << endl;

			d << L"channel_map:" << endl;
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << L"[" << i << L"]: " << mod->channel_map[i] << endl;

			d << L"pattern_loop:" << endl;
			for (unsigned i = 0; i < MAX_MODULE_CHANNELS; i++)
				d << L"[" << i << L"]: " << mod->pattern_loop[i].need_repetitions << L" at " << mod->pattern_loop[i].row << L" with count " << mod->pattern_loop[i].repetitions << endl;
		}
	}

	namespace
	{
		int dump_file_index = 0;
	}

	extern void dump_module_to_next_file(module_struct *mod)
	{
		wstringstream dump_buffer;

		format_module(dump_buffer, mod);

		stringstream ss;

		ss << L"dump_" << dump_file_index++ << L".txt";

		ofstream file(ss.str());

		file << unicode_to_utf8(dump_buffer.str());
	}
}
