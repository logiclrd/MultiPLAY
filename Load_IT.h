#ifndef LOAD_IT_H
#define LOAD_IT_H

#include <iostream>

#include "module.h"

namespace MultiPLAY
{
	module_struct *load_it(std::ifstream *file, bool modplug_style = false);
}

#endif // LOAD_IT_H
