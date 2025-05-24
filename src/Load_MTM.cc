#include "Load_MTM.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

#include "RAII.h"

using namespace RAII;

#include "conversion.h"
#include "module.h"
#include "mod_finetune.h"
#include "notes.h"
#include "sample_builtintype.h"

namespace MultiPLAY
{
	namespace
	{
		struct mtm_sample_description
		{
			string sample_name;
			unsigned long byte_length, loop_start, loop_end;
			unsigned char finetune;
			unsigned char default_volume;
			bool sixteen_bit;
		};

		struct mtm_track_data
		{
			unsigned char pitch;
			unsigned char instrument;
			MODEffect::Type effect;
			unsigned char effect_param;
		};

		struct mtm_pattern_data
		{
			vector<mtm_track_data> *channel[32];
		};

		int snote_from_mtm_pitch(int pitch)
		{
			if (pitch == 0)
				return -1;
			else
			{
				int note = pitch % 12;
				int octave = pitch / 12 + 2;

				return (octave << 4) | note;
			}
		}
	}

	module_struct *load_mtm(istream *file)
	{
		char magic[4];
		file->read(magic, 3);
		magic[3] = 0;
		
		if (string(magic) != "MTM")
			throw "invalid file format (missing 'MTM' at start of file)";

		unsigned char version;
		file->read((char *)&version, 1);

		char songname[21];
		file->read(songname, 20);
		songname[20] = 0;

		unsigned char lsb_bytes[4];
		file->read((char *)&lsb_bytes[0], 2);

		unsigned num_tracks = from_lsb2_u(lsb_bytes);

		unsigned char last_pattern, last_order;
		file->read((char *)&last_pattern, 1);
		file->read((char *)&last_order, 1);

		file->read((char *)&lsb_bytes[0], 2);
		unsigned extra_comment_length = from_lsb2_u(lsb_bytes);

		unsigned char num_samples;
		file->read((char *)&num_samples, 1);

		unsigned char attribute;
		file->read((char *)&attribute, 1);

		unsigned char beats_per_track;
		file->read((char *)&beats_per_track, 1);

		unsigned char num_playback_tracks;
		file->read((char *)&num_playback_tracks, 1);

		unsigned char pan_position[32];
		file->read((char *)&pan_position[0], 32);

		vector<mtm_sample_description> sample_description(num_samples);

		for (unsigned i=0; i<num_samples; i++)
		{
			char sample_name[23];
			file->read(sample_name, 22);
			sample_name[22] = 0;

			unsigned long bytes, loop_begin, loop_end;

			file->read((char *)&lsb_bytes[0], 4);
			bytes = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_begin = from_lsb4_lu(lsb_bytes);

			file->read((char *)&lsb_bytes[0], 4);
			loop_end = from_lsb4_lu(lsb_bytes);

			unsigned char finetune;
			file->read((char *)&finetune, 1);

			unsigned char default_volume;
			file->read((char *)&default_volume, 1);

			unsigned char sample_attribute;
			file->read((char *)&sample_attribute, 1);

			mtm_sample_description description;

			description.sample_name = sample_name;
			description.byte_length = bytes;
			description.loop_start = loop_begin;
			description.loop_end = loop_end;
			description.finetune = finetune;
			description.default_volume = default_volume;
			description.sixteen_bit = (0 != (sample_attribute & 1));

			sample_description[i] = description;
		}

		unsigned char order_table[128];
		file->read((char *)&order_table[0], 128);

		vector<vector<mtm_track_data> > tracks;
		unsigned char track_data[192];

		for (unsigned i=0; i<num_tracks; i++)
		{
			vector<mtm_track_data> track;

			file->read((char *)&track_data[0], 192);

			for (int row_index=0, o=0; row_index<64; row_index++, o += 3)
			{
				mtm_track_data row;

				row.pitch = track_data[o] >> 2;
				row.instrument =
					((track_data[o] & 0x3) << 4) |
					(track_data[o+1] >> 4);
				row.effect = (MODEffect::Type)(track_data[o+1] & 0xF);
				row.effect_param = track_data[o+2];

				track.push_back(row);
			}

			tracks.push_back(track);
		}

		vector<mtm_pattern_data> patterns;

		for (int i=0; i<=last_pattern; i++)
		{
			mtm_pattern_data pattern;

			for (int j=0; j<32; j++)
			{
				file->read((char *)&lsb_bytes[0], 2);

				unsigned track = from_lsb2_u(lsb_bytes);

				if (track > 0)
					pattern.channel[j] = &tracks[unsigned(track - 1)];
				else
					pattern.channel[j] = nullptr;
			}

			patterns.push_back(pattern);
		}

		file->ignore(extra_comment_length); // skip the comment

		vector<sample *> samps;

		for (unsigned i=0; i<num_samples; i++)
		{
			if (sample_description[i].byte_length == 0)
			{
				samps.push_back(new sample_builtintype<signed char>(int(i), 1, 1.0));
				continue;
			}

			if (sample_description[i].sixteen_bit)
			{
				sample_builtintype<signed short> *smp = new sample_builtintype<signed short>(int(i), 1, 1.0);

				smp->name = sample_description[i].sample_name;

				smp->num_samples = sample_description[i].byte_length >> 1;

				if (sample_description[i].loop_end - sample_description[i].loop_start > 2)
				{
					smp->loop_begin = sample_description[i].loop_start;
					smp->loop_end = sample_description[i].loop_end;
				}

				ArrayAllocator<unsigned char> data_allocator(smp->num_samples * 2);
				unsigned char *data = data_allocator.getArray();

				signed short *data_sgn = new signed short[smp->num_samples];

				file->read((char *)&data[0], smp->num_samples * 2);

				for (unsigned j=0; j < smp->num_samples; j++)
				{
					data_sgn[j] = (signed short)(from_lsb2_u(data + (j << 1)));
				}

				smp->sample_data[0] = data_sgn;

				smp->samples_per_second = mod_finetune[8 ^ sample_description[i].finetune];

				smp->default_volume = sample_description[i].default_volume / 64.0;

				samps.push_back(smp);

				if (sample_description[i].byte_length & 1)
				{
					cerr << "Warning: 16-bit MTM sample length is not even" << endl;
					file->ignore(1);
				}
			}
			else
			{
				sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(int(i), 1, 1.0);

				smp->num_samples = sample_description[i].byte_length;

				if (sample_description[i].loop_end - sample_description[i].loop_start > 2)
				{
					smp->loop_begin = sample_description[i].loop_start;
					smp->loop_end = sample_description[i].loop_end;
				}

				unsigned char *data = new unsigned char[smp->num_samples];

				file->read((char *)&data[0], smp->num_samples);

				signed char *data_sgn = (signed char *)data;

				for (unsigned j=0; j<smp->num_samples; j++)
					data_sgn[j] = (signed char)(int(data[j]) - 128); // unbias data

				smp->sample_data[0] = data_sgn;

				smp->samples_per_second = mod_finetune[8 ^ sample_description[i].finetune];

				smp->default_volume = sample_description[i].default_volume / 64.0;

				samps.push_back(smp);
			}
		}

		// convert track data to pattern data

		vector<pattern> pats;

		bool channel_used[32] = { false }; // clear array

		for (unsigned i=0; i <= last_pattern; i++)
		{
			pattern new_pattern((int)i);

			vector<mtm_track_data> *track[32];

			for (unsigned j=0; j<32; j++)
			{
				track[j] = patterns[i].channel[j];
				if (track[j])
					channel_used[j] = true;
			}
	
			for (unsigned j=0; j<64; j++)
			{
				vector<row> rowdata(32);
				
				for (unsigned k=0; k<32; k++)
					if (track[k])
					{
						mtm_track_data &m = track[k][0][j];
						row &r = rowdata[k];

						r.snote = snote_from_mtm_pitch(m.pitch);
						r.znote = znote_from_snote(r.snote);

						if ((m.instrument > 0) && (m.instrument < int(samps.size())))
							r.instrument = samps[m.instrument - 1];
						else
							r.instrument = nullptr;

						r.effect.init(m.effect, m.effect_param, &r);
					}

				new_pattern.row_list.push_back(rowdata);
			}

			pats.push_back(new_pattern);
		}

		module_struct *ret = new module_struct();

		ret->name = songname;
		ret->num_channels = 32;
		ret->speed = 6;
		ret->tempo = 125;
		ret->stereo = true;

		for (unsigned i=0; i<32; i++)
		{
			ret->channel_enabled[i] = channel_used[i];
			ret->channel_map[i] = i;

			ret->base_pan[i] = pan_position[i];
			ret->initial_panning[i].from_s3m_pan(ret->base_pan[i]);
		}

		for (int i=32; i<MAX_MODULE_CHANNELS; i++)
			ret->channel_enabled[i] = false;

		ret->speed_change();

		for (size_t i=0; i < samps.size(); i++)
			ret->information_text.push_back(samps[i]->name);

		ret->samples = samps;
		ret->patterns = pats;

		for (int i=0; i<=last_order; i++)
			ret->pattern_list.push_back(&ret->patterns[order_table[i]]);

		ret->amiga_panning = true;

		return ret;
	}
}
