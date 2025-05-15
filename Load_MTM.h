#ifndef LOAD_MTM_H
#define LOAD_MTM_H

#include <fstream>

#include "module.h"

namespace MultiPLAY
{
  extern module_struct *load_mtm(std::ifstream *file);
}

#endif // LOAD_MTM_H
