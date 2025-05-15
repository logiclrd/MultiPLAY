#include "effect.h"

#include <iostream>

using namespace std;

#include "pattern.h"

namespace MultiPLAY
{
	// struct effect_info_type
	effect_info_type::operator unsigned char()
	{
		return data;
	}

	effect_info_type &effect_info_type::operator =(unsigned char data)
	{
		this->data = data;
		low_nybble = data & 0xF;
		high_nybble = (data >> 4) & 0xF;
		return *this;
	}

	effect_info_type::effect_info_type()
	{
		data = 0;
		low_nybble = 0;
		high_nybble = 0;
	}

	// struct effect_struct
	effect_struct &effect_struct::operator =(int value)
	{
		if (value < 0)
			present = false;
		else
		{
			present = true;
			command = (value >> 16) & 0xFF;
			info = (value & 0xFF);
			type = EffectType::S3M;
		}
		return *this;
	}

	effect_struct::effect_struct(EffectType::Type type, char command, unsigned char info)
	{
		init(type, command, info);
	}

	void effect_struct::init(EffectType::Type type, char command, unsigned char info, row *r)
	{
		switch (type)
		{
			case EffectType::MOD:
				// This is largely possible due to the correspondence between MOD and
				// S3M effect parameters. Only the 'invert loop' option required
				// special coding.
				switch (command)
				{
					case 0x0: // arpeggio
						if (info)
							command = 'J';
						else
						{
							present = false;
							return; // no further processing required
						}
						break;
					case 0x1: // portamento up
						command = 'F';
						if (info >= 0xE0)
							info = 0xDF;
						break;
					case 0x2: // portamento down
						command = 'E';
						if (info >= 0xE0)
							info = 0xDF;
						break;
					case 0x3: // tone portamento
						command = 'G';
						break;
					case 0x4: // vibrato
						command = 'H';
						break;
					case 0x5: // 300 & Axy
						command = 'L';
						break;
					case 0x6: // 400 & Axy
						command = 'K';
						break;
					case 0x7: // tremolo
						command = 'R';
						break;
					case 0x8: // fine panning (0-255; converted here to 0-128 for Xxx)
						command = 'X';
						info = info * 128 / 255;
						break;
					case 0x9: // set sample offset
						command = 'O';
						break;
					case 0xA: // volume slide
						command = 'D';
						break;
					case 0xB: // pattern/row jump
						command = 'B';
						break;
					case 0xC: // set volume
						if (r)
							r->volume = info;
						else
							cerr << "Warning: had to discard a MOD set volume effect" << endl;

						present = false; // no further processing required
						return;
					case 0xD: // pattern break
						command = 'C';
						break;
					case 0xE:
						switch (info >> 4)
						{
							case 0x0: // set filter
								present = false; // no further processing required
								return;
							case 0x1: // fine slide up
								if (info & 0xF)
								{
									command = 'F';
									info |= 0xF0;
									if ((info & 0xF) == 0xF)
										info = 0xFE;
								}
								else
								{
									present = false; // no further processing required
									return;
								}
								break;
							case 0x2: // fine slide down
								if (info & 0xF)
								{
									command = 'E';
									info |= 0xF;
									if ((info & 0xF) == 0xF)
										info = 0xFE;
								}
								else
								{
									present = false; // no further processing required
									return;
								}
								break;
							case 0x3: // glissando control
								command = 'S';
								info = (info & 0xF) | 0x10;
								break;
							case 0x4: // set vibrato waveform
								command = 'S';
								info = (info & 0xF) | 0x30;
								break;
							case 0x5: // set finetune
								command = 'S';
								info = ((info & 0xF) ^ 8) | 0x20;
								break;
							case 0x6: // loop pattern
								command = 'S';
								info = (info & 0xF) | 0xB0;
								break;
							case 0x7: // set tremolo waveform
								command = 'S';
								info = (info & 0xF) | 0x40;
								break;
							case 0x8: // rough panning
								command = 'S';
								break;
							case 0x9: // retrigger sample
								if (info & 0xF)
								{
									command = 'Q';
									info = info & 0xF;
								}
								else
								{
									present = false; // no further processing required
									return;
								}
								break;
							case 0xA: // fine volume slide up
								command = 'D';
								if ((info & 0xF) == 0xF)
									info = 0xE;
								info = ((info & 0xF) << 4) | 0xF;
								break;
							case 0xB: // fine volume slide down
								command = 'D';
								if ((info & 0xF) == 0xF)
									info = 0xE;
								info = (info & 0xF) | 0xF0;
								break;
							case 0xC: // note cut
								command = 'S';
								break;
							case 0xD: // note delay
								command = 'S';
								break;
							case 0xE: // pattern delay
								command = 'S';
								break;
							case 0xF: // invert loop
								break; // do not change
						}
						break;
					case 0xF: // speed change
						if (info >= 0x20)
							command = 'T';
						else
							command = 'A';
						break;
				}
				type = EffectType::S3M; // translated :-)
				this->command = command;
				this->info = info;
				break;
			case EffectType::S3M:
			case EffectType::IT:
				if ((command < 1) || (command > 26))
				{
					present = false; // no further processing required
					return;
				}
				this->command = command + 64;
				this->info = info;
				break;
			default:
				throw "unimplemented effect type";
		}

		this->type = type;
		present = true;
	}

	effect_struct::effect_struct()
		: present(false)
	{
	}

	bool effect_struct::keepNote()
	{
		if (!present)
			return false;

		switch (type)
		{
			case EffectType::S3M:
			case EffectType::IT:
				switch (command)
				{
					case 'G':
					case 'L':
						return true;
					default:
						return false;
				}
			default:
				throw "internal error";
		}
	}
		
	bool effect_struct::keepVolume()
	{
		if (!present)
			return false;

		switch (type)
		{
			case EffectType::S3M:
			case EffectType::IT:
				switch (command)
				{
					case 'D':
					case 'K':
					case 'L':
						return true;
					default:
						return false;
				}
			default:
				throw "internal error";
		}
	}

	bool effect_struct::isnt(char effect, signed char high_nybble/* = -1*/, signed char low_nybble/* = -1*/)
	{
		if (command != effect)
			return true;
		if ((high_nybble != -1) && (info.high_nybble != (char)high_nybble))
			return true;
		if ((low_nybble != -1) && (info.low_nybble != (char)high_nybble))
			return true;
		return false;
	}

	bool effect_struct::is(char effect, signed char high_nybble/* = -1*/, signed char low_nybble/* = -1*/)
	{
		return !isnt(effect, high_nybble, low_nybble);
	}
}
