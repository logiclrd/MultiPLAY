#ifndef LOAD_S3M_H
#define LOAD_S3M_H

#include <fstream>

#include "module.h"

namespace MultiPLAY
{
  module_struct *load_s3m(std::ifstream *file);
}

#endif // LOAD_S3M_H
