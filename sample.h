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
		int num_samples;

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

		int fade_out;

		int num_samples;
		double samples_per_second;

		bool use_vibrato;
		double vibrato_depth, vibrato_cycle_frequency; // frequency relative to samples with samples_per_second per second

		virtual one_sample get_sample(int sample, double offset, sample_context *c = NULL) = 0;
		virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **c = NULL, double effect_tick_length = 0.0, bool top_level = true, int *znote = NULL) = 0;
		virtual void occlude_note(channel *p = NULL, sample_context **c = NULL, sample *new_sample = NULL, row *r = NULL) = 0;
		virtual void exit_sustain_loop(sample_context *c = NULL) = 0; //..if it has one :-)
		virtual void kill_note(sample_context *c = NULL) = 0;
		virtual bool past_end(int sample, double offset, sample_context *c = NULL) = 0;

		sample(int idx);
	};
}

#endif // SAMPLE_H
