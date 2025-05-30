#include <cstddef>
#include <string>

using namespace std;

#include "sample.h"

#include "channel.h"
#include "envelope.h"
#include "one_sample.h"

namespace MultiPLAY
{
	// struct sample_context
	sample_context::sample_context(sample *cw) : created_with(cw) { znote = -1; }
	/*virtual*/ sample_context::~sample_context() {}

	sample_context *sample_context::clone()
	{
		sample_context *ret = create_new();

		copy_to(ret);

		return ret;
	}

	/*virtual*/ bool sample_context::is_sample_match(sample *other)
	{
		return (other == created_with);
	}

	/*virtual*/ sample_context *sample_context::create_new()
	{
		return new sample_context(nullptr);
	}

	/*virtual*/ void sample_context::copy_to(sample_context *other)
	{
		other->created_with = this->created_with;
		other->samples_per_second = this->samples_per_second;
		other->default_volume = this->default_volume;
		other->num_samples = this->num_samples;
		other->vibrato_sweep_ticks = this->vibrato_sweep_ticks;
	}

	// struct sample
	sample::sample(int idx)
		: use_vibrato(false), index(idx + 1)
	{
		global_volume = 1.0;

		duplicate_note_check = DuplicateCheck::Off;
		duplicate_note_action = DuplicateCheckAction::Cut;
	}

	/*virtual*/ void sample::detect_false_looping()
	{
	}

	/*virtual*/ sample *sample::get_root_sample(int inote)
	{
		return this;
	}

	/*virtual*/ void sample::set_global_volume(double new_global_volume)
	{
		global_volume = new_global_volume;
	}

	/*virtual*/ const pan_value &sample::get_default_pan(const pan_value &channel_default)
	{
		return channel_default;
	}
}
