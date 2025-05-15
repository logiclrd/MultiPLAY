#ifndef LOAD_UMX_H
#define LOAD_UMX_H

#include <fstream>

#include "module.h"

namespace MultiPLAY
{
	module_struct *load_umx(std::ifstream *file);
}

#endif // LOAD_UMX_H
