#ifndef FORMATTING_H
#define FORMATTING_H

#include <iostream>
#include <vector>

#include "module.h"

namespace MultiPLAY
{
	extern void format_pattern_note(std::wostream &dest, const row &row);
	extern void format_pattern_row(std::wostream &dest, unsigned int row_number, const std::vector<row> &row_data, bool endl = true);
	extern void format_pattern(std::wostream &dest, const pattern *pattern);
	extern void format_sample(std::wostream &d, const sample *sample);
	extern void format_module(std::wostream &dest, const module_struct *mod);

	extern void dump_module_to_next_file(module_struct *mod);
}

#endif // FORMATTING_H
