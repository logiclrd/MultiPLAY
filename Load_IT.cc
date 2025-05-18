#include "Load_IT.h"

#include <iostream>
#include <cstring>

using namespace std;

#include "RAII.h"

using namespace RAII;

#include "bit_memory_stream.h"
#include "bit_value.h"
#include "conversion.h"
#include "module.h"
#include "notes.h"
#include "sample_builtintype.h"
#include "sample_instrument.h"

namespace MultiPLAY
{
	namespace
	{
		struct it_created_with_tracker
		{
			int major_version, minor_version;
			int compatible_with_major_version, compatible_with_minor_version;

			bool compatible_exceeds(double version)
			{
				double compatible = compatible_with_major_version + (compatible_with_minor_version / 100.0);

				return (compatible >= version);
			}
		};

		struct it_flags
		{
			char value;

			bool stereo()                 { return (0 != (value &   1)); }
			bool vol0mixopt()             { return (0 != (value &   2)); }
			bool use_instruments()        { return (0 != (value &   4)); }
			bool linear_slides()          { return (0 != (value &   8)); }
			bool old_effects()            { return (0 != (value &  16)); }
			bool portamento_memory_link() { return (0 != (value &  32)); }
			bool use_midi_pitch_ctrl()    { return (0 != (value &  64)); }
			bool embedded_midi_config()   { return (0 != (value & 128)); }
		};

		struct it_settings
		{
			unsigned char global_volume, mix_volume, initial_speed, initial_tempo;
		};

		struct it_envelope_node
		{
			int tick;
			double yValue;

			it_envelope_node(int t, double y)
				: tick(t), yValue(y)
			{
			}
		};

		struct it_envelope_description
		{
			bool enabled, looping, sustain_loop;

			unsigned loop_start_node, loop_end_node;
			unsigned sustain_loop_start_node, sustain_loop_end_node;

			vector<it_envelope_node> envelope;
		};

		struct it_instrument_description
		{
			double global_volume;
			int default_pan;
			bool use_default_pan;
			
			unsigned char pitch_pan_center;
			unsigned char pitch_pan_separation;

			double volume_variation_pctg, panning_variation;

			unsigned int fade_out;

			DuplicateCheck::Type duplicate_note_check;
			DuplicateCheckAction::Type duplicate_check_action;
			NewNoteAction::Type new_note_action;

			int note_sample[120];
			int tone_offset[120];

			it_envelope_description volume_envelope, pan_envelope, pitch_envelope;
		};

		struct it_instrument_flags
		{
			char value;

			bool use_volume_envelope()     { return (0 != (value & 1)); }
			bool use_volume_loop()         { return (0 != (value & 2)); }
			bool use_sustain_volume_loop() { return (0 != (value & 4)); }
		};

		struct it_envelope_flags
		{
			char value;

			bool enabled()      { return (0 != (value & 1)); }
			bool looping()      { return (0 != (value & 2)); }
			bool sustain_loop() { return (0 != (value & 4)); }
		};

