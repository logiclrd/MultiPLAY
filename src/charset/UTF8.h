#ifndef UTF8_H
#define UTF8_H

#include <string>

namespace CharSet
{
	std::wstring utf8_to_unicode(std::string str);
	std::wstring utf8_to_unicode(const char *str);

	std::string unicode_to_utf8(std::wstring str);
	std::string unicode_to_utf8(const wchar_t *str);
}

#endif // UTF8_H