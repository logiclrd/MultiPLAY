#ifndef LOAD_XM_H
#define LOAD_XM_H

#include <iostream>

#include "module.h"

namespace MultiPLAY
{
	module_struct *load_xm(std::istream *file);
}

#endif // LOAD_XM_H