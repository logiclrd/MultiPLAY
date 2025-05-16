#ifndef FORMATTING_H
#define FORMATTING_H

#include <iostream>

#include "module.h"

namespace MultiPLAY
{
	extern void format_pattern_note(std::ostream &dest, const row &row);
	extern void format_pattern_row(std::ostream &dest, unsigned int row_number, const vector<row> &row_data, bool endl = true);
	extern void format_pattern(std::ostream &dest, const pattern *pattern);
	extern void format_sample(ostream &d, const sample *sample);
	extern void format_module(std::ostream &dest, const module_struct *mod);

	extern void dump_module_to_next_file(module_struct *mod);
}

#endif // FORMATTING_H

