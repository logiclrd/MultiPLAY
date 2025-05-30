#include "Load_S3M.h"

#include <iostream>
#include <cstring>

using namespace std;

#include "RAII.h"

using namespace RAII;

#include "sample_builtintype.h"
#include "conversion.h"
#include "module.h"
#include "string.h"
#include "notes.h"

namespace MultiPLAY
{
	namespace
	{
		struct s3m_flags
		{
			char flags;

			bool st2vibrato()  { return (0 != (flags &   1)); }
			bool st2tempo()    { return (0 != (flags &   2)); }
			bool amigaslides() { return (0 != (flags &   4)); }
			bool zerovol()     { return (0 != (flags &   8)); }
			bool amigalimits() { return (0 != (flags &  16)); }
			bool sbfiltersfx() { return (0 != (flags &  32)); }
			bool reserved()    { return (0 != (flags &  64)); }
			bool customdata()  { return (0 != (flags & 128)); }
		};

		struct s3m_created_with_tracker
		{
			short value;

			int minorversion()
			{
				return static_cast<unsigned short>(value) & 255;
			}

			int majorversion()
			{
				return (static_cast<unsigned short>(value) >> 8) & 15;
			}

			int tracker()
			{
				return (static_cast<unsigned short>(value) >> 12);
			}
		};

		struct s3m_settings
		{
			unsigned char global_volume;
			unsigned char initial_speed;
			unsigned char initial_tempo;
			unsigned char master_volume;
		};

		struct s3m_channel_description
		{
			char value;

			unsigned int channel_type()
			{
				return static_cast<unsigned char>(value) & 127;
			}

			bool disabled()
			{
				return (0 != (value & 128));
			}
		};

		struct s3m_channel_default_pan
		{
			char value;

			unsigned int default_pan()
			{
				return static_cast<unsigned int>(value) & 15;
			}

			bool default_specified()
			{
				return (0 != (value & 32));
			}

			bool default_specified(bool specified)
			{
				value = (value & ~32) | (specified ? 32 : 0);
				return specified;
			}
		};

		struct s3m_sample_flags
		{
			char value;

			bool loop()
			{
				return (0 != (value & 1));
			}

			bool stereo()
			{
				return (0 != (value & 2));
			}

			bool sixteen_bit()
			{
				return (0 != (value & 4));
			}
		};
		namespace S3MEffect
		{
			enum Type : unsigned char
			{
				SetSpeed                     =  1,
				OrderJump                    =  2,
				PatternJump                  =  3,
				VolumeSlide                  =  4,
				PortamentoDown               =  5,
				PortamentoUp                 =  6,
				TonePortamento               =  7,
				Vibrato                      =  8,
				Tremor                       =  9,
				Arpeggio                     = 10,
				VibratoAndVolumeSlide        = 11,
				TonePortamentoAndVolumeSlide = 12,
				ChannelVolume                = 13,
				ChannelVolumeSlide           = 14,
				SampleOffset                 = 15,
				PanSlide                     = 16,
				Retrigger                    = 17,
				Tremolo                      = 18,
				ExtendedEffect               = 19,
				Tempo                        = 20,
				FineVibrato                  = 21,
				GlobalVolume                 = 22,
				GlobalVolumeSlide            = 23,
				Panning                      = 24,
				Panbrello                    = 25,
				MIDIMacros                   = 26,
			};
		}

