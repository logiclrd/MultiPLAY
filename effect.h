#ifndef EFFECT_H
#define EFFECT_H

#include <cstddef>

namespace MultiPLAY
{
	namespace EffectType
	{
		enum Type
		{
			MOD,
			S3M,
			IT
		};
	}

	struct effect_info_type
	{
		struct // anonymous structs like this are not valid ISO C++, but both VC++ and g++ support them
		{
			unsigned int low_nybble  : 4;
			unsigned int high_nybble : 4;
		};
		unsigned char data;

		effect_info_type();

		operator unsigned char();
		effect_info_type &operator =(unsigned char data);
	};

	struct row;

	struct effect_struct
	{
		bool present;
		char command;
		effect_info_type info;
		EffectType::Type type;

		effect_struct();
		effect_struct(EffectType::Type type, char command, unsigned char info);

		void init(EffectType::Type type, char command, unsigned char info, row *r = NULL);

		bool keepNote();
		bool keepVolume();
		bool isnt(char effect, signed char high_nybble = -1, signed char low_nybble = -1);
		bool is(char effect, signed char high_nybble = -1, signed char low_nybble = -1);

		effect_struct &operator =(int value);
	};
}

#endif // EFFECT_H
