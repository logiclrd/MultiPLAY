#include "sample_builtintype.h"

namespace MultiPLAY
{
	// struct sample_builtintype_context
	sample_builtintype_context::sample_builtintype_context(sample *cw) : sample_context(cw) { }

	/*virtual*/ sample_context *sample_builtintype_context::create_new()
	{
		return new sample_builtintype_context(NULL);
	}

	/*virtual*/ void sample_builtintype_context::copy_to(sample_context *other)
	{
		sample_builtintype_context *target = dynamic_cast<sample_builtintype_context *>(other);

		if (target == NULL)
			throw "Copy sample context to wrong type";

		sample_context::copy_to(target);

		target->last_looped_sample = this->last_looped_sample;
		target->sustain_loop_exit_difference = this->sustain_loop_exit_difference;
		target->sustain_loop_exit_sample = this->sustain_loop_exit_sample;
		target->sustain_loop_state = this->sustain_loop_state;
	}
}
