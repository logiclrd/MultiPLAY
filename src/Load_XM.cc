#include "Load_XM.h"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <new>

using namespace std;

#include "sample_builtintype.h"
#include "sample_instrument.h"
#include "mod_finetune.h"
#include "module.h"
#include "notes.h"

namespace MultiPLAY
{
	namespace
	{
		#pragma pack(1)
		struct xm_preamble
		{
			char id_text[17]; // nominally "Extended module: ", but not always present
			char module_name[20];
			char escape; // nominally 0x1A, except in stripped modules where it is 0x00
			char tracker_name[20]; // sometimes identifies the software, sometimes identifies the person, sometimes isn't used
			unsigned char version_major;
			unsigned char version_minor;
		};

		struct xm_header
		{
			int header_length;
			unsigned short pattern_list_length; // order table size in bytes
			short restart_position; // zero-based index into the pattern list for looping
			unsigned short num_channels;
			unsigned short num_patterns;
			unsigned short num_instruments;
			short frequency_table; // 0 == amiga, 1 == linear
			short initial_speed;
			short initial_tempo;
			unsigned char pattern_list[256];
		};

		struct xm_pattern_header
		{
			int header_length;
			char packing_type; // not actually used
			unsigned short pattern_rows;
			short packed_data_length; // can't be trusted, if it's not zero then don't actually use the value, just decode until end is reached
		};

		#define XM_NOTE_OFF 97

		int snote_from_xnote(int xm_note)
		{
			if (xm_note == XM_NOTE_OFF)
				return SNOTE_NOTE_OFF;
			else
			{
				// xm_note: 1 == C-1, 13 == C-2, 25 == C-3, 37 == C-4
				// znote: 39 == middle C
				int znote = xm_note - 10;

				return snote_from_znote(znote);
			}
		}

		namespace XMNoteCommand
		{
			enum Type
			{
				None,

				VolumeSet,
				VolumeSlideDown,
				VolumeSlideUp,
				VolumeFineSlideDown,
				VolumeFineSlideUp,
				VibratoSpeed,
				VibratoDepth,
				Pan,
				PanSlideLeft,
				PanSlideRight,
				TonePortamento,
			};
		}

		namespace XMNoteEffect
		{
			enum Type : unsigned char
			{
				Arpeggio = 0,
				PortaUp = 1,
				PortaDown = 2,
				TonePortamento = 3,
				Vibrato = 4,
				TonePortamentoAndVolumeSlide = 5,
				VibratoAndVolumeSlide = 6,
				Tremolo = 7,
				Pan = 8,
				SampleOffset = 9,
				VolumeSlide = 10,
				PositionJump = 11,
				SetChannelVolume = 12,
				PatternBreak = 13,
				MODExtendedEffect = 14,
				SetSpeedAndTempo = 15,
				SetGlobalVolume = 16,
				GlobalVolumeSlide = 17,
				KeyOff = 20,
				SetEnvelopePosition = 21,
				PanningSlide = 25,
				MultiRetrigger = 27,
				Tremor = 29,
				ExtraFinePortamento = 33,
			};

			MODEffect::Type ConvertToMODEffect(XMNoteEffect::Type effect)
			{
				if (effect <= 15)
					return (MODEffect::Type)effect;
				else
					return MODEffect::None;
			}
		}

		struct xm_pattern_note
		{
			char note;
			unsigned char instrument;
			XMNoteCommand::Type command;
			unsigned char command_parameter;
			XMNoteEffect::Type effect_type;
			unsigned char effect_parameter;

			void parse_command(unsigned char command_byte)
			{
				if (command_byte < 0x10)
					command = XMNoteCommand::None;
				else if (command_byte <= 0x50)
				{
					command = XMNoteCommand::VolumeSet;
					command_parameter = command_byte - 0x10;
				}
				else if (command_byte < 0x60)
					command = XMNoteCommand::None;
				else if (command_byte < 0x70)
				{
					command = XMNoteCommand::VolumeSlideDown;
					command_parameter = command_byte - 0x60;
				}
				else if (command_byte < 0x80)
				{
					command = XMNoteCommand::VolumeSlideUp;
					command_parameter = command_byte - 0x70;
				}
				else if (command_byte < 0x90)
				{
					command = XMNoteCommand::VolumeFineSlideDown;
					command_parameter = command_byte - 0x80;
				}
				else if (command_byte < 0xA0)
				{
					command = XMNoteCommand::VolumeFineSlideUp;
					command_parameter = command_byte - 0x90;
				}
				else if (command_byte < 0xB0)
				{
					command = XMNoteCommand::VibratoSpeed;
					command_parameter = command_byte - 0xA0;
				}
				else if (command_byte < 0xC0)
				{
					command = XMNoteCommand::VibratoDepth;
					command_parameter = command_byte - 0xB0;
				}
				else if (command_byte < 0xD0)
				{
					command = XMNoteCommand::Pan;
					command_parameter = (command_byte - 0xC0) * 4 + 2;
				}
				else if (command_byte < 0xE0)
				{
					command = XMNoteCommand::PanSlideLeft;
					command_parameter = command_byte - 0xD0;
				}
				else if (command_byte < 0xF0)
				{
					command = XMNoteCommand::PanSlideRight;
					command_parameter = command_byte - 0xE0;
				}
				else
				{
					command = XMNoteCommand::TonePortamento;
					command_parameter = command_byte - 0xF0;
				}
			}