		void load_it_old_instrument(istream *file, it_instrument_description &desc, int i)
		{
			char magic[4];

			file->read(magic, 4);

			if (string(magic, 4) != "IMPI")
				throw "invalid instrument (missing 'IMPI' at start of header)";

			char dos_filename[13];
			file->read(dos_filename, 13);

			if (dos_filename[12] != 0)
			{
				dos_filename[12] = 0;
				cerr << "Warning: DOS filename not properly null-terminated in instrument #" << i << endl;
			}

			it_instrument_flags instrument_flags;
			file->read(&instrument_flags.value, 1);

			unsigned char volume_loop_start_node, volume_loop_end_node;
			unsigned char sustain_loop_start_node, sustain_loop_end_node;

			file->read((char *)&volume_loop_start_node, 1);
			file->read((char *)&volume_loop_end_node, 1);
			file->read((char *)&sustain_loop_start_node, 1);
			file->read((char *)&sustain_loop_end_node, 1);

			unsigned char lsb_bytes[2];

			file->read((char *)&lsb_bytes, 2);
			unsigned fade_out = from_lsb2_u(lsb_bytes) * 2; // fadeout is twice as fine in the new format

			char new_note_action = char(file->get());
			char duplicate_note_check = char(file->get());

			file->ignore(3);

			char instrument_name[27];
			file->read(instrument_name, 26);
			instrument_name[26] = 0;

			file->ignore(6);

			unsigned char instrument_for_note[120] = { 0 }; // empty array
			int note_difference[120] = { 0 }; // empty

			for (int note_index=0; note_index<120; note_index++)
			{
				unsigned char instrument, note;

				file->read((char *)&note, 1);
				file->read((char *)&instrument, 1);

				instrument_for_note[note_index] = instrument;
				note_difference[note_index] = note - note_index;
			}

			char rendered_volume_envelope[200];
			file->read(rendered_volume_envelope, 200);

			vector<it_envelope_node> volume_envelope;
			for (int node_index=0; node_index<25; node_index++)
			{
				unsigned char tick, magnitude;

				file->read((char *)&tick, 1);
				file->read((char *)&magnitude, 1);

				if (tick != 0xFF)
					volume_envelope.push_back(it_envelope_node(tick, magnitude / 64.0));
			}

			desc.global_volume = 1.0;
			desc.use_default_pan = false;

			desc.volume_variation_pctg = 0.0;
			desc.panning_variation = 0;

			desc.fade_out = fade_out;

			desc.new_note_action = (NewNoteAction::Type)new_note_action;

			if (duplicate_note_check == 1)
				desc.duplicate_note_check = DuplicateCheck::Instrument;
			else
				desc.duplicate_note_check = DuplicateCheck::Off;
			desc.duplicate_check_action = DuplicateCheckAction::Cut;

			for (int note_index=0; note_index<120; note_index++)
			{
				desc.note_sample[note_index] = instrument_for_note[note_index];
				desc.tone_offset[note_index] = note_difference[note_index];
			}

			desc.volume_envelope.enabled = instrument_flags.use_volume_envelope();
			desc.volume_envelope.looping = instrument_flags.use_volume_loop();
			desc.volume_envelope.sustain_loop = instrument_flags.use_sustain_volume_loop();

			desc.volume_envelope.loop_start_node = volume_loop_start_node;
			desc.volume_envelope.loop_end_node = volume_loop_end_node;

			desc.volume_envelope.sustain_loop_start_node = sustain_loop_start_node;
			desc.volume_envelope.sustain_loop_end_node = sustain_loop_end_node;

			desc.volume_envelope.envelope = volume_envelope;

			desc.pan_envelope.enabled = false;
			desc.pitch_envelope.enabled = false;
		}

		void load_it_new_instrument_envelope(istream *file, it_envelope_description &desc, bool signed_magnitude)
		{
			it_envelope_flags flags;
			flags.value = char(file->get());

			int num_node_points = file->get();

			unsigned loop_begin, loop_end;
			unsigned sustain_loop_begin, sustain_loop_end;

			loop_begin = unsigned(file->get());
			loop_end = unsigned(file->get());
			sustain_loop_begin = unsigned(file->get());
			sustain_loop_end = unsigned(file->get());

			vector<it_envelope_node> nodes;

			unsigned char lsb_bytes[2];

			for (int i=0; i<25; i++)
			{
				int magnitude, tick;

				magnitude = file->get();

				if (signed_magnitude)
					magnitude = (signed char)magnitude;

				file->read((char *)&lsb_bytes[0], 2);
				tick = from_lsb2(lsb_bytes);

				if (i >= num_node_points)
					continue;

				if (signed_magnitude)
					nodes.push_back(it_envelope_node(tick, magnitude / 32.0));
				else
					nodes.push_back(it_envelope_node(tick, magnitude / 64.0));
			}

			// Trailing byte for alignment.
			file->get();

			desc.enabled = flags.enabled();
			desc.looping = flags.looping();
			desc.sustain_loop = flags.sustain_loop();

			desc.loop_start_node = loop_begin;
			desc.loop_end_node = loop_end;
			desc.sustain_loop_start_node = sustain_loop_begin;
			desc.sustain_loop_end_node = sustain_loop_end;

			desc.envelope = nodes;
		}

