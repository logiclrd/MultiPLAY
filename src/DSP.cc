#include "DSP.h"

#include "MultiPLAY.h"

#include "math.h"

namespace MultiPLAY
{
	// struct DSP
	DSP::DSP()
	{
		current_cutoff_frequency = 30000;
		current_it_cutoff_value = compute_it_cutoff_value(current_cutoff_frequency);
	}

	DSP::DSP(double cutoff_frequency, double /*reverb_response*/, double /*sampling_rate*/)
	{
		current_cutoff_frequency = cutoff_frequency;
		current_it_cutoff_value = compute_it_cutoff_value(cutoff_frequency);
	}

	one_sample &DSP::compute_next(one_sample &input)
	{
		result = input * iir_input_coefficient
		+ out_data[0] * iir_output_coefficient_1 
		+ out_data[1] * iir_output_coefficient_2;

		out_data.turn();
		out_data[0] = result;

		return result;
	}

	/*static*/ int DSP::compute_it_cutoff_value(double cutoff_frequency)
	{
		static double base_frequency = 110.0 * 1.189207115002721;
		double p2_value = cutoff_frequency / base_frequency;
		double exponent = lg(p2_value) / 256.0;
		int it_cutoff_value = (int)(24.0 * 512.0 * exponent);

		return it_cutoff_value;
	}

	/*static*/ double DSP::compute_it_cutoff_frequency(int it_cutoff_value, int modifier, bool extended_filter_range)
	{
		static double nyquist = ticks_per_second / 2.0;
		static double base_frequency = 110.0 * 1.189207115002721;

		double exponent;
		if (extended_filter_range)
			exponent = it_cutoff_value / (21.0 * 512.0);
		else
			exponent = it_cutoff_value / (24.0 * 512.0);
		
		double cutoff_frequency = base_frequency * p2(exponent * (modifier + 256));

		if (cutoff_frequency < 120.0)
			return 120.0;
		if (cutoff_frequency > 10000.0)
			return 10000.0;
		if (cutoff_frequency > nyquist)
			return nyquist;
		return cutoff_frequency;
	}

	/*static*/ double DSP::compute_it_reverb_response(int it_reverb_value)
	{
		static double base = 1.122018454301963;
		static double log_base = log(base);
		double exponent = it_reverb_value * (-24.0 / 128.0);
		return exp(log_base * exponent);
	}

	void DSP::setup_filter(int it_cutoff_value, int it_reverb_value, int modifier, bool extended_filter_range)
	{
		if (it_cutoff_value != current_it_cutoff_value)
		{
			current_it_cutoff_value = it_cutoff_value;
			current_cutoff_frequency = compute_it_cutoff_frequency(it_cutoff_value, modifier, extended_filter_range);
		}

		current_reverb_response = compute_it_reverb_response(it_reverb_value);

		setup_filter(current_cutoff_frequency, current_reverb_response);
	}

	void DSP::setup_filter(double cutoff_frequency, double reverb_response)
	{
		static double samples_per_sec = ticks_per_second;
		static double frequency_to_radians = (3.14159265358979323 * 2.0) / samples_per_sec;

		double cutoff_radian = cutoff_frequency * frequency_to_radians;

		double d = (1.0 - reverb_response) * cutoff_radian;

		if (d > 2.0)
			d = 2.0;

		d = (reverb_response - d) / cutoff_radian;

		double e = 1.0 / cutoff_radian;
		e *= e;

		iir_input_coefficient = 1.0 / (1.0 + d + e);
		iir_output_coefficient_1 = (d + e + e) * iir_input_coefficient;
		iir_output_coefficient_2 = -e * iir_input_coefficient;
	}
}
