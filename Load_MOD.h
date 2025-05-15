#ifndef LOAD_MOD_H
#define LOAD_MOD_H

#include <fstream>

#include "module.h"

namespace MultiPLAY
{
	extern module_struct *load_mod(std::ifstream *file, bool mud = false);
}

#endif // LOAD_MOD_H