			void decode_and_fill(istream *src)
			{
				int first_byte = src->get();

				if (first_byte < 0)
					throw "Does not appear to be an XM file";

				if ((first_byte & 0x80) == 0)
				{
					// Unpacked; first_byte is the note.
					this->note = (char)first_byte;
					this->instrument = (unsigned char)src->get();
					parse_command((unsigned char)src->get());
					this->effect_type = (XMNoteEffect::Type)src->get();
					this->effect_parameter = (unsigned char)src->get();
				}
				else
				{
					// Packed: first_byte is the field map.
					int fields = first_byte;

					this->note = (char)((fields & 1) ? src->get() : 0);
					this->instrument = (fields & 2) ? (unsigned char)src->get() : 0;
					if (fields & 4)
						parse_command((unsigned char)src->get());
					this->effect_type = (XMNoteEffect::Type)((fields & 8) ? src->get() : 0);
					this->effect_parameter = (fields & 16) ? (unsigned char)src->get() : 0;
				}
			}
		};
#pragma pack()
		struct xm_pattern_row
		{
			vector<xm_pattern_note> channel_notes;

			xm_pattern_row() {}

			xm_pattern_row(int num_channels)
			{
				channel_notes.resize(unsigned(num_channels));
			}
		};

		struct xm_pattern
		{
			xm_pattern_header header;
			vector<xm_pattern_row> rows;
		};
#pragma pack(1)
		namespace XMSampleFlags
		{
			enum Type : char
			{
				NoLooping = 0,
				ForwardLooping = 1,
				PingPongLooping = 2,

				SampleSize8bit = 0,
				SampleSize16bit = 16,

				Stereo = 32, // ModPlug Tracker extension

				LoopMask = 3,
				SampleSizeMask = 16,
			};
		}

		namespace XMSampleEncoding
		{
			enum Type : unsigned char
			{
				Delta = 0,
				ADPCM = 0xAD,
			};

			string ToString(Type type)
			{
				if (type == Delta)
					return "Delta";
				if (type == ADPCM)
					return "ADPCM";

				stringstream ss;

				ss << "unknown sample encoding: " << int(type);

				return ss.str();
			}
		}

		struct xm_sample_header
		{
			unsigned int sample_length;
			unsigned int sample_loop_start;
			unsigned int sample_loop_length; // 0 here is a hard off signal for looping
			char volume;
			char fine_tune;
			XMSampleFlags::Type flags;
			unsigned char panning;
			char relative_note_number;
			XMSampleEncoding::Type sample_encoding;
			char sample_name[22];
		};

		struct xm_sample
		{
			xm_sample_header header;
			char *encoded_sample_data;

			xm_sample(const xm_sample_header &header)
			{
				this->header = header;
				this->encoded_sample_data = new char[header.sample_length];
			}

			xm_sample(xm_sample &&other) noexcept
			{
				this->header = other.header;
				this->encoded_sample_data = other.encoded_sample_data;

				other.encoded_sample_data = nullptr;
			}

			template <typename SampleType>
			vector<SampleType *> decode_sample_data() const
			{
				bool looks_like_adpcm = 
					((header.flags & XMSampleFlags::SampleSize16bit) == 0) &&
					(header.sample_encoding == XMSampleEncoding::ADPCM);

				bool stereo = (header.flags & XMSampleFlags::Stereo) != 0;

				if (looks_like_adpcm)
					throw "Not implemented: ADPCM";

				vector<SampleType *> sample_channels;

				if (!stereo)
				{
					if (header.flags & XMSampleFlags::SampleSize16bit)
					{
						unsigned sample_count = header.sample_length >> 1;

						short *encoded = (short *)&encoded_sample_data[0];
						short *decoded = new short[sample_count];

						decoded[0] = encoded[0];

						for (unsigned i=1; i < sample_count; i++)
							decoded[i] = decoded[i - 1] + encoded[i];

						sample_channels.push_back((SampleType *)decoded);
					}
					else
					{
						unsigned sample_count = header.sample_length;

						char *encoded = (char *)&encoded_sample_data[0];
						char *decoded = new char[sample_count];

						decoded[0] = encoded[0];

						for (unsigned i=1; i < sample_count; i++)
							decoded[i] = decoded[i - 1] + encoded[i];

						sample_channels.push_back((SampleType *)decoded);
					}
				}
				else
				{
					if (header.flags & XMSampleFlags::SampleSize16bit)
					{
						unsigned sample_count = header.sample_length >> 1;

						short *encoded = (short *)&encoded_sample_data[0];
						short *decoded_left = new short[sample_count];
						short *decoded_right = new short[sample_count];

						// Left channel
						decoded_left[0] = encoded[0];

						for (unsigned i=1; i < sample_count; i++)
							decoded_left[i] = decoded_left[i - 1] + encoded[i];

						// Right channel
						decoded_right[0] = encoded[sample_count] + decoded_left[sample_count - 1];

						for (unsigned i=1, p=sample_count+1; i < sample_count; i++, p++)
							decoded_right[i] = decoded_right[i - 1] + encoded[p];

						sample_channels.push_back((SampleType *)decoded_left);
						sample_channels.push_back((SampleType *)decoded_right);
					}
					else
					{
						unsigned sample_count = header.sample_length;

						char *encoded = (char *)&encoded_sample_data[0];
						char *decoded_left = new char[sample_count];
						char *decoded_right = new char[sample_count];

						// Left channel
						decoded_left[0] = encoded[0];

						for (unsigned i=1; i < sample_count; i++)
							decoded_left[i] = decoded_left[i - 1] + encoded[i];

						// Right channel
						decoded_right[0] = encoded[sample_count] + decoded_left[sample_count - 1];

						for (unsigned i=1, p=sample_count+1; i < sample_count; i++, p++)
							decoded_right[i] = decoded_right[i - 1] + encoded[p];

						sample_channels.push_back((SampleType *)decoded_left);
						sample_channels.push_back((SampleType *)decoded_right);
					}
				}

				return sample_channels;
			}

