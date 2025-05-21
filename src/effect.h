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
			XM,
			IT,
		};

		inline const char *ToString(Type type)
		{
			switch (type)
			{
				case MOD: return "MOD";
				case S3M: return "S3M";
				case XM: return "XM";
				case IT: return "IT";

				default: return "<unknown type>";
			}
		}
	}

	namespace Effect
	{
		enum Type : unsigned char
		{
			None = 0,

			MODExtraEffects = 0xE,

			// Global effects, processed up-front
			SetSpeed = 'A',
			OrderJump = 'B',
			PatternJump = 'C',
			Tempo = 'T',
			GlobalVolume = 'V',
			GlobalVolumeSlide = 'W',

			// Channel effects
			VolumeSlide = 'D',
			PortamentoDown = 'E',
			PortamentoUp = 'F',
			TonePortamento = 'G',
			Vibrato = 'H',
			SetVibratoSpeed = 'h', // For XM volume column command
			Tremor = 'I',
			Arpeggio = 'J',
			VibratoAndVolumeSlide = 'K',
			TonePortamentoAndVolumeSlide = 'L',
			ChannelVolume = 'M',
			ChannelVolumeSlide = 'N',
			SampleOffset = 'O',
			PanSlide = 'P',
			Retrigger = 'Q',
			Tremolo = 'R',
			ExtendedEffect = 'S',
			FineVibrato = 'U',
			Panning = 'X',
			Panbrello = 'Y',
			SetEnvelopePosition = 'Z', // XM
		};
	}

	namespace ExtendedEffect
	{
		enum Type : unsigned
		{
			GlissandoControl = 0x1,
			SetFineTune = 0x2,
			SetVibratoWaveform = 0x3,
			SetTremoloWaveform = 0x4,
			SetPanbrelloWaveform = 0x5,
			FinePatternDelay = 0x6,
			OverrideNewNoteAction = 0x7,
			RoughPanning = 0x8,
			ExtendedITEffect = 0x9,
			Panning = 0xA,
			PatternLoop = 0xB,
			NoteCut = 0xC,
			NoteDelay = 0xD,
			PatternDelay = 0xE,
		};
	}

	namespace MODEffect
	{
		enum Type : unsigned char
		{
			Arpeggio                     = 0x0,
			PortamentoUp                 = 0x1,
			PortamentoDown               = 0x2,
			TonePortamento               = 0x3,
			Vibrato                      = 0x4,
			TonePortamentoAndVolumeSlide = 0x5,
			VibratoAndVolumeSlide        = 0x6,
			Tremolo                      = 0x7,
			FinePanning                  = 0x8,
			SampleOffset                 = 0x9,
			VolumeSlide                  = 0xA,
			OrderJump                    = 0xB,
			SetVolume                    = 0xC,
			PatternJump                  = 0xD,
			Extended                     = 0xE,
			SetSpeed                     = 0xF,

			None = 0xFF,
		};
	}

	namespace MODExtendedEffect
	{
		enum Type : unsigned char
		{
			SetFilter           = 0x0,
			FinePortamentoUp    = 0x1,
			FinePortamentoDown  = 0x2,
			GlissandoControl    = 0x3,
			SetVibratoWaveform  = 0x4,
			SetFineTune         = 0x5,
			PatternLoop         = 0x6,
			SetTremoloWaveform  = 0x7,
			RoughPanning        = 0x8,
			Retrigger           = 0x9,
			FineVolumeSlideUp   = 0xA,
			FineVolumeSlideDown = 0xB,
			NoteCut             = 0xC,
			NoteDelay           = 0xD,
			PatternDelay        = 0xE,
			InvertLoop          = 0xF,
		};
	}

	struct effect_info_type
	{
		unsigned int low_nybble  : 4;
		unsigned int high_nybble : 4;
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
		effect_struct(EffectType::Type type, unsigned char raw_command, unsigned char info);
		effect_struct(EffectType::Type type, Effect::Type command, unsigned char info);
		effect_struct(EffectType::Type type, Effect::Type command, unsigned char high_nybble, unsigned char low_nybble);

		void init(EffectType::Type type, unsigned char raw_command, unsigned char info, row *r = NULL);
		void init(EffectType::Type type, Effect::Type command, unsigned char info, row *r = NULL);
		void init(EffectType::Type type, Effect::Type command, unsigned char high_nybble, unsigned char low_nybble, row *r = NULL);
		void init(EffectType::Type type, MODEffect::Type command, unsigned char info, row *r = NULL);
		void init(MODEffect::Type command, unsigned char info, row *r = NULL);

		bool keepNote();
		bool keepVolume();
		bool isnt(Effect::Type effect, signed char high_nybble = -1, signed char low_nybble = -1);
		bool is(Effect::Type effect, signed char high_nybble = -1, signed char low_nybble = -1);

		effect_struct &operator =(int value);
	};
}

#endif // EFFECT_H
