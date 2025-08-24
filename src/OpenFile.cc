#ifndef OPENFILE_H
#define OPENFILE_H

#include "charset/UTF8.h"

#include <iostream>
#include <fstream>

using namespace std;

namespace File
{
#ifdef WIN32
	ifstream OpenRead(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return ifstream(path, mode);
	}

	ofstream OpenWrite(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return ofstream(path, mode);
	}

	ifstream *OpenReadPtr(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return new ifstream(path, mode);
	}

	ofstream *OpenWritePtr(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return new ofstream(path, mode);
	}
#else
	ifstream OpenRead(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return ifstream(CharSet::unicode_to_utf8(path), mode);
	}

	ofstream OpenWrite(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return ofstream(CharSet::unicode_to_utf8(path), mode);
	}

	ifstream *OpenReadPtr(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return new ifstream(CharSet::unicode_to_utf8(path), mode);
	}

	ofstream *OpenWritePtr(const std::wstring &path, ios_base::openmode mode = ios::binary)
	{
		return new ofstream(CharSet::unicode_to_utf8(path), mode);
	}
#endif
}

#endif // OPENFILE_H