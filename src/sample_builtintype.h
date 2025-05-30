#ifndef SAMPLE_BUILTINTYPE_H
#define SAMPLE_BUILTINTYPE_H

#include <cstddef>
#include <string>

using namespace std;

#include "one_sample.h"
#include "envelope.h"
#include "channel.h"
#include "pattern.h"
#include "sample.h"

namespace MultiPLAY
{
	#define LOOP_END_NO_LOOP 0xFFFFFFFF
	#define SOFT_START_SAMPLES 10

	struct sample_builtintype_context : sample_context
	{
		int last_looped_sample;

		unsigned int sustain_loop_exit_sample;
		unsigned int sustain_loop_exit_difference;

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

		unsigned loop_begin, loop_end;
		unsigned sustain_loop_begin, sustain_loop_end;

		LoopStyle::Type loop_style, sustain_loop_style;

		bool use_sustain_loop;

		unsigned int include_loop_end_sample;

		sample_builtintype(
			int index, int sample_channels, double default_volume,
			T **data = nullptr, unsigned int num_samples = 0,
			LoopStyle::Type loop_style = LoopStyle::Forward,
			LoopStyle::Type sustain_loop_style = LoopStyle::Forward,
			unsigned loop_begin = 0, unsigned loop_end = LOOP_END_NO_LOOP,
			unsigned susloop_begin = 0, unsigned susloop_end = LOOP_END_NO_LOOP)
			: sample(index)
		{
			include_loop_end_sample = 1;

			initialize_sample_scale();

			this->sample_channels = sample_channels;

			if (data)
				for (int i=0; i<sample_channels; i++)
					this->sample_data[i] = data[i];
			else
				for (int i=0; i<sample_channels; i++)
					this->sample_data[i] = nullptr;

			this->num_samples = num_samples;
			this->default_volume = default_volume;

			this->loop_begin = loop_begin;
			this->loop_end = loop_end;

			this->sustain_loop_begin = susloop_begin;
			this->sustain_loop_end = susloop_end;

			this->loop_style = loop_style;
			this->sustain_loop_style = sustain_loop_style;

			use_sustain_loop = (susloop_end != LOOP_END_NO_LOOP);
		}

		virtual void detect_false_looping()
		{
			if (loop_begin == loop_end)
				loop_end = LOOP_END_NO_LOOP;
		}

		void initialize_sample_scale()
		{
			switch (sizeof(T))
			{
				case 1: sample_scale = global_volume / 127.5;        break;
				case 2: sample_scale = global_volume / 32767.5;      break;
				case 4: sample_scale = global_volume / 2147483647.5; break;
				default: throw "could not deduce sample scale";
			}
		}

		virtual void set_global_volume(double new_global_volume)
		{
			sample::set_global_volume(new_global_volume);

			initialize_sample_scale();
		}

		virtual void begin_new_note(row *r = nullptr, channel *p = nullptr, sample_context **c = nullptr, double effect_tick_length = 0, bool update_channel = true, int */*znote*/ = nullptr)
		{
			if (c == nullptr)
				throw "need sample context";

			if (p && update_channel)
			{
				if (r)
					p->occlude_note(this, r->znote);

				p->current_sample = this;

				if (p->volume_envelope)
					delete p->volume_envelope;
				if (p->panning_envelope)
					delete p->panning_envelope;
				if (p->pitch_envelope)
					delete p->pitch_envelope;

				p->volume_envelope = nullptr;
				p->panning_envelope = nullptr;
				p->pitch_envelope = nullptr;

				p->clear_filter();

				p->samples_this_note = 0;
				p->envelope_offset = 0;
			}

			if (*c)
				delete *c;

			sample_builtintype_context *context_ptr = new sample_builtintype_context(this);

			*c = context_ptr;

			sample_builtintype_context &context = *context_ptr;

			if (use_sustain_loop) // reset sustain loop
				context.sustain_loop_state = SustainLoopState::Running;
			else
				context.sustain_loop_state = SustainLoopState::Off;

			context.znote = r->znote;
			context.last_looped_sample = 0;
			context.sustain_loop_exit_difference = 0;
			context.sustain_loop_exit_sample = LOOP_END_NO_LOOP;
			context.samples_per_second = samples_per_second;
			context.default_volume = default_volume;
			context.num_samples = num_samples;
			context.vibrato_sweep_ticks = (long)round(vibrato_sweep_frames * effect_tick_length);
		}

		virtual void exit_sustain_loop(sample_context *c)
		{
			sample_builtintype_context *context_ptr = dynamic_cast<sample_builtintype_context *>(c);

			if (context_ptr == nullptr)
				throw "INTERNAL ERROR: sample/context type mismatch";

			sample_builtintype_context &context = *context_ptr;

			if (context.sustain_loop_state == SustainLoopState::Running)
				context.sustain_loop_state = SustainLoopState::Finishing;
		}

		virtual void kill_note(sample_context */*c = nullptr*/)
		{
			// no implementation required
		}

