#include <iostream>

using namespace std;

#include "bit_memory_stream.h"

#include "bit_value.h"

namespace MultiPLAY
{
	bit_memory_stream::bit_memory_stream(char *memory, long bits)
	{
		this->memory = memory;
		this->bits = bits;
		index = 0;
	}

	long bit_memory_stream::size()
	{
		return bits;
	}

	long bit_memory_stream::tellg()
	{
		return index;
	}

	void bit_memory_stream::seekg(long offset, seek_dir dir/* = ios::beg*/)
	{
		long base = 0;

		switch (dir)
		{
			case ios::beg: base = 0; break;
			case ios::cur: base = tellg(); break;
			case ios::end: base = size(); break;
		}

		index = base + offset;
	}

	bool bit_memory_stream::get()
	{
		unsigned offset = unsigned(index >> 3);
		unsigned bit = unsigned(index & 7);

		index++;

		return (0 != (unsigned(memory[offset]) & bit_value[bit]));
	}

	bool bit_memory_stream::eof()
	{
		return (index >= size());
	}

	void bit_memory_stream::read(char *ptr, int num_bits, int pad_to_length/* = 4*/)
	{
		while (num_bits > 8)
		{
			unsigned int value = 0;

			for (int i=0; i<8; i++)
				if (get())
					value |= bit_value[i];

			*ptr = (char)value;
			ptr++;
			num_bits -= 8;
			pad_to_length--;
		}

		unsigned int value = 0;

		for (int i=0; i<num_bits; i++)
			if (get())
				value |= bit_value[i];

		*ptr = (char)value;
		ptr++;
		pad_to_length--;

		while (pad_to_length-- > 0)
			*(ptr++) = 0;
	}

	int bit_memory_stream::read_int(int num_bits/* = 32*/)
	{
		unsigned int value = 0;

		for (int i=0; i<num_bits; i++)
			if (get())
				value |= bit_value[i];

		return int(value);
	}
}
