#ifndef __BIT_MEMORY_STREAM_H__
#define __BIT_MEMORY_STREAM_H__

#include <iostream>

namespace MultiPLAY
{
	struct bit_memory_stream
	{
	#ifdef _HAS
		typedef std::ios_base::seek_dir seek_dir;
	#else
		typedef std::ios_base::seekdir seek_dir;
	#endif

		char *memory;
		long bits;

		long index;

		bit_memory_stream(char *memory, long bits);

		long size();
		long tellg();
		void seekg(long offset, seek_dir dir = ios::beg);
		bool get();
		bool eof();
		void read(char *ptr, int num_bits, int pad_to_length = 4);
		int read_int(int num_bits = 32);
	};
}

#endif // __BIT_MEMORY_STREAM_H__