		void load_it_new_instrument(istream *file, it_instrument_description &desc, int i)
		{
			char magic[4];

			file->read(magic, 4);

			if (string(magic, 4) != "IMPI")
				throw "invalid instrument (missing 'IMPI' at start of header)";

			char dos_filename[13];
			file->read(dos_filename, 13);

			if (dos_filename[12] != 0)
			{
				dos_filename[12] = 0;
				cerr << "Warning: DOS filename not properly null-terminated in instrument #" << i << endl;
			}

			char new_note_action = char(file->get());
			char duplicate_check_type = char(file->get());
			char duplicate_check_action = char(file->get());

			unsigned char lsb_bytes[2];

			file->read((char *)&lsb_bytes[0], 2);
			unsigned fade_out = from_lsb2_u(lsb_bytes);

			unsigned char pitch_pan_center, pitch_pan_separation;
			file->read((char *)&pitch_pan_separation, 1);
			file->read((char *)&pitch_pan_center, 1);

			unsigned char global_volume_from_file, default_pan;
			file->read((char *)&global_volume_from_file, 1);
			file->read((char *)&default_pan, 1);

			unsigned char volume_variation;
			char panning_variation;
			file->read((char *)&volume_variation, 1);
			file->read((char *)&panning_variation, 1);

			file->ignore(4); // TrkVers (2), NoS and an unused byte

			char instrument_name[27];
			file->read(instrument_name, 26);
			instrument_name[26] = 0;

			char ipc, ipr;
			ipc = char(file->get());
			ipr = char(file->get());

			char midi_channel, midi_program;
			unsigned midi_bank;
			midi_channel = char(file->get());
			midi_program = char(file->get());
			file->read((char *)&lsb_bytes[0], 2);
			midi_bank = from_lsb2_u(lsb_bytes);

			unsigned char instrument_for_note[120] = { 0 }; // empty array
			int note_difference[120] = { 0 }; // empty

			for (int note_index=0; note_index<120; note_index++)
			{
				unsigned char instrument, note;

				file->read((char *)&note, 1);
				file->read((char *)&instrument, 1);

				instrument_for_note[note_index] = instrument;
				note_difference[note_index] = note - note_index;
			}

			it_envelope_description volume_envelope, pan_envelope, pitch_envelope;

			load_it_new_instrument_envelope(file, volume_envelope, false);
			load_it_new_instrument_envelope(file, pan_envelope, true);
			load_it_new_instrument_envelope(file, pitch_envelope, true);

			desc.global_volume = global_volume_from_file / 128.0;

			desc.use_default_pan = (0 == (default_pan & 128));
			if (desc.use_default_pan)
				desc.default_pan = default_pan;

			desc.volume_variation_pctg = volume_variation / 100.0;
			desc.panning_variation = panning_variation;

			desc.fade_out = fade_out;

			desc.pitch_pan_separation = pitch_pan_separation;
			desc.pitch_pan_center = pitch_pan_center;

			desc.new_note_action = (NewNoteAction::Type)new_note_action;
			desc.duplicate_note_check = (DuplicateCheck::Type)duplicate_check_type;
			desc.duplicate_check_action = (DuplicateCheckAction::Type)duplicate_check_action;

			for (int note_index=0; note_index<120; note_index++)
			{
				desc.note_sample[note_index] = instrument_for_note[note_index];
				desc.tone_offset[note_index] = note_difference[note_index];
			}

			desc.volume_envelope = volume_envelope;
			desc.pan_envelope = pan_envelope;
			desc.pitch_envelope = pitch_envelope;
		}

		struct it_sample_flags
		{
			char value;

			bool sample_present()        { return (0 != (value &   1)); }
			bool sixteen_bit()           { return (0 != (value &   2)); }
			bool stereo()                { return (0 != (value &   4)); }
			bool compressed()            { return (0 != (value &   8)); }
			bool looping()               { return (0 != (value &  16)); }
			bool sustain_loop()          { return (0 != (value &  32)); }
			bool pingpong_loop()         { return (0 != (value &  64)); }
			bool pingpong_sustain_loop() { return (0 != (value & 128)); }
		};

		struct it_sample_conversion_flags
		{
			char value;

			bool signed_samples()    { return (0 != (value &  1)); }
			bool msb_order()         { return (0 != (value &  2)); }
			bool delta_values()      { return (0 != (value &  4)); }
			bool byte_delta_values() { return (0 != (value &  8)); }
			bool tx_wave_12bit()     { return (0 != (value & 16)); }
			bool stereo_prompt()     { return (0 != (value & 32)); }
		};

