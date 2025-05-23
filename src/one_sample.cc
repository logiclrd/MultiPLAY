#include "one_sample.h"

namespace MultiPLAY
{
	// struct pan_value
	pan_value::pan_value(int num_channels/*=1*/)
	{
		num_scales = num_channels;
		for (int i=0; i<num_channels; i++)
			sample_scale[i] = 1.0;
		for (int i=num_channels; i < MAX_CHANNELS; i++)
			sample_scale[i] = 0.0;
	}

	pan_value::pan_value(double position)
	{
		num_scales = 2;

		from_linear_pan(position, -1.0, +1.0);
	}

	/*static*/ pan_value pan_value::from_exponential(double position)
	{
		pan_value ret(2);

		if (position > 0)
			ret.sample_scale[0] = exp(-position);
		if (position < 0)
			ret.sample_scale[1] = exp(position);

		return ret;
	}
}
