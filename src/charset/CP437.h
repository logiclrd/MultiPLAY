#ifndef CP437_H
#define CP437_H

#include <string>

namespace CharSet
{
	wchar_t cp437_to_unicode(unsigned char ch);
	std::wstring cp437_to_unicode(std::string str);
	std::wstring cp437_to_unicode(const char *str);
}

#endif // CP437_H