		template <class T>
		T *load_it_sample_compressed(istream *file, unsigned int sample_length, bool double_delta, bool signed_samples)
		{
			int bits_per_sample = sizeof(T) * 8;

			unsigned max_block_sample_count;
			int initial_bit_width;
			int bits_to_encode_bit_width;
			int sample_bias;

			switch (bits_per_sample)
			{
				case 8:
					max_block_sample_count = 0x8000;
					initial_bit_width = 9;
					bits_to_encode_bit_width = 3;
					sample_bias = signed_samples ? 0 : -128;
					break;
				case 16:
					max_block_sample_count = 0x4000;
					initial_bit_width = 17;
					bits_to_encode_bit_width = 4;
					sample_bias = signed_samples ? 0 : -32768;
					break;
				default:
					throw "bits_per_sample is not 8 or 16";
			}

			T *ret = new T[sample_length];
			unsigned offset = 0;

			unsigned char lsb_bytes[2];

			while (offset < sample_length)
			{
				file->read((char *)&lsb_bytes[0], 2);

				unsigned block_size = from_lsb2_u(lsb_bytes);

				ArrayAllocator<char> block_allocator(block_size);
				char *block = block_allocator.getArray();

				file->read(block, block_size);

				bit_memory_stream block_bits(block, block_size * 8);

				auto block_sample_count = sample_length - offset;

				if (block_sample_count > max_block_sample_count)
					block_sample_count = max_block_sample_count;

				auto block_samples_left = block_sample_count;

				int bit_width = initial_bit_width;

				/* set up integrator buffers */
				int integrator = sample_bias;
				int integrator2 = sample_bias;

				if (double_delta)
					integrator = 0;

				while (block_samples_left > 0)
				{
					/* read bits */
					int encoded_value = block_bits.read_int(bit_width);

					// check for changes in the bit width
					if (bit_width <= 6) // Type A: 1 <= bit_width <= 6
					{
						if (encoded_value == int(bit_value[bit_width - 1]))
						{
							/* yes -> read new width; */
							encoded_value = block_bits.read_int(bits_to_encode_bit_width) + 1;

							/* and expand it */
							if (int(encoded_value) >= bit_width)
								encoded_value++;

							bit_width = int(encoded_value);
							continue;
						}
					}
					else if (bit_width <= bits_per_sample) // Type B: 7 <= bit_width <= bits_per_sample
					{
						int field = 0xFFFF >> (17 - bit_width);

						int border = field - (bits_per_sample >> 1);

						// check for range indicating new width
						if ((encoded_value > border) && (encoded_value <= border + bits_per_sample))
						{
							// yes -> calculate new width;
							int new_bit_width = (int)(encoded_value - border);

							// and expand it
							if (new_bit_width >= bit_width)
								new_bit_width++;

							bit_width = new_bit_width;
							continue;
						}
					}
					else if (bit_width == bits_per_sample + 1) // Type C: bit_width == bits_per_sample + 1
					{
						int border = int(bit_value[bits_per_sample]);

						if (encoded_value & border)
						{
							int new_bit_width = (encoded_value + 1) & 0xFF;

							bit_width = new_bit_width;
							continue;
						}
					}
					else
						throw "invalid bit width";

					/* now expand value to signed byte */
					if (bit_width < 32)
					{
						// Fill the top of the 32-bit integer with the top bit of the bit_width value
						int unused_bit_count = 32 - bit_width;

						encoded_value = encoded_value << unused_bit_count >> unused_bit_count;
					}

					/* integrate upon the sample values */
					integrator += encoded_value;

					if (double_delta)
					{
						integrator2 += integrator;
						ret[offset++] = T(integrator2);
					}
					else
						ret[offset++] = T(integrator);

					block_samples_left--;
				}
			}

			return ret;
		}

		template <class T>
		void load_it_sample_uncompressed(istream *file, unsigned int channels, unsigned int sample_length, it_sample_conversion_flags &conversion, T *data[])
		{
			for (unsigned i=0; i<channels; i++)
				data[i] = new T[sample_length];

			unsigned total_byte_length = sample_length * channels * sizeof(T);

			ArrayAllocator<unsigned char> data_block_allocator(total_byte_length);
			unsigned char *data_block = data_block_allocator.getArray();

			file->read((char *)&data_block[0], total_byte_length);

			long offset = 0;

			for (unsigned i=0; i<sample_length; i++)
				for (unsigned j=0; j<channels; j++)
				{
					switch (sizeof(T))
					{
						case 1:
							if (conversion.signed_samples())
								data[j][i] = (signed char)data_block[offset++];
							else
								data[j][i] = (signed char)(data_block[offset++] - 0x80);
							break;
						case 2:
							if (conversion.msb_order())
								if (conversion.signed_samples())
									data[j][i] = T(from_msb2(&data_block[offset]));
								else
									data[j][i] = T(from_msb2_u(&data_block[offset]) - 0x8000);
							else
								if (conversion.signed_samples())
									data[j][i] = T(from_lsb2(&data_block[offset]));
								else
									data[j][i] = T(from_lsb2_u(&data_block[offset]) - 0x8000);
							offset += 2;
							break;
					}
				}
		}

