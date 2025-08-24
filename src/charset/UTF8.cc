#include "UTF8.h"

#include <string>

using namespace std;

namespace CharSet
{
	#define UNKNOWN_CHAR wchar_t(0xFFFD)

	wstring utf8_to_unicode(string str)
	{
		wstring ret;

		ret.reserve(str.size());

		string::size_type i = 0, l = str.size();

		while (i < l)
		{
			char ch = str[i];

			int bits = 0;
			int additional_bytes;

			if ((ch & 0xF8) == 0xF0)
			{
				bits = ch & 0x07;
				additional_bytes = 3;
			}
			else if ((ch & 0xF0) == 0xE0)
			{
				bits = ch & 0x0F;
				additional_bytes = 2;
			}
			else if ((ch & 0xE0) == 0xC0)
			{
				bits = ch & 0x1F;
				additional_bytes = 1;
			}
			else if ((ch & 0x80) == 0x00)
			{
				bits = ch & 0x7F;
				additional_bytes = 0;
			}
			else
			{
				// Bare 0b10uuuuuu in the middle of the string?
				ret.append(1, UNKNOWN_CHAR);
				continue;
			}

			while (additional_bytes > 0)
			{
				i++;

				if (i >= l)
				{
					// Unexpected end of string
					ret.append(1, UNKNOWN_CHAR);
					break;
				}

				ch = str[i];

				if ((ch & 0xC0) != 0x80)
				{
					// Encountered a byte with the wrong prefix in the middle of a sequence
					i--;
					ret.append(1, UNKNOWN_CHAR);
					continue;
				}

				bits = (bits << 6) | (ch & 0x3F);
				additional_bytes--;
			}

			i++;

			ret.append(1, wchar_t(bits));
		}

		return ret;
	}

	wstring utf8_to_unicode(const char *str)
	{
		return utf8_to_unicode(string(str));
	}

	string unicode_to_utf8(wstring str)
	{
		string ret;

		ret.reserve(str.size() * 5 / 2);

		for (wstring::size_type i=0, l=str.size(); i<l; i++)
		{
			wchar_t code_point = str[i];

			if (code_point < 0x80)
				ret.append(1, (char)code_point);
			else if (code_point < 0x800)
			{
				ret.append(1, 0xC0 | (code_point >> 6));
				ret.append(1, 0x80 | (code_point & 0x3F));
			}
			else if (code_point < 0x10000)
			{
				ret.append(1, 0xE0 | (code_point >> 12));
				ret.append(1, 0x80 | ((code_point >> 6) & 0x3F));
				ret.append(1, 0x80 | (code_point & 0x3F));
			}
			else
			{
				// Will probably never get here, because I don't think wchar_t is more than 16 bits on any of the target platforms
				ret.append(1, 0xF0 | (code_point >> 18) & 7);
				ret.append(1, 0x80 | ((code_point >> 12) & 0x3F));
				ret.append(1, 0x80 | ((code_point >> 6) & 0x3F));
				ret.append(1, 0x80 | (code_point & 0x3F));
			}
		}

		return ret;
	}

	string unicode_to_utf8(const wchar_t *str)
	{
		return unicode_to_utf8(wstring(str));
	}
}
