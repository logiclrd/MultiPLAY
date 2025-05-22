#ifndef WAVE_HEADER_H
#define WAVE_HEADER_H

#include <fstream>

namespace MultiPLAY
{
	struct wave_header
	{
		bool enabled;
		int samples_per_second;
		int num_channels;
		int bits_per_sample;
		std::ofstream *output;
		std::ofstream::pos_type header_offset;
		std::ofstream::off_type header_length;
		bool in_progress;

		wave_header();
		~wave_header();

		void enable();
		void set_samples_per_second(int new_samples_per_second);
		void set_bits_per_sample(int new_bits_per_sample);
		void set_num_channels(int new_num_channels);
		void begin(std::ofstream *new_output);
		void finalize();
	};
}

#endif // WAVE_HEADER_H