		sample *load_it_sample(istream *file, unsigned int file_base_offset, int i, it_created_with_tracker &cwt)
		{
			bool new_format = cwt.compatible_exceeds(2.15);

			char magic[4];

			file->read(magic, 4);
			if (string(magic, 4) != "IMPS")
			{
				cerr << "Warning: sample #" << i << " does not have a valid header" << endl;
				return new sample_builtintype<signed char>(i, 1, 1.0);
			}

			char dos_filename[13];
			file->read(dos_filename, 13);

			if (dos_filename[12] != 0)
			{
				dos_filename[12] = 0;
				cerr << "Warning: DOS filename not properly null-terminated in sample #" << i << endl;
			}

			file->get(); // global volume -- TODO?

			it_sample_flags flags;
			flags.value = char(file->get());

			if (flags.sample_present() == false)
				return new sample_builtintype<signed char>(i, 1, 1.0);

			double default_volume = file->get();

			char sample_name[27];
			file->read(sample_name, 26);
			sample_name[26] = 0;

			it_sample_conversion_flags conversion;
			conversion.value = char(file->get());

			file->get(); // default panning -- TODO?

			unsigned long sample_length, loop_begin, loop_end, c5spd;
			unsigned long susloop_begin, susloop_end, sample_pointer;

			LoopStyle::Type loop_style, susloop_style;

			unsigned char lsb_bytes[4];

			file->read((char *)&lsb_bytes[0], 4);
			sample_length = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_begin = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_end = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			c5spd = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			susloop_begin = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			susloop_end = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			sample_pointer = from_lsb4_lu(lsb_bytes);

			loop_style = flags.pingpong_loop() ? LoopStyle::PingPong : LoopStyle::Forward;
			susloop_style = flags.pingpong_sustain_loop() ? LoopStyle::PingPong : LoopStyle::Forward;

			file->get(); // auto vibrato speed -- TODO?
			file->get(); // auto vibrato depth -- TODO?

			char auto_vibrato_waveform_char = char(file->get());

			file->get(); // auto vibrato rate -- TODO?

			Waveform::Type auto_vibrato_waveform;

			switch (auto_vibrato_waveform_char)
			{
				case 3:
				case 0: auto_vibrato_waveform = Waveform::Sine;     break;
				case 1: auto_vibrato_waveform = Waveform::RampDown; break;
				case 2: auto_vibrato_waveform = Waveform::Square;   break;
				//case 3: auto_vibrato_waveform = Waveform::Random;   break;
			}

			default_volume /= 64.0;

			sample *ret;

			unsigned channels_from_file = flags.stereo() ? 2 : 1;

			if (flags.looping() == false)
				loop_end = LOOP_END_NO_LOOP;
			if (flags.sustain_loop() == false)
				susloop_end = LOOP_END_NO_LOOP;

			file->seekg(streamoff(file_base_offset + sample_pointer));

			if (flags.compressed())
			{
				if (flags.stereo())
				{
					cerr << "Unable to load sample #" << i << ": compression on stereo samples is not defined" << endl;
					return new sample_builtintype<signed char>(i, 1, 1.0);
				}

				if (flags.sixteen_bit())
				{
					signed short *data;
					try
					{
						data = load_it_sample_compressed<signed short>(file, sample_length, new_format, conversion.signed_samples());
					}
					catch (const char *msg)
					{
						cerr << "Unable to load sample #" << i << ": cannot decode compressed format (" << msg << ")" << endl;
						return new sample_builtintype<signed char>(i, 1, 1.0);
					}

					auto new_sample = new sample_builtintype<signed short>(i, 1, default_volume, &data, sample_length, loop_style, susloop_style, loop_begin, loop_end, susloop_begin, susloop_end);

					ret = new_sample;
				}
				else
				{
					signed char *data;
					try
					{
						data = load_it_sample_compressed<signed char>(file, sample_length, new_format, conversion.signed_samples());
					}
					catch (const char *msg)
					{
						cerr << "Unable to load sample #" << i << ": cannot decode compressed format (" << msg << ")" << endl;
						return new sample_builtintype<signed char>(i, 1, 1.0);
					}

					auto new_sample = new sample_builtintype<signed char>(i, 1, default_volume, &data, sample_length, loop_style, susloop_style, loop_begin, loop_end, susloop_begin, susloop_end);

					ret = new_sample;
				}
			}
			else
			{
				if (flags.sixteen_bit())
				{
					signed short *data[MAX_CHANNELS];
					load_it_sample_uncompressed<signed short>(file, channels_from_file, sample_length, conversion, data);
					ret = new sample_builtintype<signed short>(i, int(channels_from_file), default_volume, data, sample_length, loop_style, susloop_style, loop_begin, loop_end, susloop_begin, susloop_end);
				}
				else
				{
					signed char *data[MAX_CHANNELS];
					load_it_sample_uncompressed<signed char>(file, channels_from_file, sample_length, conversion, data);
					ret = new sample_builtintype<signed char>(i, int(channels_from_file), default_volume, data, sample_length, loop_style, susloop_style, loop_begin, loop_end, susloop_begin, susloop_end);
				}
			}

			ret->samples_per_second = c5spd / 2.0; // impulse tracker octaves go lower than we can display!

			ret->name = sample_name;

			return ret;
		}

		struct it_pattern_mask
		{
			char value;

			bool has_note()                { return (0 != (value &   1)); }
			bool has_instrument()          { return (0 != (value &   2)); }
			bool has_volume_panning()      { return (0 != (value &   4)); }
			bool has_effect()              { return (0 != (value &   8)); }
			bool use_last_note()           { return (0 != (value &  16)); }
			bool use_last_instrument()     { return (0 != (value &  32)); }
			bool use_last_volume_panning() { return (0 != (value &  64)); }
			bool use_last_effect()         { return (0 != (value & 128)); }

			it_pattern_mask()
			{
				value = 0;
			}
		};

