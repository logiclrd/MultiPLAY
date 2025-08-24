#ifndef STRING_H
#define STRING_H

#include <string>

#include "charset/CP437.h"

namespace
{
	inline void assign_string_from_structure_field(wstring &dest, const char *src, int src_size)
	{
		std::string bytes(src, src_size);

		std::string::size_type term = bytes.find('\0');

		if (term != std::string::npos)
			bytes.erase(term);

		dest = CharSet::cp437_to_unicode(bytes);
	}
}

// This macro assumes that src is a char[N] embedded in the middle
// of a structure, and that it may be null-terminated, but it might
// also use all characters in the buffer. This is a common case in
// the data structures within various module formats.
//
// dest is an std::string to be populated with the string contained
// in src, truncated at the first '\0', or containing every character
// in src if there is no terminator.
//
// std::find(begin, end, item) takes two iterators and a value of the
// type the iterators refer to, and it walks from begin to end looking
// for a match. If it finds one, it returns the first such iterator
// whose target equals the desired item. If it reaches end, then it
// simply returns end. This is perfect for our use case.

#define ASSIGN_STRING_FROM_STRUCTURE_FIELD(dest, src)   \
        assign_string_from_structure_field(dest, src, sizeof(src))

#endif // STRING_H
