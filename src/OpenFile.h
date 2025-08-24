#ifndef OPENFILE_H
#define OPENFILE_H

#include "charset/UTF8.h"

#include <iostream>
#include <fstream>

using namespace std;

namespace File
{
	ifstream OpenRead(const std::wstring &path, ios_base::openmode mode = ios::binary);
	ofstream OpenWrite(const std::wstring &path, ios_base::openmode mode = ios::binary);

	ifstream *OpenReadPtr(const std::wstring &path, ios_base::openmode mode = ios::binary);
	ofstream *OpenWritePtr(const std::wstring &path, ios_base::openmode mode = ios::binary);
}

#endif // OPENFILE_H