		Effect::Type translate_s3m_effect_command(S3MEffect::Type s3m_command)
		{
			switch (s3m_command)
			{
				case S3MEffect::SetSpeed: return Effect::SetSpeed;
				case S3MEffect::OrderJump: return Effect::OrderJump;
				case S3MEffect::PatternJump: return Effect::PatternJump;
				case S3MEffect::VolumeSlide: return Effect::VolumeSlide;
				case S3MEffect::PortamentoDown: return Effect::PortamentoDown;
				case S3MEffect::PortamentoUp: return Effect::PortamentoUp;
				case S3MEffect::TonePortamento: return Effect::TonePortamento;
				case S3MEffect::Vibrato: return Effect::Vibrato;
				case S3MEffect::Tremor: return Effect::Tremor;
				case S3MEffect::Arpeggio: return Effect::Arpeggio;
				case S3MEffect::VibratoAndVolumeSlide: return Effect::VibratoAndVolumeSlide;
				case S3MEffect::TonePortamentoAndVolumeSlide: return Effect::TonePortamentoAndVolumeSlide;
				case S3MEffect::ChannelVolume: return Effect::ChannelVolume;
				case S3MEffect::ChannelVolumeSlide: return Effect::ChannelVolumeSlide;
				case S3MEffect::SampleOffset: return Effect::SampleOffset;
				case S3MEffect::PanSlide: return Effect::PanSlide;
				case S3MEffect::Retrigger: return Effect::Retrigger;
				case S3MEffect::Tremolo: return Effect::Tremolo;
				case S3MEffect::ExtendedEffect: return Effect::ExtendedEffect;
				case S3MEffect::Tempo: return Effect::Tempo;
				case S3MEffect::FineVibrato: return Effect::FineVibrato;
				case S3MEffect::GlobalVolume: return Effect::GlobalVolume;
				case S3MEffect::GlobalVolumeSlide: return Effect::GlobalVolumeSlide;
				case S3MEffect::Panning: return Effect::Panning;
				case S3MEffect::Panbrello: return Effect::Panbrello;

				default:
					return Effect::None;
			}
		}
	}

