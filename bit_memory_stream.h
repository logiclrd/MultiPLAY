#ifndef __BIT_MEMORY_STREAM_H__
#define __BIT_MEMORY_STREAM_H__

#include <locale>

const unsigned int bit_value[]
  = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536,
      131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432,
      67108864, 134217728, 268435456, 536870912, 1073741824, 2147483648U };

struct bit_memory_stream
{
#ifdef _HAS
  typedef ios_base::seek_dir seek_dir;
#else
  typedef ios_base::seekdir seek_dir;
#endif

  char *memory;
  long bits;

  long index;

  bit_memory_stream(char *memory, long bits)
  {
    this->memory = memory;
    this->bits = bits;
  }

  long size()
  {
    return bits;
  }

  long tellg()
  {
    return index;
  }

  void seekg(long offset, seek_dir dir = ios::beg)
  {
    long base;

    switch (dir)
    {
      case ios::beg: base = 0;
      case ios::cur: base = tellg();
      case ios::end: base = size();
    }

    index = base + offset;
  }

  bool get()
  {
    int offset = index >> 3;
    int bit = index & 7;

    index++;

    return (0 != (memory[offset] & bit_value[bit]));
  }

  bool eof()
  {
    return (index >= size());
  }

  void read(char *ptr, int num_bits, int pad_to_length = 4)
  {
    while (num_bits > 8)
    {
      int value = 0;

      for (int i=0; i<8; i++)
        if (get())
          value |= bit_value[i];

      *ptr = value;
      ptr++;
      num_bits -= 8;
      pad_to_length--;
    }

    int value = 0;

    for (int i=0; i<num_bits; i++)
      if (get())
        value |= bit_value[i];

    *ptr = value;
    ptr++;
    pad_to_length--;

    while (pad_to_length-- > 0)
      *(ptr++) = 0;
  }

  int read_int(int num_bits = 32)
  {
    int value;

    for (int i=0; i<num_bits; i++)
      if (get())
        value |= bit_value[i];

    return value;
  }
};

#endif // __BIT_MEMORY_STREAM_H__