			~xm_sample()
			{
				if (encoded_sample_data != nullptr)
					delete[] encoded_sample_data;
			}
		};

		struct xm_envelope_point
		{
			unsigned short frame;
			unsigned short value;
		};

		namespace XMEnvelopeFlags
		{
			enum Type : char
			{
				On = 1,
				Sustain = 2,
				Loop = 4,
			};

			const char *ToString(Type value)
			{
				switch (value)
				{
					case On: return "On";
					case On|Sustain: return "On+Sustain";
					case On|Loop: return "On+Loop";
					case On|Sustain|Loop: return "On+Sustain+Loop";

					default: return "Off";
				}
			}
		}

		struct xm_sample_set_header
		{
			int sample_set_header_length;
			unsigned char keymap[96];
			xm_envelope_point volume_envelope_points[12];
			xm_envelope_point panning_envelope_points[12];
			char num_volume_envelope_points;
			char num_panning_envelope_points;
			char volume_sustain_point;
			char volume_loop_start_point;
			char volume_loop_end_point;
			char panning_sustain_point;
			char panning_loop_start_point;
			char panning_loop_end_point;
			XMEnvelopeFlags::Type volume_envelope_flags;
			XMEnvelopeFlags::Type panning_envelope_flags;
			char vibrato_type;
			char vibrato_sweep;
			char vibrato_depth;
			char vibrato_rate;
			unsigned short fade_out;
			short reserved[22];
		};

		struct xm_instrument_header
		{
			int total_instrument_header_length;
			char instrument_name[22];
			char instrument_type;
			unsigned short number_of_samples;
		};
#pragma pack()

		struct xm_instrument
		{
			xm_instrument_header header;
			xm_sample_set_header sample_set_header;
			vector<xm_sample> samples;
		};

		struct xm_module
		{
			xm_preamble preamble;
			xm_header header;
			vector<xm_pattern> patterns;
			vector<xm_instrument> instruments;

			void load_preamble(istream *input);
			void load_header(istream *input);
			void load_pattern(istream *input);
			void load_instrument(istream *input);

			static xm_module load(istream *input);

			vector<pattern> convert_patterns(const vector<sample *> &converted_instruments, bool has_note_events[MAX_MODULE_CHANNELS]);
			vector<sample *> convert_samples();

			module_struct *to_module_struct();
		};

		xm_pattern load_xm_pattern(istream *file, const xm_header &module_header)
		{
			xm_pattern pattern;

			file->read((char *)&pattern.header, sizeof(pattern.header));

			if (pattern.header.pattern_rows > 1000)
				throw "Does not appear to be an XM file";

			for (int i=0; i < pattern.header.pattern_rows; i++)
				pattern.rows.push_back(xm_pattern_row(module_header.num_channels));

			if (pattern.header.packed_data_length != 0)
			{
				for (unsigned i=0; i < pattern.header.pattern_rows; i++)
				{
					xm_pattern_row &row = pattern.rows[i];

					for (unsigned j = 0; j < unsigned(module_header.num_channels); j++)
						row.channel_notes[j].decode_and_fill(file);
				}
			}

			return pattern;
		}

		xm_instrument load_xm_instrument(istream *file)
		{
			xm_instrument instrument;

			memset((void *)&instrument.header, 0, sizeof(instrument.header));

			if (instrument.header.total_instrument_header_length > 10000)
				throw "Does not appear to be an XM file";

			auto instrument_start = file->tellg();

			file->read((char *)&instrument.header.total_instrument_header_length, 4);
			file->seekg(instrument_start);

			file->read((char *)&instrument.header, sizeof(instrument.header));

			if (instrument.header.number_of_samples == 0)
			{
				file->seekg(instrument_start);
				file->seekg(instrument.header.total_instrument_header_length, ios::cur);
			}
			else
			{
				file->seekg(instrument_start);
				file->seekg(sizeof(instrument.header), ios::cur);

				memset((void *)&instrument.sample_set_header, 0, sizeof(instrument.sample_set_header));

				auto sample_set_header_start = file->tellg();

				int sample_set_header_length = instrument.header.total_instrument_header_length - int(sizeof(instrument.header));

				file->read((char *)&instrument.sample_set_header.sample_set_header_length, 4);
				file->seekg(sample_set_header_start);

				file->read((char *)&instrument.sample_set_header, min((int)sizeof(instrument.sample_set_header), sample_set_header_length));

				file->seekg(sample_set_header_start);
				file->seekg(sample_set_header_length, ios::cur);

				for (int i=0; i < instrument.header.number_of_samples; i++)
				{
					xm_sample_header sample_header;

					file->read((char *)&sample_header, sizeof(sample_header));

					if ((sample_header.sample_loop_start > sample_header.sample_length)
					 || (sample_header.sample_loop_length > sample_header.sample_length - sample_header.sample_loop_start))
						throw "Does not appear to be an XM file";

					instrument.samples.push_back(xm_sample(sample_header));
				}

				for (unsigned i=0; i < instrument.header.number_of_samples; i++)
				{
					xm_sample &sample = instrument.samples[i];

					file->read(sample.encoded_sample_data, sample.header.sample_length);
				}
			}

			return instrument;
		}

		void xm_module::load_preamble(istream *input)
		{
			input->read((char *)&preamble, sizeof(preamble));
		}

		void xm_module::load_header(istream *input)
		{
			auto module_start = input->tellg();

			input->read((char *)&header, sizeof(header));

			input->seekg(module_start);
			input->seekg(header.header_length, ios::cur);
		}