	module_struct *load_s3m(istream *file)
	{
		int file_base_offset = (int)file->tellg();

		unsigned char lsb_bytes[4];

		char songname[29];
		songname[28] = 0;
		file->read(songname, 28);

		int sig = file->get();

		if (sig != 0x1A)
			cerr << "Warning: missing 1A signature at byte 28" << endl;
			//throw "invalid file format";

		int type = file->get();

		if (type != 16)
			throw "unknown file type";

		file->ignore(2);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned pattern_list_length = from_lsb2_u(lsb_bytes);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned num_samples = from_lsb2_u(lsb_bytes);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned num_patterns = from_lsb2_u(lsb_bytes);

		s3m_flags flags;
		file->read((char *)&flags.flags, 1);
		file->ignore(1);

		s3m_created_with_tracker cwt;

		file->read((char *)&lsb_bytes[0], 2);
		cwt.value = lsb_bytes[0] | (lsb_bytes[1] << 8);

		short signed_samples;
		file->read((char *)&lsb_bytes[0], 2);
		signed_samples = lsb_bytes[0] | (lsb_bytes[1] << 8);
		bool unsigned_samples = (signed_samples == 2);

		char magic[4];
		file->read(magic, 4);
		
		if (string(magic, 4) != "SCRM")
			throw "invalid file format (missing 'SCRM' near start of file)";

		s3m_settings settings;

		file->read((char *)&settings.global_volume, 1);
		file->read((char *)&settings.initial_speed, 1);
		file->read((char *)&settings.initial_tempo, 1);
		file->read((char *)&settings.master_volume, 1);

		file->ignore(1); // ultraclick removal
		char default_panning = char(file->get());
		file->ignore(8);

	/*  short special_ptr;
		file->read((char *)&special_ptr, 2); /* */
		file->ignore(2);

		if (int(file->tellg()) != 64)
			throw "internal error: not at the right place in the file";

		s3m_channel_description file_channels[32];
		for (int i=0; i<32; i++)
			file_channels[i].value = char(file->get());

		//if (pattern_list_length & 1)
		//  throw "pattern list length is not even";

		ArrayAllocator<unsigned char> pattern_list_index_allocator(pattern_list_length);
		unsigned char *pattern_list_index = pattern_list_index_allocator.getArray();
		file->read((char *)pattern_list_index, pattern_list_length);

		ArrayAllocator<unsigned short> sample_parapointer_allocator(num_samples);
		unsigned short *sample_parapointer = sample_parapointer_allocator.getArray();
		for (unsigned i=0; i<num_samples; i++)
		{
			file->read((char *)&lsb_bytes[0], 2);
			sample_parapointer[i] = from_lsb2_u(lsb_bytes);
		}

		ArrayAllocator<unsigned short> pattern_parapointer_allocator(num_patterns);
		unsigned short *pattern_parapointer = pattern_parapointer_allocator.getArray();
		for (unsigned i=0; i<num_patterns; i++)
		{
			file->read((char *)&lsb_bytes[0], 2);
			pattern_parapointer[i] = from_lsb2_u(lsb_bytes);
		}

		s3m_channel_default_pan default_panning_value[32];
	#pragma pack()

		if ((unsigned char)default_panning == 0xFC)
			for (unsigned i=0; i<32; i++)
				default_panning_value[i].value = char(file->get());
		else
			for (unsigned i=0; i<32; i++)
				default_panning_value[i].default_specified(false);

		vector<sample *> samps;

		for (unsigned i=0; i<num_samples; i++)
		{
			file->seekg(file_base_offset + sample_parapointer[i] * 16, ios::beg);

			int sample_type = file->get();

			if (sample_type != 1)
			{
				samps.push_back(new sample_builtintype<char>(int(i), 1, 1.0));
				continue;
			}

			char dos_filename[13];
			dos_filename[12] = 0;
			file->read(dos_filename, 12);

			file->ignore(1);

			file->read((char *)&lsb_bytes[0], 2);
			int parapointer = (int)from_lsb2_u(lsb_bytes);

			unsigned long length, loop_begin, loop_end;

			file->read((char *)&lsb_bytes[0], 4);
			length = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_begin = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_end = from_lsb4_lu(lsb_bytes);

			char volume = char(file->get());

			file->ignore(1);

			bool packed;
			packed = (file->get() == 1);

			s3m_sample_flags sample_flags;
			file->read((char *)&sample_flags.value, 1);

			file->read((char *)&lsb_bytes[0], 4);
			unsigned long c2spd = from_lsb4_lu(lsb_bytes);

			file->ignore(4);

			file->ignore(2); // gravis ultrasound ptr
			file->ignore(2); // soundblaster loop expansion
			file->ignore(4); // soundblaster 'last used' position

			char sample_name[28];
			file->read(sample_name, 28);

			char sample_magic[4];
			file->read(sample_magic, 4);

			if (string(sample_magic, 4) != "SCRS")
				throw "invalid sample format";

			file->seekg(file_base_offset + parapointer * 16, ios::beg);

			/*if (sample_flags.stereo())
				length >>= 1; /* */

			if (sample_flags.sixteen_bit())
			{
				//length >>= 1;
				if (unsigned_samples)
				{
					unsigned short *data = new unsigned short[length];
					unsigned short *data_right = data;
					
					if (sample_flags.stereo())
					{
						data_right = new unsigned short[length];

						for (unsigned int j=0; j<length; j++)
						{
							file->read((char *)&data[j], 2);
							file->read((char *)&data_right[j], 2);
						}
					}
					else
						file->read((char *)data, streamsize(2 * length));

					unsigned char *uc_data = (unsigned char *)&data[0];
					unsigned char *uc_data_right = nullptr;
					if (sample_flags.stereo())
						uc_data_right = (unsigned char *)&data_right[0];

					for (unsigned int j=0; j<length; j++)
					{
						data[j] = from_lsb2_u(uc_data + 2*j);
						if (sample_flags.stereo())
							data_right[j] = from_lsb2_u(uc_data_right + 2*j);
					}

					signed short *data_sgn = (signed short *)data;
					signed short *data_right_sgn = (signed short *)data_right;

					if (sample_flags.stereo())
						for (unsigned int sample_offset=0; sample_offset<length; sample_offset++)
						{
							data_sgn[sample_offset] = (short)(int(data[sample_offset]) - 32768);
							data_right_sgn[sample_offset] = (short)(int(data_right[sample_offset]) - 32768);
						}
					else
						for (unsigned int sample_offset=0; sample_offset<length; sample_offset++)
							data_sgn[sample_offset] = (short)(int(data[sample_offset]) - 32768);

					sample_builtintype<signed short> *smp = new sample_builtintype<signed short>(int(i), sample_flags.stereo() ? 2 : 1, volume / 64.0);

					ASSIGN_STRING_FROM_STRUCTURE_FIELD(
						smp->name,
						sample_name);

					smp->num_samples = length;
					smp->sample_data[0] = data_sgn;
					if (sample_flags.stereo())
						smp->sample_data[1] = data_right_sgn;
					smp->samples_per_second = c2spd;
					if (sample_flags.loop())
					{
						smp->loop_begin = loop_begin;
						smp->loop_end = loop_end - 1;
					}

					samps.push_back(smp);
				}
				else
				{
					signed short *data = new signed short[length];
					signed short *data_right = data;

					if (sample_flags.stereo())
					{
						data_right = new signed short[length];

						for (unsigned int j=0; j<length; j++)
						{
							file->read((char *)&data[j], 2);
							file->read((char *)&data_right[j], 2);
						}
					}
					else
						file->read((char *)data, streamsize(2 * length));

					unsigned char *uc_data = (unsigned char *)&data[0];
					unsigned char *uc_data_right = nullptr;
					if (sample_flags.stereo())
						uc_data_right = (unsigned char *)&data_right[0];

					for (unsigned int j=0; j<length; j++)
					{
						data[j] = from_lsb2(uc_data + 2*j);
						if (sample_flags.stereo())
							data_right[j] = from_lsb2(uc_data_right + 2*j);
					}

					sample_builtintype<signed short> *smp = new sample_builtintype<signed short>(int(i), sample_flags.stereo() ? 2 : 1, volume / 64.0);

					ASSIGN_STRING_FROM_STRUCTURE_FIELD(
						smp->name,
						sample_name);

					smp->num_samples = length;
					smp->sample_data[0] = data;
					if (sample_flags.stereo())
						smp->sample_data[1] = data_right;
					smp->samples_per_second = c2spd;
					if (sample_flags.loop())
					{
						smp->loop_begin = loop_begin;
						smp->loop_end = loop_end - 1;
					}

					samps.push_back(smp);
				}
			}
			else
			{
				if (unsigned_samples)
				{
					unsigned char *data = new unsigned char[length];
					unsigned char *data_right = data;

					if (sample_flags.stereo())
					{
						data_right = new unsigned char[length];

						for (unsigned int j=0; j<length; j++)
						{
							file->read((char *)&data[j], 1);
							file->read((char *)&data_right[j], 1);
						}
					}
					else
						file->read((char *)data, streamsize(length));

					signed char *data_sgn = (signed char *)data;
					signed char *data_right_sgn = (signed char *)data_right;

					if (sample_flags.stereo())
						for (unsigned int sample_offset=0; sample_offset<length; sample_offset++)
						{
							data_sgn[sample_offset] = (char)(int(data[sample_offset]) - 128);
							data_right_sgn[sample_offset] = (char)(int(data_right[sample_offset]) - 128);
						}
					else
						for (unsigned int sample_offset=0; sample_offset<length; sample_offset++)
							data_sgn[sample_offset] = (char)(int(data[sample_offset]) - 128);

					sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(int(i), sample_flags.stereo() ? 2 : 1, volume / 64.0);

					ASSIGN_STRING_FROM_STRUCTURE_FIELD(
						smp->name,
						sample_name);

					smp->num_samples = length;
					smp->sample_data[0] = data_sgn;
					if (sample_flags.stereo())
						smp->sample_data[1] = data_right_sgn;
					smp->samples_per_second = c2spd;
					if (sample_flags.loop())
					{
						smp->loop_begin = loop_begin;
						smp->loop_end = loop_end - 1;
					}

					samps.push_back(smp);
				}
				else
				{
					signed char *data = new signed char[length];
					signed char *data_right = data;

					if (sample_flags.stereo())
					{
						data_right = new signed char[length];

						for (unsigned int j=0; j<length; j++)
						{
							file->read((char *)&data[j], 1);
							file->read((char *)&data_right[j], 1);
						}
					}
					else
						file->read((char *)data, streamsize(length));

					sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(int(i), sample_flags.stereo() ? 2 : 1, volume / 64.0);

					ASSIGN_STRING_FROM_STRUCTURE_FIELD(
						smp->name,
						sample_name);

					smp->num_samples = length;
					smp->sample_data[0] = data;
					if (sample_flags.stereo())
						smp->sample_data[1] = data_right;
					smp->samples_per_second = c2spd;
					if (sample_flags.loop())
					{
						smp->loop_begin = loop_begin;
						smp->loop_end = loop_end - 1;
					}

					samps.push_back(smp);
				}
			}
		}

		vector<pattern> pats;

		for (unsigned i=0; i<num_patterns; i++)
		{
			file->seekg(file_base_offset + pattern_parapointer[i] * 16, ios::beg);

			file->read((char *)&lsb_bytes[0], 2);
			unsigned short pattern_data_length = from_lsb2_u(lsb_bytes);

			int bytes_left = pattern_data_length;

			pattern new_pattern((int)i);

			for (int row_number=0; row_number<64; row_number++)
			{
				vector<row> rowdata(32);

				while (true)
				{
					char what = char(file->get()); if (!--bytes_left) break;

					if (what == 0)
						break;

					unsigned channel = (what & 31);

					if (what & 32)
					{
						int snote = file->get(); if (!--bytes_left) break;
						if (snote > 127)
							snote -= 256;
						rowdata[channel].snote = snote;
						rowdata[channel].znote = znote_from_snote(snote);

						unsigned instrument = unsigned(file->get());
						if ((instrument > 0) && (instrument <= samps.size()))
							rowdata[channel].instrument = samps[instrument - 1];
						else
							rowdata[channel].instrument = nullptr;
						if (!--bytes_left) break;
					}

					if (what & 64)
					{
						rowdata[channel].volume = file->get(); if (!--bytes_left) break;
					}

					if (what & 128)
					{
						auto s3m_command = (S3MEffect::Type)file->get(); if (!--bytes_left) break;
						int info = file->get(); if (!--bytes_left) break;

						auto command = translate_s3m_effect_command(s3m_command);

						if (command != Effect::None)
							rowdata[channel].effect = effect_struct(EffectType::S3M, command, (unsigned char)info);
					}
				}

				new_pattern.row_list.push_back(rowdata);

				if (!bytes_left)
					break;
			}
			pats.push_back(new_pattern);
		}

		module_struct *ret = new module_struct();

		ASSIGN_STRING_FROM_STRUCTURE_FIELD(
			ret->name,
			songname);

		ret->stereo = ((settings.master_volume & 128) != 0);

		for (size_t i=0; i < samps.size(); i++)
			ret->information_text.push_back(samps[i]->name);

		ret->patterns = pats;
		ret->samples = samps;

		for (unsigned i=0; i<pattern_list_length; i++)
		{
			if (pattern_list_index[i] == 254) //  ++ (skip)
				ret->pattern_list.push_back(&pattern::skip_marker);
			else if (pattern_list_index[i] == 255) //  -- (end of tune)
				ret->pattern_list.push_back(&pattern::end_marker);
			else
				ret->pattern_list.push_back(&ret->patterns[pattern_list_index[i]]);
		}

		memset(ret->base_pan, 0, sizeof(ret->base_pan));

		{
			unsigned i, j;

			for (i=0, j=0; i<32; i++)
			{
				ret->channel_enabled[i] = !file_channels[i].disabled();
				if (ret->channel_enabled[i])
					ret->channel_map[i] = j++;
				if (ret->stereo)
				{
					ret->initial_panning[i].set_channels(2);
					if ((unsigned char)default_panning == 0xFC)
					{
						if (default_panning_value[i].default_specified())
							ret->base_pan[i] = (int)default_panning_value[i].default_pan();
						else
							ret->base_pan[i] = (i & 1) ? 0xC : 0x3;
						ret->initial_panning[i].from_s3m_pan(ret->base_pan[i]);
					}
					else
						switch (file_channels[i].channel_type())
						{
							case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
								ret->base_pan[i] = 0x3;
								ret->initial_panning[i].from_s3m_pan(0x3);
								break;
							case 0x8: case 0x9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE: case 0xF:
								ret->base_pan[i] = 0xC;
								ret->initial_panning[i].from_s3m_pan(0xC);
								break;
						}
				}
			}

			for (i=32; i<MAX_MODULE_CHANNELS; i++)
				ret->channel_enabled[i] = false;

			ret->num_channels = j;
		}

		ret->speed = settings.initial_speed;
		ret->tempo = settings.initial_tempo;

		return ret;
	}
}