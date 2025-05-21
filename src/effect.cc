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

	effect_struct::effect_struct(EffectType::Type type, Effect::Type command, unsigned char high_nybble, unsigned char low_nybble)
	{
		init(type, command, high_nybble, low_nybble);
	}

	effect_struct::effect_struct(EffectType::Type type, Effect::Type command, unsigned char info)
	{
		init(type, command, info);
	}

	void effect_struct::init(MODEffect::Type mod_command, unsigned char info, row *r)
	{
		init(EffectType::MOD, mod_command, info, r);
	}

	void effect_struct::init(EffectType::Type type, MODEffect::Type mod_command, unsigned char info, row *r)
	{
		this->type = type;

		// This is largely possible due to the correspondence between MOD and
		// S3M effect parameters. Only the 'invert loop' option required
		// special coding.
		switch (mod_command)
		{
			case MODEffect::None:
				present = false;
				return;

			case MODEffect::Arpeggio: // 0x0
				if (info)
					command = Effect::Arpeggio;
				else
				{
					present = false;
					return; // no further processing required
				}
				break;
			case MODEffect::PortamentoUp: // 0x1
				command = Effect::PortamentoUp;
				if (info >= 0xE0)
					info = 0xDF;
				break;
			case MODEffect::PortamentoDown: // 0x2
				command = Effect::PortamentoDown;
				if (info >= 0xE0)
					info = 0xDF;
				break;
			case MODEffect::TonePortamento: // 0x3
				command = Effect::TonePortamento;
				break;
			case MODEffect::Vibrato: // 0x4
				command = Effect::Vibrato;
				break;
			case MODEffect::TonePortamentoAndVolumeSlide: // 0x5
				command = Effect::TonePortamentoAndVolumeSlide;
				break;
			case MODEffect::VibratoAndVolumeSlide: // 0x6
				command = Effect::VibratoAndVolumeSlide;
				break;
			case MODEffect::Tremolo: // 0x7
				command = Effect::Tremolo;
				break;
			case MODEffect::FinePanning: // 0x8, fine panning (0-255; converted here to 0-128 for Xxx)
				command = Effect::Panning;
				info = info * 128 / 255;
				break;
			case MODEffect::SampleOffset: // 0x9
				command = Effect::SampleOffset;
				break;
			case MODEffect::VolumeSlide: // 0xA
				command = Effect::VolumeSlide;
				break;
			case MODEffect::OrderJump: // 0xB
				command = Effect::OrderJump;
				break;
			case MODEffect::SetVolume: // 0xC
				if (r)
					r->volume = info;
				else
					cerr << "Warning: had to discard a MOD set volume effect" << endl;

				present = false; // no further processing required
				return;
			case MODEffect::PatternJump: // 0xD
				command = Effect::PatternJump;
				break;
			case MODEffect::Extended: // 0xE
				switch (info >> 4)
				{
					case MODExtendedEffect::SetFilter: // 0x0
						present = false; // no further processing required
						return;
					case MODExtendedEffect::FinePortamentoUp: // 0x1
						if (info & 0xF)
						{
							command = Effect::PortamentoUp;
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
					case MODExtendedEffect::FinePortamentoDown: // 0x2
						if (info & 0xF)
						{
							command = Effect::PortamentoDown;
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
					case MODExtendedEffect::GlissandoControl: // 0x3
						command = Effect::ExtendedEffect;
						info = (info & 0xF) | (ExtendedEffect::GlissandoControl << 4);
						break;
					case MODExtendedEffect::SetVibratoWaveform: // 0x4
						command = Effect::ExtendedEffect;
						info = (info & 0xF) | (ExtendedEffect::SetVibratoWaveform << 4);
						break;
					case MODExtendedEffect::SetFineTune: // 0x5
						command = Effect::ExtendedEffect;
						info = ((info & 0xF) ^ 8) | (ExtendedEffect::SetFineTune << 4);
						break;
					case MODExtendedEffect::PatternLoop: // 0x6
						command = Effect::ExtendedEffect;
						info = (info & 0xF) | (ExtendedEffect::PatternLoop << 4);
						break;
					case MODExtendedEffect::SetTremoloWaveform: // 0x7
						command = Effect::ExtendedEffect;
						info = (info & 0xF) | (ExtendedEffect::SetTremoloWaveform << 4);
						break;
					case MODExtendedEffect::RoughPanning: // 0x8
						command = Effect::ExtendedEffect;
						// No further conversion needed, because this is also S3M extended effect 0x8.
						break;
					case MODExtendedEffect::Retrigger: // 0x9
						if (info & 0xF)
						{
							command = Effect::Retrigger;
							info = info & 0xF;
						}
						else
						{
							present = false; // no further processing required
							return;
						}
						break;
					case MODExtendedEffect::FineVolumeSlideUp: // 0xA
						command = Effect::VolumeSlide;
						if ((info & 0xF) == 0xF)
							info = 0xE;
						info = ((info & 0xF) << 4) | 0xF;
						break;
					case MODExtendedEffect::FineVolumeSlideDown: // 0xB
						command = Effect::VolumeSlide;
						if ((info & 0xF) == 0xF)
							info = 0xE;
						info = (info & 0xF) | 0xF0;
						break;
					case MODExtendedEffect::NoteCut: // 0xC
						command = Effect::ExtendedEffect;
						break;
					case MODExtendedEffect::NoteDelay: // 0xD
						command = Effect::ExtendedEffect;
						break;
					case MODExtendedEffect::PatternDelay: // 0xE
						command = Effect::ExtendedEffect;
						break;
					case 0xF: // invert loop
						break; // do not change
				}
				break;
			case MODEffect::SetSpeed: // speed change
				if (info >= 0x20)
					command = Effect::Tempo;
				else
					command = Effect::SetSpeed;
				break;
		}

		this->type = EffectType::S3M; // translated :-)
		this->info = (Effect::Type)info;

		present = true;
	}

	void effect_struct::init(EffectType::Type type, Effect::Type command, unsigned char high_nybble, unsigned char low_nybble, row *r)
	{
		init(type, command, (high_nybble << 4) | (low_nybble & 0xF), r);
	}

	void effect_struct::init(EffectType::Type type, Effect::Type command, unsigned char info, row *r)
	{
		switch (type)
		{
			case EffectType::MOD:
				init((MODEffect::Type)command, info, r);
				break;
			case EffectType::S3M:
			case EffectType::IT:
				if ((command < 1) || (command > 26))
				{
					present = false; // no further processing required
					return;
				}
				this->command = command + 64;
				this->info = (Effect::Type)info;
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
		command = 0;
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

	bool effect_struct::isnt(Effect::Type effect, signed char high_nybble/* = -1*/, signed char low_nybble/* = -1*/)
	{
		if (!present)
			return true;

		if (command != effect)
			return true;
		if ((high_nybble != -1) && (info.high_nybble != (unsigned char)high_nybble))
			return true;
		if ((low_nybble != -1) && (info.low_nybble != (unsigned char)high_nybble))
			return true;
		return false;
	}

	bool effect_struct::is(Effect::Type effect, signed char high_nybble/* = -1*/, signed char low_nybble/* = -1*/)
	{
		return !isnt(effect, high_nybble, low_nybble);
	}
}