		struct it_pattern_slot
		{
			int note;
			unsigned int instrument;
			int volume_panning;
			int effect_command, effect_data;

			const static unsigned int NoInstrument = 0xFFFFFFFF;

			it_pattern_slot()
			{
				note = volume_panning = -1;
				effect_command = effect_data = 0;

				instrument = 0xFFFFFFFF;
			}
		};

		const unsigned char it_pattern_slide_table[10]
			= { 0, 1, 4, 8, 16, 32, 64, 96, 128, 255 };

		void load_it_pattern(istream *file, pattern &p, vector<sample *> &samps, bool has_note_events[MAX_MODULE_CHANNELS])
		{
			unsigned char lsb_data[2];

			file->read((char *)&lsb_data[0], 2);
			unsigned pattern_bytes = from_lsb2_u(lsb_data);

			file->read((char *)&lsb_data[0], 2);
			unsigned pattern_rows = from_lsb2_u(lsb_data);

			file->ignore(4);

			ArrayAllocator<char> data_allocator(pattern_bytes);
			char *data = data_allocator.getArray();

			file->read(&data[0], pattern_bytes);

			it_pattern_mask masks[64];

			it_pattern_slot last_row[64], cur_row[64];

			cerr << ".";

			for (unsigned i=0; i<pattern_rows; i++)
			{
				vector<row> rowdata(64);

				for (unsigned j=0; j<64; j++)
				{
					cur_row[j].note = -1;
					cur_row[j].instrument = it_pattern_slot::NoInstrument;
					cur_row[j].volume_panning = -1;
					cur_row[j].effect_command = -1;
				}

				while (true)
				{
					char channel_byte = *(data++);
					if (channel_byte == 0) // end of row
						break;

					unsigned channel = (channel_byte - 1) & 63;

					it_pattern_mask &mask = masks[channel];

					if (channel_byte & 128)
						mask.value = *(data++); // else use old value

					it_pattern_slot &c = cur_row[channel], &l = last_row[channel];

					if (mask.has_note())
						c.note = (unsigned char)*(data++);
					if (mask.has_instrument())
						c.instrument = unsigned(*(data++));
					if (mask.has_volume_panning())
						c.volume_panning = *(data++);
					if (mask.has_effect())
					{
						c.effect_command = (unsigned char)*(data++);
						c.effect_data = (unsigned char)*(data++);
					}
					if (mask.use_last_note())
						c.note = l.note;
					if (mask.use_last_instrument())
						c.instrument = l.instrument;
					if (mask.use_last_volume_panning())
						c.volume_panning = l.volume_panning;
					if (mask.use_last_effect())
					{
						c.effect_command = l.effect_command;
						c.effect_data = l.effect_data;
					}

					row &r = rowdata[channel];

					r.snote = snote_from_inote(c.note);
					r.znote = znote_from_snote(r.snote);

					if (r.snote >= 0)
						has_note_events[channel] = true;

					if ((c.instrument > 0) && (c.instrument <= samps.size()))
						r.instrument = samps[c.instrument - 1];
					else
						r.instrument = NULL;
					
					if (c.volume_panning >= 0)
					{
						if (c.volume_panning <= 64)
							r.volume = c.volume_panning;
						else if (c.volume_panning <= 74)
						{
							int param = c.volume_panning - 64;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::VolumeSlide, (unsigned char)((param << 4) | 0xF));
						}
						else if (c.volume_panning <= 84)
						{
							int param = c.volume_panning - 74;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::VolumeSlide, (unsigned char)(param | 0xF0));
						}
						else if (c.volume_panning <= 94)
						{
							int param = c.volume_panning - 84;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::VolumeSlide, (unsigned char)(param << 4));
						}
						else if (c.volume_panning <= 104)
						{
							int param = c.volume_panning - 94;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::VolumeSlide, (unsigned char)param);
						}
						else if (c.volume_panning <= 114)
						{
							int param = c.volume_panning - 104;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::PortamentoDown, it_pattern_slide_table[param]);
						}
						else if (c.volume_panning <= 124)
						{
							int param = c.volume_panning - 114;
							r.secondary_effect = effect_struct(EffectType::IT, Effect::PortamentoUp, it_pattern_slide_table[param]);
						}
						else if (c.volume_panning >= 128)
						{
							if (c.volume_panning <= 192)
							{
								int param = c.volume_panning - 128;
								r.secondary_effect = effect_struct(EffectType::IT, Effect::Panning, (unsigned char)param);
							}
							else if (c.volume_panning <= 202)
							{
								int param = c.volume_panning - 192;
								r.secondary_effect = effect_struct(EffectType::IT, Effect::TonePortamento, (unsigned char)param);
							}
							else if (c.volume_panning <= 212)
							{
								int param = c.volume_panning - 202;
								r.secondary_effect = effect_struct(EffectType::IT, Effect::Vibrato, (unsigned char)param); // TODO
							}
						}
					}

					r.effect = effect_struct(EffectType::IT, Effect::Type(c.effect_command), (unsigned char)(c.effect_data));

					if (r.effect.present)
						has_note_events[channel] = true;
				}

				for (int row_index=0; row_index<64; row_index++)
				{
					if (cur_row[row_index].note != -1)
						last_row[row_index].note = cur_row[row_index].note;
					if (cur_row[row_index].instrument != it_pattern_slot::NoInstrument)
						last_row[row_index].instrument = cur_row[row_index].instrument;
					if (cur_row[row_index].volume_panning != -1)
						last_row[row_index].volume_panning = cur_row[row_index].volume_panning;
					if (cur_row[row_index].effect_command != -1)
						last_row[row_index].effect_command = cur_row[row_index].effect_command,
						last_row[row_index].effect_data = cur_row[row_index].effect_data;
				}

				p.row_list.push_back(rowdata);
			}
		}

		void load_it_convert_envelope(instrument_envelope &target, it_envelope_description &source)
		{
			target.enabled = source.enabled;
			target.looping = source.looping;
			if (target.looping)
			{
				target.loop_begin_tick = source.envelope[source.loop_start_node].tick;
				target.loop_end_tick = source.envelope[source.loop_end_node].tick;
			}

			target.sustain_loop = source.sustain_loop;
			if (target.sustain_loop)
			{
				target.sustain_loop_begin_tick = source.envelope[source.sustain_loop_start_node].tick;
				target.sustain_loop_end_tick = source.envelope[source.sustain_loop_end_node].tick;
			}

			for (unsigned i=0; i < source.envelope.size(); i++)
				target.node.push_back(instrument_envelope_node(source.envelope[i].tick, source.envelope[i].yValue));
		}

		int dec_as_hex(unsigned char hexrep)
		{
			int hex_upper = (hexrep >> 4) & 15;
			int hex_lower = hexrep & 15;

			return hex_upper * 10 + hex_lower;
		}
	}

	module_struct *load_it(istream *file, bool /*modplug_style = false*/)
	{
		unsigned int file_base_offset = (unsigned int)file->tellg();

		char magic[4];
		file->read(magic, 4);

		if (string(magic, 4) != "IMPM")
			throw "invalid file format (missing 'IMPM' at start of file)";

		char songname[27];
		file->read(songname, 26);
		songname[26] = 0;

		file->ignore(2); // pattern highlight info

		unsigned char lsb_bytes[4];

		file->read((char *)&lsb_bytes[0], 2);
		unsigned order_list_length = from_lsb2_u(lsb_bytes);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned num_instruments = from_lsb2_u(lsb_bytes);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned num_samples = from_lsb2_u(lsb_bytes);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned num_patterns = from_lsb2_u(lsb_bytes);

		it_created_with_tracker cwt;

		file->read((char *)&lsb_bytes[0], 4);
		cwt.major_version = lsb_bytes[1];
		cwt.minor_version = dec_as_hex(lsb_bytes[0]);
		cwt.compatible_with_major_version = lsb_bytes[3];
		cwt.compatible_with_minor_version = dec_as_hex(lsb_bytes[2]);

		it_flags flags;

		file->read(&flags.value, 1);
		file->ignore(1);

		file->read((char *)&lsb_bytes[0], 2); // special

		it_settings settings;

		file->read((char *)&settings.global_volume, 1);
		file->read((char *)&settings.mix_volume, 1);
		file->read((char *)&settings.initial_speed, 1);
		file->read((char *)&settings.initial_tempo, 1);

		unsigned char panning_separation;
		file->read((char *)&panning_separation, 1);

		unsigned char midi_pitch_wheel_depth;
		file->read((char *)&midi_pitch_wheel_depth, 1);

		file->read((char *)&lsb_bytes[0], 2); // message length
		file->read((char *)&lsb_bytes[0], 4); // message offset
		file->read((char *)&lsb_bytes[0], 4); // reserved

		unsigned char initial_channel_pan[64];
		unsigned char initial_channel_volume[64];

		file->read((char *)&initial_channel_pan[0], 64);
		file->read((char *)&initial_channel_volume[0], 64);

		vector<unsigned char> order_table(order_list_length);

		for (unsigned i=0; i<order_list_length; i++)
		{
			unsigned char this_order;

			file->read((char *)&this_order, 1);
			order_table[i] = this_order;
		}
		
		vector<unsigned int> instrument_offset(num_instruments);
		vector<unsigned int> sample_header_offset(num_samples);
		vector<unsigned int> pattern_offset(num_patterns);

		for (unsigned i=0; i<num_instruments; i++)
		{
			file->read((char *)&lsb_bytes[0], 4);
			instrument_offset[i] = from_lsb4_lu(lsb_bytes);
		}
		for (unsigned i=0; i<num_samples; i++)
		{
			file->read((char *)&lsb_bytes[0], 4);
			sample_header_offset[i] = from_lsb4_lu(lsb_bytes);
		}
		for (unsigned i=0; i<num_patterns; i++)
		{
			file->read((char *)&lsb_bytes[0], 4);
			pattern_offset[i] = from_lsb4_lu(lsb_bytes);
		}

		{
			unsigned expected_offset = file_base_offset + 0xC0u + order_list_length + 4u * (num_instruments + num_samples + num_patterns);
			unsigned actual_offset = (unsigned)file->tellg();

			if (expected_offset != actual_offset)
				throw "internal error (file offset is not what it should be)";
		}

		vector<it_instrument_description> insts;

		for (unsigned i=0; i<num_instruments; i++)
		{
			file->seekg(file_base_offset + instrument_offset[i]);

			it_instrument_description desc;
			if (cwt.compatible_with_major_version < 2)
				load_it_old_instrument(file, desc, int(i));
			else
				load_it_new_instrument(file, desc, int(i));
			insts.push_back(desc);
		}

		vector<sample *> samps;

		for (unsigned i=0; i<num_samples; i++)
		{
			file->seekg(file_base_offset + sample_header_offset[i]);
			sample *smp = load_it_sample(file, file_base_offset, int(i), cwt);
			samps.push_back(smp);
		}

		int channel_count = flags.stereo() ? 2 : 1;

		if (flags.use_instruments()) // this conversion MUST be done before loading patterns
		{
			vector<sample *> instruments;

			for (unsigned i=0; i<num_instruments; i++)
			{
				it_instrument_description &id = insts[i];
				
				sample_instrument *is = new sample_instrument(int(i));

				is->global_volume = id.global_volume;
				is->default_pan.set_channels(channel_count).from_linear_pan(id.default_pan, 0, 64);
				is->use_default_pan = id.use_default_pan;

				is->pitch_pan_center = id.pitch_pan_center;
				is->pitch_pan_separation = id.pitch_pan_separation;

				is->volume_variation_pctg = id.volume_variation_pctg;
				is->panning_variation = id.panning_variation;

				is->fade_out = id.fade_out;

				is->duplicate_note_check = id.duplicate_note_check;
				is->duplicate_check_action = id.duplicate_check_action;
				is->new_note_action = id.new_note_action;

				for (unsigned note_index=0; note_index<120; note_index++)
				{
					is->tone_offset[note_index] = id.tone_offset[note_index];
					if (id.note_sample[note_index])
						is->note_sample[note_index] = samps[unsigned(id.note_sample[note_index] - 1)];
					else
						is->note_sample[note_index] = NULL;
				}

				load_it_convert_envelope(is->volume_envelope, id.volume_envelope);
				load_it_convert_envelope(is->panning_envelope, id.pan_envelope);
				load_it_convert_envelope(is->pitch_envelope, id.pitch_envelope);

				instruments.push_back(is);
			}

			samps = instruments;
		}

		vector<pattern> pats;
		bool has_note_events[MAX_MODULE_CHANNELS] = { false }; // entire struct to 0

		cerr << "Loading " << num_patterns << " patterns" << endl;
		cerr << '.' << string(num_patterns, '-') << '.' << endl << ' ';
		for (unsigned i=0; i<num_patterns; i++)
		{
			file->seekg(file_base_offset + pattern_offset[i]);

			pattern pat((int)i);
			load_it_pattern(file, pat, samps, has_note_events);
			pats.push_back(pat);
		}
		cerr << endl << endl;

		module_struct *ret = new module_struct();

		ret->name = songname;

		ret->stereo = flags.stereo();

		ret->patterns = pats;
		ret->samples = samps;

		for (unsigned i=0; i<order_list_length; i++)
		{
			if (order_table[i] == 254) //  ++ (skip)
			{
				ret->pattern_list.push_back(NULL);
				continue;
			}
			if (order_table[i] == 255) //  -- (end of tune)
				break;
			ret->pattern_list.push_back(&ret->patterns[order_table[i]]);
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
				if (ret->stereo)
				{
					ret->initial_panning[i].set_channels(2);

					ret->base_pan[i] = initial_channel_pan[i];
					ret->initial_panning[i].from_amiga_pan(ret->base_pan[i]);
				}
			}
			else
				ret->channel_enabled[i] = false;
		}

		ret->num_channels = count;

		ret->speed = settings.initial_speed;
		ret->tempo = settings.initial_tempo;

		ret->it_module = true;
		ret->it_module_new_effects = !flags.old_effects();
		ret->it_module_portamento_link = flags.portamento_memory_link();
		ret->it_module_linear_slides = flags.linear_slides();

		return ret;
	}
}
