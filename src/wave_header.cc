#include "wave_header.h"

#include <fstream>

using namespace std;

namespace MultiPLAY
{
	wave_header::wave_header()
	{
		enabled = false;
		samples_per_second = 44100;
		num_channels = 1;
	}

	wave_header::~wave_header()
	{
		finalize();
	}

	void wave_header::enable()
	{
		enabled = true;
	}

	void wave_header::set_samples_per_second(int new_samples_per_second)
	{
		samples_per_second = new_samples_per_second;
	}

	void wave_header::set_bits_per_sample(int new_bits_per_sample)
	{
		bits_per_sample = new_bits_per_sample;
	}

	void wave_header::set_num_channels(int new_num_channels)
	{
		num_channels = new_num_channels;
	}

	#define HEADER_TEMPLATE_1 \
		"RIFF"         /* offset 0, fourcc that identifies the file */ \
		"\0\0\0\0"     /* offset 4, int, overall size of file */       \
		"WAVE"         /* offset 8, file type header */                \
		"fmt "         /* offset 12, format chunk marker */            \
		"\x10\0\0\0"   /* offset 16, int, length of format data */     \
		"\x01"/*\0*/   /* offset 20, short, data type (1 == PCM) */

		               /* offset 22, short, number of channels */
		               /* offset 24, int, sample rate */
		               /* offset 28, int, bits per second */
		               /* offset 32, short, bytes per sample (all channels) */
		               /* offset 34, short, bits per sample (one channel) */
	#define HEADER_TEMPLATE_2                                   \
		"data"         /* offset 36, data chunk marker */         \
		"\0\0\0"/*\0*/ /* offset 40, size of the data section */

	namespace
	{
		void put_short(ofstream *str, short value)
		{
			str->put((char)(value & 0xFF));
			str->put((char)((value >> 8) & 0xFF));
		}

		void put_int(ofstream *str, int value)
		{
			str->put((char)(value & 0xFF));
			str->put((char)((value >> 8) & 0xFF));
			str->put((char)((value >> 16) & 0xFF));
			str->put((char)((value >> 24) & 0xFF));
		}
	}

	void wave_header::begin(std::ofstream *new_output)
	{
		if (in_progress)
			throw L"already writing a wave file header";

		if (enabled)
		{
			output = new_output;

			header_offset = output->tellp();

			int bytes_per_sample = bits_per_sample / 8;
			int bytes_per_sample_set = num_channels * bytes_per_sample;
			int bits_per_second = samples_per_second * bytes_per_sample_set * 8;

			output->write(HEADER_TEMPLATE_1, sizeof(HEADER_TEMPLATE_1));

			put_short(output, (short)num_channels);
			put_int(output, samples_per_second);
			put_int(output, bits_per_second);
			put_short(output, bytes_per_sample_set);
			put_short(output, bits_per_sample);

			output->write(HEADER_TEMPLATE_2, sizeof(HEADER_TEMPLATE_2));

			header_length = output->tellp() - header_offset;
		}

		in_progress = true;
	}

	void wave_header::finalize()
	{
		if (in_progress)
		{
			if (enabled)
			{
				ofstream::pos_type end_offset = output->tellp();

				int overall_length = end_offset - header_offset;
				int data_section_length = overall_length - header_length;

				output->seekp(header_offset);
				output->seekp(4, ios::cur);

				put_int(output, overall_length);

				output->seekp(32, ios::cur);

				put_int(output, data_section_length);

				output->seekp(end_offset);
			}

			in_progress = false;
		}
	}
}