		void xm_module::load_pattern(istream *input)
		{
			patterns.push_back(load_xm_pattern(input, header));
		}

		void xm_module::load_instrument(istream *input)
		{
			instruments.push_back(load_xm_instrument(input));
		}

		/*static*/ xm_module xm_module::load(istream *input)
		{
			xm_module ret;

			ret.load_preamble(input);
			ret.load_header(input);

			for (int i=0; i < ret.header.num_patterns; i++)
				ret.load_pattern(input);

			for (int i=0; i < ret.header.num_instruments; i++)
				ret.load_instrument(input);

			return ret;
		}

		vector<pattern> xm_module::convert_patterns(const vector<sample *> &converted_instruments, bool has_note_events[MAX_MODULE_CHANNELS])
		{
			vector<pattern> ret;

			for (unsigned i=0; i < header.num_patterns; i++)
			{
				ret.push_back(pattern(int(i)));

				xm_pattern &xm_pattern = this->patterns[i];
				pattern &pattern = ret.back();

				for (unsigned row_number=0; row_number < xm_pattern.header.pattern_rows; row_number++)
				{
					xm_pattern_row &xm_row = xm_pattern.rows[row_number];

					vector<row> row_data(header.num_channels);

					for (size_t channel = 0, l = row_data.size(); channel < l; channel++)
					{
						if (channel < xm_row.channel_notes.size())
						{
							xm_pattern_note &xm_note = xm_row.channel_notes[channel];
							row &note = row_data[channel];

							if (xm_note.note)
								has_note_events[channel] = true;

							note.snote = snote_from_xnote(xm_note.note);
							note.znote = znote_from_snote(note.snote);

							if ((xm_note.instrument > 0) && (xm_note.instrument <= converted_instruments.size()))
								note.instrument = converted_instruments[xm_note.instrument - 1];
							else
								note.instrument = nullptr;

							switch (xm_note.command)
							{
								case XMNoteCommand::VolumeSet:
									note.volume = xm_note.command_parameter;
									break;
								case XMNoteCommand::VolumeSlideDown:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::VolumeSlide, xm_note.command_parameter);
									break;
								case XMNoteCommand::VolumeSlideUp:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::VolumeSlide, xm_note.command_parameter << 4);
									break;
								case XMNoteCommand::VolumeFineSlideDown:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::VolumeSlide, 0xF0 | xm_note.command_parameter);
									break;
								case XMNoteCommand::VolumeFineSlideUp:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::VolumeSlide, 0x0F | (xm_note.command_parameter << 4));
									break;
								case XMNoteCommand::VibratoSpeed:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::SetVibratoSpeed, xm_note.command_parameter * 0x11);
									break;
								case XMNoteCommand::VibratoDepth:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::Vibrato, xm_note.command_parameter);
									break;
								case XMNoteCommand::Pan:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::Panning, xm_note.command_parameter * 4);
									break;
								case XMNoteCommand::PanSlideLeft:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::PanSlide, xm_note.command_parameter << 4);
									break;
								case XMNoteCommand::PanSlideRight:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::PanSlide, xm_note.command_parameter);
									break;
								case XMNoteCommand::TonePortamento:
									note.secondary_effect = effect_struct(EffectType::XM, Effect::TonePortamento, xm_note.command_parameter * 0x11);
									break;
							}

							note.effect = effect_struct();

							bool is_null_effect =
								(xm_note.effect_type == 0) && // "Arpeggio"
								(xm_note.effect_parameter == 0);

							if (!is_null_effect)
							{
								auto mod_effect = XMNoteEffect::ConvertToMODEffect(xm_note.effect_type);

								if (mod_effect != MODEffect::None)
									note.effect.init(EffectType::XM, mod_effect, xm_note.effect_parameter, &note);
								else
								{
									// XM-specific extensions to the MOD effects
									switch (xm_note.effect_type)
									{
										case XMNoteEffect::SetGlobalVolume: // 'G'
											note.effect.init(EffectType::XM, Effect::GlobalVolume, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::GlobalVolumeSlide: // 'H'
											note.effect.init(EffectType::XM, Effect::GlobalVolumeSlide, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::KeyOff: // 'K'
											note.effect.init(EffectType::XM, Effect::ExtendedEffect, ExtendedEffect::NoteCut, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::SetEnvelopePosition: // 'L'
											note.effect.init(EffectType::XM, Effect::SetEnvelopePosition, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::PanningSlide: // 'P'
											note.effect.init(EffectType::XM, Effect::PanSlide, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::MultiRetrigger: // 'R'
											note.effect.init(EffectType::XM, Effect::Retrigger, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::Tremor: // 'T'
											note.effect.init(EffectType::XM, Effect::Tremor, xm_note.effect_parameter, &note);
											break;
										case XMNoteEffect::ExtraFinePortamento: // 'X'
											switch (xm_note.command_parameter >> 4)
											{
												case 1: // up
													note.effect.init(EffectType::XM, Effect::PortamentoUp, 0xE, xm_note.command_parameter & 0xF, &note);
													break;
												case 2: // down
													note.effect.init(EffectType::XM, Effect::PortamentoDown, 0xE, xm_note.command_parameter & 0xF, &note);
													break;
											}

											break;
									}
								}
							}
						}
					}

					pattern.row_list.push_back(row_data);
				}
			}

			return ret;
		}

		instrument_envelope convert_xm_envelope(int num_points, xm_envelope_point *points, int sustain_point, int loop_start_point, int loop_end_point, XMEnvelopeFlags::Type flags, bool signed_values)
		{
			instrument_envelope ret;

			ret.enabled = ((flags & XMEnvelopeFlags::On) != 0);
			ret.looping = true;
			ret.sustain_loop = ((flags & XMEnvelopeFlags::Sustain) != 0);

			if (ret.enabled)
			{
				if ((flags & XMEnvelopeFlags::Loop) != 0)
				{
					if ((loop_start_point < 0) || (loop_start_point >= num_points))
						throw "Does not appear to be an XM file";
					if ((loop_end_point < 0) || (loop_end_point >= num_points))
						throw "Does not appear to be an XM file";

					ret.loop_begin_tick = points[loop_start_point].frame;
					ret.loop_end_tick = points[loop_end_point].frame;
				}
				else
				{
					ret.loop_begin_tick = points[num_points - 1].frame;
					ret.loop_end_tick = points[num_points - 1].frame;
				}

				if (ret.sustain_loop)
				{
					if ((sustain_point < 0) && (sustain_point >= num_points))
						throw "Does not appear to be an XM file";

					ret.sustain_loop_begin_tick = points[sustain_point].frame;
					ret.sustain_loop_end_tick = points[sustain_point].frame;
				}

				for (int i=0; i < num_points; i++)
				{
					if (signed_values)
						ret.node.push_back(instrument_envelope_node(points[i].frame, (points[i].value - 32) / 32.0));
					else
						ret.node.push_back(instrument_envelope_node(points[i].frame, points[i].value / 64.0));
				}
			}

			return ret;
		}

		vector<sample *> xm_module::convert_samples()
		{
			vector<sample *> ret;

			int sample_index = 0;

			for (auto iter = instruments.begin(), l = instruments.end(); iter != l; ++iter)
			{
				xm_instrument &xm_instrument = *iter;

				xm_sample_set_header &xm_header = xm_instrument.sample_set_header;

				vector<sample *> converted_samples;

				for (size_t i=0; i < xm_instrument.samples.size(); i++)
				{
					const xm_sample &xm_sample = xm_instrument.samples[i];
					const xm_sample_header &xm_sample_header = xm_sample.header;

					sample *sample;

					unsigned loop_begin = xm_sample_header.sample_loop_start;
					unsigned loop_end = loop_begin + xm_sample_header.sample_loop_length;

					if ((xm_sample_header.flags & XMSampleFlags::SampleSizeMask) == XMSampleFlags::SampleSize16bit)
					{
						loop_begin >>= 1;
						loop_end >>= 1;
					}

					if ((xm_sample_header.flags & XMSampleFlags::LoopMask) == XMSampleFlags::NoLooping)
						loop_end = LOOP_END_NO_LOOP;

					LoopStyle::Type loop_style = LoopStyle::Forward;

					if ((xm_sample_header.flags & XMSampleFlags::PingPongLooping) != 0)
						loop_style = LoopStyle::PingPong;

					switch (xm_sample_header.flags & XMSampleFlags::SampleSizeMask)
					{
						case XMSampleFlags::SampleSize8bit:
						{
							vector<signed char *> data = xm_sample.decode_sample_data<signed char>();

							sample = new sample_builtintype<signed char>(
								int(i),
								int(data.size()),
								xm_sample_header.volume / 64.0,
								&data[0],
								unsigned(xm_sample_header.sample_length / data.size()),
								loop_style,
								LoopStyle::Undefined,
								loop_begin,
								loop_end);

							break;
						}
						case XMSampleFlags::SampleSize16bit:
						{
							vector<signed short *> data = xm_sample.decode_sample_data<signed short>();

							sample = new sample_builtintype<signed short>(
								int(i),
								int(data.size()),
								xm_sample_header.volume / 64.0,
								&data[0],
								unsigned(xm_sample_header.sample_length / 2 / data.size()), // convert bytes to samples
								loop_style,
								LoopStyle::Undefined,
								loop_begin,
								loop_end);

							break;
						}

						default:
							throw "Don't know how to convert this XM sample";
					}

					// This crazy, redundant math is lifted straight out of the CelerSMS reverse-engineered format document unchanged.
					double period = 10.0 * 12.0 * 16.0 * 4.0 - xm_sample_header.relative_note_number * 16.0 * 4.0 - xm_sample_header.fine_tune * 0.5;

					double exponent = (6.0 * 12.0 * 16.0 * 4.0 - period) / (12.0 * 16.0 * 4.0);

					// The exponent by default gives us the frequency of C-0, but we want the frequency of C-4.
					exponent += 4;

					double frequency = 8363 * exp2(exponent);

					sample->samples_per_second = frequency;

					if (xm_header.vibrato_depth > 0)
					{
						sample->use_vibrato = true;

						sample->vibrato_depth = xm_header.vibrato_depth / 600.0;
						sample->vibrato_cycle_frequency = xm_header.vibrato_rate * 95;
						sample->vibrato_sweep_frames = xm_header.vibrato_sweep;
					}

					converted_samples.push_back(sample);
				}

				sample_instrument *instrument = new sample_instrument(sample_index);

				// vibrato_type is ignored, because who would use square wave or ramp down vibrato shapes?
				// maybe I'll bump into someone using them and have to add it later.

				instrument->global_volume = 1.0;

				instrument->use_vibrato = (xm_header.vibrato_depth > 0);

				// These coefficients arrived at experimentally. *hangs head in shame*
				instrument->vibrato_depth = xm_header.vibrato_depth / 600.0;
				instrument->vibrato_cycle_frequency = xm_header.vibrato_rate * 95;
				instrument->vibrato_sweep_frames = xm_header.vibrato_sweep;

				instrument->fade_out = xm_header.fade_out / 32768.0;

				if (xm_instrument.samples.size() > 0)
				{
					instrument->volume_envelope = convert_xm_envelope(
						xm_header.num_volume_envelope_points,
						xm_header.volume_envelope_points,
						xm_header.volume_sustain_point,
						xm_header.volume_loop_start_point,
						xm_header.volume_loop_end_point,
						xm_header.volume_envelope_flags,
						false); // unsigned values

					instrument->panning_envelope = convert_xm_envelope(
						xm_header.num_panning_envelope_points,
						xm_header.panning_envelope_points,
						xm_header.panning_sustain_point,
						xm_header.panning_loop_start_point,
						xm_header.panning_loop_end_point,
						xm_header.panning_envelope_flags,
						true); // signed values

					for (int i=0; i < 96; i++)
					{
						unsigned mapped_sample_index = xm_header.keymap[i];

						if (mapped_sample_index < converted_samples.size())
							instrument->note_sample[i] = converted_samples[mapped_sample_index];
					}
				}

				ret.push_back(instrument);

				sample_index++;
			}

			return ret;
		}

		module_struct *xm_module::to_module_struct()
		{
			module_struct *ret = new module_struct();

			ret->name = preamble.module_name;

			ret->stereo = true;

			bool has_note_events[MAX_MODULE_CHANNELS];

			memset(&has_note_events[0], 0, sizeof(has_note_events));

			ret->samples = convert_samples();
			ret->patterns = convert_patterns(ret->samples, has_note_events);

			for (unsigned i=0; i<header.pattern_list_length; i++)
			{
				unsigned pattern_number = header.pattern_list[i];

				if (pattern_number >= ret->patterns.size())
					throw "Does not appear to be an XM file";

				ret->pattern_list.push_back(&ret->patterns[pattern_number]);
			}

			memset(ret->base_pan, 0, sizeof(ret->base_pan));

			unsigned count = 0;

			for (unsigned i=0; i<64; i++)
			{
				if (has_note_events[i])
				{
					count++;
					ret->channel_enabled[i] = true;
					ret->channel_map[i] = i;
				}
				else
					ret->channel_enabled[i] = false;
			}

			ret->num_channels = count;

			ret->speed = header.initial_speed;
			ret->tempo = header.initial_tempo;

			ret->xm_module = true;
			ret->amiga_panning = false;
			ret->channel_remember_note = true;
			ret->linear_slides = true;

			return ret;
		}

		void dump_str(const char *buffer, const char *heading, int data_length)
		{
			cerr << "STRING " << heading << ": [";

			bool hit_nul = false;

			for (int i=0; i < data_length; i++)
			{
				char ch = buffer[i];

				if (ch == '\0')
					hit_nul = true;

				if (hit_nul)
					ch = '#';

				if (ch < 32)
					ch = '.';
				if (ch > 126)
					ch = '?';

				cerr << ch;
			}

			cerr << ']' << endl;
		}

		void dump_preamble(const xm_preamble &preamble)
		{
			cerr << "PREAMBLE" << endl;
			cerr << "========" << endl;

			dump_str(preamble.id_text, "id_text", 17);
			dump_str(preamble.module_name, "module_name", 20);

			cerr << "escape character (nominally 0x1A): 0x" << hex << int(preamble.escape) << dec << endl;

			dump_str(preamble.tracker_name, "tracker_name", 20);

			cerr << "version: " << int(preamble.version_major) << "." << int(preamble.version_minor) << endl;
			cerr << endl;
		}

		void dump_header(const xm_header &header)
		{
			cerr << "HEADER" << endl;
			cerr << "======" << endl;

			cerr << "header_length: "  << header.header_length << endl;
			cerr << "pattern_list_length: " << header.pattern_list_length	 << endl;
			cerr << "restart_position: " << header.restart_position << endl;
			cerr << "num_channels: " << header.num_channels << endl;
			cerr << "num_patterns: " << header.num_patterns << endl;
			cerr << "num_instruments: " << header.num_instruments << endl;
			cerr << "frequency_table: " << header.frequency_table << " ";

			if (header.frequency_table == 0)
				cerr << "Amiga";
			else if (header.frequency_table == 1)
				cerr << "Linear";
			else
				cerr << "???";

			cerr << endl;

			cerr << "initial_speed: " << header.initial_speed << endl;
			cerr << "initial_tempo: " << header.initial_tempo << endl;

			cerr << "pattern_list:";

			for (int i=0; i < header.pattern_list_length; i++)
				cerr << " " << int(header.pattern_list[i]);
			cerr << endl << endl;
		}

		void dump_note(const xm_pattern_note &note)
		{
			if (note.note == 0)
				cerr << "--- ";
			else
			{
				int snote = snote_from_xnote(note.note);

				int octave = snote >> 4;
				int note_in_octave = snote & 15;

				switch (note_in_octave)
				{
					case 0: cerr << "C-"; break;
					case 1: cerr << "C#"; break;
					case 2: cerr << "D-"; break;
					case 3: cerr << "D#"; break;
					case 4: cerr << "E-"; break;
					case 5: cerr << "F-"; break;
					case 6: cerr << "F#"; break;
					case 7: cerr << "G-"; break;
					case 8: cerr << "G#"; break;
					case 9: cerr << "A-"; break;
					case 10: cerr << "A#"; break;
					case 11: cerr << "B-"; break;
				}

				cerr << octave << " ";
			}

			cerr << setfill('0');

			if (note.instrument > 0)
				cerr << setw(2) << int(note.instrument) << setw(0) << " ";
			else
				cerr << "-- ";

			switch (note.command)
			{
				case XMNoteCommand::None: cerr << "---"; break;

				case XMNoteCommand::VolumeSet: cerr << ' ' << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VolumeSlideDown: cerr << 'v' << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VolumeSlideUp: cerr << '^' << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VolumeFineSlideDown: cerr << "⭭" << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VolumeFineSlideUp: cerr << "⭫" << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VibratoSpeed: cerr << 'w' << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::VibratoDepth: cerr << '~' << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::Pan: cerr << "⮂" << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::PanSlideLeft: cerr << "«" << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::PanSlideRight: cerr << "»" << setw(2) << int(note.command_parameter); break;
				case XMNoteCommand::TonePortamento: cerr << "⭜" << setw(2) << int(note.command_parameter); break;
			}

			cerr << setw(0) << ' ';
			
			if (note.effect_type <= 36)
			{
				cerr << ' ';

				if (note.effect_type < 16)
					cerr << hex << int(note.effect_type);
				else
					cerr << char(note.effect_type - 16 + 64);

				cerr << setw(2) << hex << uppercase << int(note.effect_parameter) << dec << setw(0);
			}
			else if ((note.effect_type >= 0xE0) && (note.effect_type < 0xF0))
				cerr << setw(2) << hex << uppercase << int(note.effect_type) << int(note.effect_parameter) << dec << setw(0);
			else
				cerr << "----";

			cerr << setfill(' ');
		}

		void dump_pattern(const xm_pattern &pattern)
		{
			vector<xm_pattern_row> rows;

			for (unsigned row_number=0; row_number < pattern.header.pattern_rows; row_number++)
			{
				const xm_pattern_row &row = pattern.rows[row_number];

				cerr << setw(4) << row_number << setw(0) << " | ";

				for (unsigned channel=0; channel < row.channel_notes.size(); channel++)
				{
					if (channel > 0)
						cerr << "    ";
					dump_note(row.channel_notes[channel]);
				}

				cerr << endl;
			}
		}

		void dump_envelope(int num_points, const xm_envelope_point *points)
		{
			#define ENVELOPE_COLS 80
			#define ENVELOPE_ROWS 15

			char picture[ENVELOPE_ROWS * ENVELOPE_COLS];

			memset(picture, ' ', ENVELOPE_ROWS * ENVELOPE_COLS);

			char *buf = &picture[0];

			auto draw_dot = [buf](int x, int y) { if ((x >= 0) && (x < ENVELOPE_COLS) && (y >= 0) && (y < ENVELOPE_ROWS)) buf[y * ENVELOPE_COLS + x] = '*'; };

			auto swap = [](int &a, int &b) { int c = a; a = b; b = c; };

			auto draw_line = [swap, draw_dot](int x1, int y1, int x2, int y2)
			{
				y1 = (64 - y1) * ENVELOPE_ROWS / 64;
				y2 = (64 - y2) * ENVELOPE_ROWS / 64;

				int dx = x2 - x1;
				int dy = y2 - y1;

				if (abs(dx) > abs(dy))
				{
					if (x1 > x2)
					{
						swap(x1, x2);
						swap(y1, y2);
					}

					for (int x = x1; x <= x2; x++)
					{
						int y = (x - x1) * dy / dx + y1;

						draw_dot(x, y);
					}
				}
				else
				{
					if (y1 > y2)
					{
						swap(x1, x2);
						swap(y1, y2);
					}

					for (int y = y1; y <= y2; y++)
					{
						int x = (y - y1) * dx / dy + x1;

						draw_dot(x, y);
					}
				}
			};

			for (int i = 1; i < num_points; i++)
			{
				auto a = points[i - 1];
				auto b = points[i];

				draw_line(a.frame, a.value, b.frame, b.value);
			}

			for (int y=0; y < ENVELOPE_ROWS; y++)
				cerr << string(&picture[y * ENVELOPE_COLS], ENVELOPE_COLS) << endl;
			cerr << endl;

			for (int i=0; i < num_points; i++)
				cerr << "[" << i << "]: " << points[i].value << " at frame " << points[i].frame << endl;
		}

		void dump_sample(const xm_sample &sample)
		{
			#define WAVEFORM_COLS 140
			#define WAVEFORM_ROWS 25

			char picture[WAVEFORM_ROWS * WAVEFORM_COLS];

			memset(picture, ' ', WAVEFORM_ROWS * WAVEFORM_COLS);

			char *buf = &picture[0];

			auto draw_dot = [buf](unsigned x, unsigned y) { if ((x < WAVEFORM_COLS) && (y < WAVEFORM_ROWS)) buf[y * WAVEFORM_COLS + x] = '*'; };

			unsigned samples_in_view = sample.header.sample_length + sample.header.sample_length / 10;

			if (samples_in_view < 2000)
				samples_in_view = 2000;

			unsigned samples_per_col = samples_in_view / WAVEFORM_COLS;

			// If the sample is stereo, only the left channel is rendered presently.
			char *sample_data_8bit = nullptr;
			short *sample_data_16bit = nullptr;

			bool is_16bit = (sample.header.flags & XMSampleFlags::SampleSize16bit) != 0;

			if (!is_16bit)
				sample_data_8bit = sample.decode_sample_data<char>().front();
			else
				sample_data_16bit = sample.decode_sample_data<short>().front();

			unsigned sample_count = sample.header.sample_length;

			if (is_16bit)
				sample_count >>= 1;

			for (unsigned x = 0; x < WAVEFORM_COLS; x++)
			{
				unsigned start_sample = x * samples_per_col;
				unsigned end_sample = start_sample + samples_per_col - 1;

				short min_sample = 32767, max_sample = -32768;

				if (start_sample >= sample_count)
					break;

				if (end_sample >= sample_count)
					end_sample = sample_count - 1;

				for (unsigned i=start_sample; i <= end_sample; i++)
				{
					short sample_value;

					if (is_16bit)
						sample_value = sample_data_16bit[i];
					else
					{
						sample_value = sample_data_8bit[i];
						sample_value *= 0x101;
					}

					if (sample_value < min_sample)
						min_sample = sample_value;
					if (sample_value > max_sample)
						max_sample = sample_value;

					int min_sample_y = WAVEFORM_ROWS - (min_sample + 32768) * WAVEFORM_ROWS / 65536;
					int max_sample_y = WAVEFORM_ROWS - (max_sample + 32768) * WAVEFORM_ROWS / 65536;

					if (max_sample_y < 0)
						max_sample_y = 0;

					for (unsigned y = unsigned(max_sample_y); y <= unsigned(min_sample_y); y++)
						draw_dot(x, y);
				}
			}

			for (int y=0; y < WAVEFORM_ROWS; y++)
				cerr << string(&picture[y * WAVEFORM_COLS], WAVEFORM_COLS) << endl;

			// This crazy, redundant math is lifted straight out of the CelerSMS reverse-engineered format document unchanged.
			double period = 10.0 * 12.0 * 16.0 * 4.0 - sample.header.relative_note_number * 16.0 * 4.0 - sample.header.fine_tune * 0.5;

			double exponent = (6.0 * 12.0 * 16.0 * 4.0 - period) / (12.0 * 16.0 * 4.0);

			double frequency = 8363 * exp2(exponent);

			// We've now calculated the frequency of the floor of the note range: C-0. It seems to be
			// conventional to use C-4, so we need to go up 4 octivates. That means doubling the
			// frequency 4 times.
			frequency *= 16;

			cerr << "number of samples: " << sample_count << " / frequency: " << fixed << setprecision(1) << frequency << defaultfloat << " Hz" << endl;
			cerr << endl;
		}

		void dump_instrument(const xm_instrument &instrument)
		{
			const xm_instrument_header &header = instrument.header;

			cerr << "total_instrument_header_length: " << header.total_instrument_header_length << endl;

			dump_str(header.instrument_name, "instrument_name", 22);

			cerr << "instrument_type: " << int(header.instrument_type) << endl;
			cerr << "number_of_samples: " << header.number_of_samples << endl;

			if (header.number_of_samples > 0)
			{
				const xm_sample_set_header &header2 = instrument.sample_set_header;

				cerr << endl;
				cerr << "sample_set_header_length: " << header2.sample_set_header_length << endl;

				cerr << "keymap:" << endl;

				int last_change = 0;

				for (int i=1; i < 96; i++)
				{
					if (header2.keymap[i] != header2.keymap[i - 1])
					{
						cerr << "  " << last_change;

						if (i - 1 > i)
							cerr << " - " << (i - 1);

						cerr << ": " << header2.keymap[i - 1] << endl;

						last_change = i;
					}
				}

				cerr << "  " << last_change;

				if (last_change < 95)
					cerr << " - 95";

				cerr << ": " << header2.keymap[95] << endl;

				if (header2.num_volume_envelope_points == 0)
					cerr << "volume envelope: none" << endl;
				else
				{
					cerr << "volume envelope:" << endl;

					dump_envelope(header2.num_volume_envelope_points, header2.volume_envelope_points);
				}

				if (header2.num_panning_envelope_points == 0)
					cerr << "panning envelope: none" << endl;
				else
				{
					cerr << "panning envelope:" << endl;

					dump_envelope(header2.num_panning_envelope_points, header2.panning_envelope_points);
				}

				cerr << "volume sustain point: " << int(header2.volume_sustain_point) << endl;
				cerr << "volume loop start point: " << int(header2.volume_loop_start_point) << endl;
				cerr << "volume loop end point: " << int(header2.volume_loop_end_point) << endl;
				cerr << "panning sustain point: " << int(header2.panning_sustain_point) << endl;
				cerr << "panning loop start point: " << int(header2.panning_loop_start_point) << endl;
				cerr << "panning loop end point: " << int(header2.panning_loop_end_point) << endl;

				cerr << "volume envelope flags: " << XMEnvelopeFlags::ToString(header2.volume_envelope_flags) << endl;
				cerr << "panning envelope flags: " << XMEnvelopeFlags::ToString(header2.panning_envelope_flags) << endl;

				cerr << "vibrato type: " << int(header2.vibrato_type) << endl;
				cerr << "vibrato sweep: " << int(header2.vibrato_sweep) << endl;
				cerr << "vibrato depth: " << int(header2.vibrato_depth) << endl;
				cerr << "vibrato rate: " << int(header2.vibrato_rate) << endl;

				cerr << "fade out: " << int(header2.fade_out) << endl;

				for (size_t sample_index = 0, l = instrument.samples.size(); sample_index < l; sample_index++)
				{
					cerr << endl;

					cerr << "sample #" << sample_index << ":" << endl;

					dump_sample(instrument.samples[sample_index]);
				}
			}
		}

		void dump(const xm_module &module)
		{
			dump_preamble(module.preamble);
			dump_header(module.header);

			for (size_t i=0; i < module.patterns.size(); i++)
			{
				cerr << "PATTERN " << i << endl;
				cerr << "===========" << endl;

				dump_pattern(module.patterns[i]);

				cerr << endl;
			}

			for (size_t i=0; i < module.instruments.size(); i++)
			{
				cerr << "INSTRUMENT " << hex << uppercase << (i + 1) << nouppercase << dec << endl;
				cerr << "============= (" << i << ")" << endl;

				dump_instrument(module.instruments[i]);

				cerr << endl;
			}
		}
	}

	module_struct *load_xm(istream *file)
	{
		return xm_module::load(file).to_module_struct();
	}
}