		virtual bool past_end(unsigned int sample, double /*offset*/, sample_context *c = nullptr)
		{
			if (c == nullptr)
				throw "need context for instrument";

			if (loop_end != LOOP_END_NO_LOOP)
				return false;

			sample_builtintype_context *context_ptr = dynamic_cast<sample_builtintype_context *>(c);

			if (context_ptr == nullptr)
				throw "INTERNAL ERROR: sample/context type mismatch";

			sample_builtintype_context &context = *context_ptr;

			switch (context.sustain_loop_state)
			{
				case SustainLoopState::Running:
				case SustainLoopState::Finishing: return false;

				case SustainLoopState::Off:
				case SustainLoopState::Finished: break; // Do nothing.
			}

			if (context.sustain_loop_state == SustainLoopState::Finished)
				sample -= context.sustain_loop_exit_difference;

			return (sample >= num_samples);
		}

		virtual one_sample get_sample(unsigned int sample, double offset, sample_context *c = nullptr)
		{
			if (c == nullptr)
				throw "need a sample context";

			sample_builtintype_context *context_ptr = dynamic_cast<sample_builtintype_context *>(c);

			if (context_ptr == nullptr)
				throw "INTERNAL ERROR: sample/context type mismatch";

			sample_builtintype_context &context = *context_ptr;

			if ((unsigned(sample) >= num_samples)
			 && (loop_end == LOOP_END_NO_LOOP)
			 && (!use_sustain_loop))
				return one_sample(output_channels);

			unsigned subsequent_sample = sample + 1;

			switch (context.sustain_loop_state)
			{
				case SustainLoopState::Running:
				case SustainLoopState::Finishing:
					unsigned new_sample;
					unsigned new_subsequent_sample;
					unsigned next_clean_exit_sample;

					if ((context.sustain_loop_state == SustainLoopState::Finishing)
						&& (sample >= context.sustain_loop_exit_sample))
					{
						new_sample = unsigned(sample - context.sustain_loop_exit_difference);
						new_subsequent_sample = new_sample + 1;
						context.sustain_loop_state = SustainLoopState::Finished;
					}
					else if (sample > sustain_loop_end)
					{
						unsigned sustain_loop_length = sustain_loop_end - sustain_loop_begin + include_loop_end_sample;
						double overrun = (sample + offset) - sustain_loop_end;
						int direction = -1;

						next_clean_exit_sample = sustain_loop_end + sustain_loop_length;

						unsigned full_loop_length;

						if (sustain_loop_style == LoopStyle::PingPong)
							full_loop_length = sustain_loop_length * 2;
						else
							full_loop_length = sustain_loop_length;

						unsigned full_loops = (unsigned)floor(overrun / full_loop_length);

						overrun -= (double)(full_loops * full_loop_length);

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
							new_sample = (unsigned int)(sustain_loop_end - floor(overrun));
						}
						else
							new_sample = (unsigned int)(sustain_loop_begin + floor(overrun));

						new_subsequent_sample = unsigned(int(new_sample) + direction);

						switch (sustain_loop_style)
						{
							case LoopStyle::Undefined:
								throw "sustain_loop_style has not been initialized";

							case LoopStyle::Forward:
								if (new_subsequent_sample < sustain_loop_begin)
									new_subsequent_sample = sustain_loop_end;
								if (new_subsequent_sample > sustain_loop_end)
									new_subsequent_sample = sustain_loop_begin;
								break;
							case LoopStyle::PingPong:
								if (new_subsequent_sample < sustain_loop_begin)
									new_subsequent_sample = sustain_loop_begin + 1;
								if (new_subsequent_sample > sustain_loop_end)
									new_subsequent_sample = sustain_loop_begin + unsigned((int(sustain_loop_length) - direction) % int(sustain_loop_length));
								break;
						}

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
					if ((sample > loop_end) && (loop_end != LOOP_END_NO_LOOP))
					{
						unsigned loop_length = loop_end - loop_begin + include_loop_end_sample;
						double overrun = (sample + offset) - loop_end;
						int direction = -1;

						unsigned full_loop_length;

						if (loop_style == LoopStyle::PingPong)
							full_loop_length = loop_length * 2;
						else
							full_loop_length = loop_length;

						unsigned full_loops = (unsigned)floor(overrun / full_loop_length);

						overrun -= full_loops * full_loop_length;

						while (overrun > loop_length)
						{
							overrun -= loop_length;
							direction = -direction;
						}

						if (loop_style == LoopStyle::Forward)
							direction = 1;

						if (direction < 0)
							new_sample = (unsigned int)(loop_end - floor(overrun));
						else
							new_sample = (unsigned int)(loop_begin + floor(overrun));

						new_subsequent_sample = unsigned(int(new_sample) + direction);

						switch (loop_style)
						{
							case LoopStyle::Undefined:
								throw "loop_style has not been initialized";

							case LoopStyle::Forward:
								if (new_subsequent_sample < loop_begin)
									new_subsequent_sample = loop_end;
								if (new_subsequent_sample > loop_end)
									new_subsequent_sample = loop_begin;
								break;
							case LoopStyle::PingPong:
								if (new_subsequent_sample < loop_begin)
									new_subsequent_sample = loop_begin + 1;
								if (new_subsequent_sample > loop_end)
									new_subsequent_sample = loop_begin + unsigned(int(loop_length) - direction) % loop_length;
								break;
						}

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

			if (sample < SOFT_START_SAMPLES)
			{
				double sample_offset = sample + offset;

				if (sample_offset < SOFT_START_SAMPLES)
					ret *= (sample_offset / SOFT_START_SAMPLES);
			}

			return ret.scale(sample_scale).set_channels(output_channels);
		}
	};
}

#endif // SAMPLE_BUILTINTYPE_H
