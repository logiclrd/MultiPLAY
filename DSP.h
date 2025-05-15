#ifndef DSP_H
#define DSP_H

#include <vector>

#include "one_sample.h"

namespace MultiPLAY
{
	template <int buffer_size>
	struct rotating_sample_buffer
	{
		int offset;
		one_sample buffer[buffer_size];

		rotating_sample_buffer() {}

		void turn()
		{
			offset = (offset + 1) % buffer_size;
		}

		one_sample &operator[](int index)
		{
			index = (index + offset) % buffer_size;
			return buffer[index];
		}
	};

	struct DSP
	{
		std::vector<double> coefficient;
		rotating_sample_buffer<2> out_data;

		double current_cutoff_frequency;
		int current_it_cutoff_value;

		double current_reverb_response;

		double iir_input_coefficient;
		double iir_output_coefficient_1, iir_output_coefficient_2;

		one_sample result;

		DSP();
		DSP(double cutoff_frequency, double reverb_response, double sampling_rate);

		one_sample &compute_next(one_sample &input);
		static int compute_it_cutoff_value(double cutoff_frequency);
		static double compute_it_cutoff_frequency(int it_cutoff_value, int modifier, bool extended_filter_range);
		static double compute_it_reverb_response(int it_reverb_value);

		void setup_filter(int it_cutoff_value, int it_reverb_value, int modifier, bool extended_filter_range);
		void setup_filter(double cutoff_frequency, double reverb_response);
	};
}

#endif // DSP_H