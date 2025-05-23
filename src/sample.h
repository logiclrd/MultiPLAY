#ifndef SAMPLE_H
#define SAMPLE_H

#include <cstddef>
#include <string>

using namespace std;

#include "envelope.h"
#include "one_sample.h"

namespace MultiPLAY
{
	// TODO: #include "row.h", "channel.h"
	struct row;
	struct channel;

	namespace LoopStyle
	{
		enum Type
		{
			Undefined,

			Forward,
			PingPong,
		};
	}

	struct sample;

	struct sample_context
	{
		sample *created_with;
		double samples_per_second;
		double default_volume;
		unsigned int num_samples;

		// "fade" the vibrato in over this many ticks, calculated at
		// note start from sample.vibrato_sweep_frames and speed
		long vibrato_sweep_ticks;

		sample_context(sample *cw);
		virtual ~sample_context();

		sample_context *clone();

		virtual sample_context *create_new();
		virtual void copy_to(sample_context *other);
	};

	struct sample
	{
		int index;
		string name;

		double fade_out;

		unsigned int num_samples;
		double samples_per_second;

		bool use_vibrato;
		double vibrato_depth;
		double vibrato_cycle_frequency; // frequency relative to samples with samples_per_second per second
		int vibrato_sweep_frames;

		virtual const pan_value &get_default_pan(const pan_value &channel_default);
		virtual one_sample get_sample(unsigned int sample, double offset, sample_context *c = nullptr) = 0;
		virtual void begin_new_note(row *r = nullptr, channel *p = nullptr, sample_context **c = nullptr, double effect_tick_length = 0.0, bool top_level = true, int *znote = nullptr, bool is_primary = true) = 0;
		virtual void occlude_note(channel *p = nullptr, sample_context **c = nullptr, sample *new_sample = nullptr, row *r = nullptr) = 0;
		virtual void exit_sustain_loop(sample_context *c = nullptr) = 0; //..if it has one :-)
		virtual void kill_note(sample_context *c = nullptr) = 0;
		virtual bool past_end(unsigned int sample, double offset, sample_context *c = nullptr) = 0;

		sample(int idx);
	};
}

#endif // SAMPLE_H
