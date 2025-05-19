#ifndef LOAD_MTM_H
#define LOAD_MTM_H

#include <iostream>

#include "module.h"

namespace MultiPLAY
{
  extern module_struct *load_mtm(std::istream *file);
}

#endif // LOAD_MTM_H
