#ifndef SAMPLE_BUILTINTYPE_H
#define SAMPLE_BUILTINTYPE_H

#include <cstddef>
#include <string>

using namespace std;

#include "channel.h"
#include "envelope.h"
#include "one_sample.h"
#include "sample.h"

namespace MultiPLAY
{
	struct sample_builtintype_context : sample_context
	{
		int last_looped_sample;

		int sustain_loop_exit_sample;
		int sustain_loop_exit_difference;

		SustainLoopState::Type sustain_loop_state;

		sample_builtintype_context(sample *cw);

		virtual sample_context *create_new();
		virtual void copy_to(sample_context *other);
	};

	template <class T>
	struct sample_builtintype : public sample
	{
		T *sample_data[MAX_CHANNELS];
		int sample_channels;

		double sample_scale;
		double default_volume;

		long loop_begin, loop_end;
		long sustain_loop_begin, sustain_loop_end;

		LoopStyle::Type loop_style, sustain_loop_style;

		bool use_sustain_loop;

		sample_builtintype(int index, int sample_channels,
											T **data = NULL, int num_samples = 0,
											long loop_begin = 0, long loop_end = 0xFFFFFFFF,
											long susloop_begin = 0, long susloop_end = 0xFFFFFFFF,
											LoopStyle::Type loop_style = LoopStyle::Forward,
											LoopStyle::Type sustain_loop_style = LoopStyle::Forward)
			: sample(index)
		{
			switch (sizeof(T))
			{
				case 1: sample_scale = 1.0 / 127.5;        break;
				case 2: sample_scale = 1.0 / 32767.5;      break;
				case 4: sample_scale = 1.0 / 2147483647.5; break;
				default: throw "could not deduce sample scale";
			}

			this->sample_channels = sample_channels;

			if (data)
				for (int i=0; i<sample_channels; i++)
					this->sample_data[i] = data[i];
			else
				for (int i=0; i<sample_channels; i++)
					this->sample_data[i] = NULL;

			this->num_samples = num_samples;

			this->loop_begin = loop_begin;
			this->loop_end = loop_end;

			this->sustain_loop_begin = susloop_begin;
			this->sustain_loop_end = susloop_end;

			this->loop_style = loop_style;
			this->sustain_loop_style = sustain_loop_style;

			use_sustain_loop = (susloop_end != 0xFFFFFFFF);
		}

		virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **c = NULL, double effect_tick_length = 0, bool top_level = true, int *znote = NULL)
		{
			if (c == NULL)
				throw "need sample context";

			if (*c)
			{
				(*c)->created_with->occlude_note(p, c, this, r);
				delete *c;
			}

			*c = new sample_builtintype_context(this);

			sample_builtintype_context &context = *(sample_builtintype_context *)*c;

			if (use_sustain_loop) // reset sustain loop
				context.sustain_loop_state = SustainLoopState::Running;
			else
				context.sustain_loop_state = SustainLoopState::Off;

			context.last_looped_sample = 0;
			context.sustain_loop_exit_difference = 0;
			context.sustain_loop_exit_sample = 0x7FFFFFFF;
			context.samples_per_second = samples_per_second;
			context.default_volume = default_volume;
			context.num_samples = num_samples;

			if (p)
			{
				if (top_level)
				{
					p->volume_envelope = NULL;
					p->panning_envelope = NULL;
					p->pitch_envelope = NULL;
				}
				p->samples_this_note = 0;
			}
		}

		virtual void occlude_note(channel *p = NULL, sample_context **c = NULL, sample *new_sample = NULL, row *r = NULL)
		{ // do nothing
		}

		virtual void exit_sustain_loop(sample_context *c)
		{
			sample_builtintype_context &context = *(sample_builtintype_context *)c;
			if (context.sustain_loop_state == SustainLoopState::Running)
				context.sustain_loop_state = SustainLoopState::Finishing;
		}

		virtual void kill_note(sample_context *c = NULL)
		{
			// no implementation required
		}

		virtual bool past_end(int sample, double offset, sample_context *c = NULL)
		{
			if (c == NULL)
				throw "need context for instrument";

			if (loop_end != 0xFFFFFFFF)
				return false;

			sample_builtintype_context &context = *(sample_builtintype_context *)c;
			switch (context.sustain_loop_state)
			{
				case SustainLoopState::Running:
				case SustainLoopState::Finishing: return false;
			}

			if (context.sustain_loop_state == SustainLoopState::Finished)
				sample -= context.sustain_loop_exit_difference;

			return (sample >= num_samples);
		}

