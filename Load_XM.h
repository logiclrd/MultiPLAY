#ifndef LOAD_XM_H
#define LOAD_XM_H

#include <fstream>

#include "module.h"

namespace MultiPLAY
{
	module_struct *load_xm(std::ifstream *file);
}

#endif // LOAD_XM_H