		virtual one_sample get_sample(int sample, double offset, sample_context *c = NULL)
		{
			if (c == NULL)
				throw "need a sample context";

			sample_builtintype_context &context = *(sample_builtintype_context *)c;

			if ((sample < 0)
				|| ((sample >= num_samples)
				&& (loop_end == 0xFFFFFFFF)
				&& (!use_sustain_loop)))
				return one_sample(output_channels);

			int subsequent_sample = sample + 1;

			switch (context.sustain_loop_state)
			{
				case SustainLoopState::Running:
				case SustainLoopState::Finishing:
					int new_sample;
					int new_subsequent_sample;
					int next_clean_exit_sample;

					if ((context.sustain_loop_state == SustainLoopState::Finishing)
						&& (sample >= context.sustain_loop_exit_sample))
					{
						new_sample = sample - context.sustain_loop_exit_difference;
						new_subsequent_sample = new_sample + 1;
						context.sustain_loop_state = SustainLoopState::Finished;
					}
					else if (sample > sustain_loop_end)
					{
						int sustain_loop_length = sustain_loop_end - sustain_loop_begin + 1;
						double overrun = (sample + offset) - sustain_loop_end;
						int direction = -1;

						next_clean_exit_sample = sustain_loop_end + sustain_loop_length;

						int full_loop_length;

						switch (sustain_loop_style)
						{
							case LoopStyle::Forward: full_loop_length = sustain_loop_length; break;
							case LoopStyle::PingPong: full_loop_length = sustain_loop_length * 2; break;
						}

						int full_loops = overrun / full_loop_length;

						overrun -= full_loops * full_loop_length;

						while (overrun > sustain_loop_length)
						{
							next_clean_exit_sample += sustain_loop_length;
							overrun -= sustain_loop_length;
							direction = -direction;
						}

						if (sustain_loop_style == LoopStyle::Forward)
							direction = 1;

						if (direction < 0)
						{
							next_clean_exit_sample += sustain_loop_length;
							new_sample = sustain_loop_end - overrun;
						}
						else
							new_sample = sustain_loop_begin + overrun;

						new_subsequent_sample = new_sample + direction;

						if (new_subsequent_sample < sustain_loop_begin)
							new_subsequent_sample = sustain_loop_begin + 1;
						if (new_subsequent_sample > sustain_loop_end)
							new_subsequent_sample = sustain_loop_begin + (sustain_loop_length - direction) % sustain_loop_length;

						if (context.sustain_loop_state < SustainLoopState::Finishing)
						{
							context.sustain_loop_exit_sample = next_clean_exit_sample;
							context.sustain_loop_exit_difference = next_clean_exit_sample - sustain_loop_end;
						}
					}
					else
					{
						new_sample = sample;
						new_subsequent_sample = subsequent_sample;
					}

					sample = new_sample;
					subsequent_sample = new_subsequent_sample;

					break;
				case SustainLoopState::Finished:
					sample -= context.sustain_loop_exit_difference; // intentionally fall through
				case SustainLoopState::Off:
					if ((sample > loop_end) && (loop_end != 0xFFFFFFFF))
					{
						int loop_length = loop_end - loop_begin + 1;
						double overrun = (sample + offset) - loop_end;
						int direction = -1;

						int full_loop_length;

						switch (sustain_loop_style)
						{
							case LoopStyle::Forward: full_loop_length = loop_length; break;
							case LoopStyle::PingPong: full_loop_length = loop_length * 2; break;
						}

						int full_loops = overrun / full_loop_length;

						overrun -= full_loops * full_loop_length;

						while (overrun > loop_length)
						{
							overrun -= loop_length;
							direction = -direction;
						}

						if (loop_style == LoopStyle::Forward)
							direction = 1;

						if (direction < 0)
							new_sample = loop_end - overrun;
						else
							new_sample = loop_begin + overrun;

						new_subsequent_sample = new_sample + direction;

						if (new_subsequent_sample < loop_begin)
							new_subsequent_sample = loop_begin + 1;
						if (new_subsequent_sample > loop_end)
							new_subsequent_sample = loop_begin + (loop_length - direction) % loop_length;

						sample = new_sample;
						subsequent_sample = new_subsequent_sample;
					}

					break;
			}

			double before, after;

			one_sample ret(sample_channels);

			ret.reset();

			for (int i=0; i<sample_channels; i++)
			{
				before = sample_data[i][sample];
				if (subsequent_sample < num_samples)
					after = sample_data[i][subsequent_sample];
				else
					after = 0.0;

				ret.next_sample() = bilinear(before, after, offset);
			}

			return ret.scale(sample_scale).set_channels(output_channels);
		}
	};
}

#endif // SAMPLE_BUILTINTYPE_H
