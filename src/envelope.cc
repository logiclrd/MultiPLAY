#include <cmath>

using namespace std;

#include "envelope.h"

#include "math.h"

namespace MultiPLAY
{
	// struct instrument_envelope_node
	instrument_envelope_node::instrument_envelope_node(int t, double v)
		: tick(t), value(v)
	{
	}

	// struct instrument_envelope
	instrument_envelope::instrument_envelope()
	{
		enabled = false;
	}

	void instrument_envelope::detect_false_looping()
	{
		if (looping)
		{
			int idx = 0;
			int node_count = int(node.size());

			while ((idx + 1 < node_count) && (node[idx + 1].tick <= loop_begin_tick))
				idx++;

			while ((idx < node_count) && (node[idx].tick <= loop_end_tick))
			{
				if (node[idx].value != 0)
					return;

				idx++;
			}

			// All nodes involved in the loop have value 0, so when we hit
			// that point, we're effectively done the note.
			looping = false;
		}
	}

	void instrument_envelope::begin_sustain_loop(sustain_loop_position &susloop)
	{
		loop_length = loop_end_tick - loop_begin_tick; // have to calc this somewhere
		sustain_loop_length = sustain_loop_end_tick - sustain_loop_begin_tick;

		susloop.still_running = (sustain_loop == true); // for clarity
		susloop.exit_diff = 0.0;
	}

	void instrument_envelope::exit_sustain_loop(double tick, sustain_loop_position &susloop)
	{
		if (susloop.still_running)
		{
			if (tick > sustain_loop_end_tick)
			{
				double new_tick;

				if (sustain_loop_end_tick == sustain_loop_begin_tick)
					new_tick = sustain_loop_begin_tick;
				else
				{
					if (sustain_loop_length)
					{
						double repetitions = floor((tick - sustain_loop_begin_tick) / sustain_loop_length);
						new_tick = tick - repetitions * sustain_loop_length;
					}
					else
						new_tick = sustain_loop_begin_tick;
				}

				susloop.exit_diff = tick - new_tick;
			}
			else
				susloop.exit_diff = 0.0;

			susloop.still_running = false;
		}
	}

	double instrument_envelope::get_value_at(double tick, sustain_loop_position &susloop)
	{
		if (susloop.still_running)
		{
			if (tick > sustain_loop_end_tick)
			{
				if (sustain_loop_length)
				{
					double repetitions = floor((tick - sustain_loop_begin_tick) / sustain_loop_length);
					tick -= repetitions * sustain_loop_length;
				}
				else
					tick = sustain_loop_begin_tick;
			}
		}
		else
		{
			tick -= susloop.exit_diff;

			if (looping && (tick > loop_end_tick))
			{
				if (loop_length)
				{
					double repetitions = floor((tick - loop_begin_tick) / loop_length);
					tick -= repetitions * loop_length;
				}
				else
					tick = loop_begin_tick;
			}
		}

		auto l = node.size() - 1;
		auto idx = decltype(l)(1);

		while (tick > node[idx].tick)
		{
			idx++;

			if (idx >= l)
				break;
		}

		// Hold end value
		if (tick > node[idx].tick)
			tick = node[idx].tick;

		tick -= node[idx - 1].tick;

		double t = tick / (node[idx].tick - node[idx - 1].tick);

		return bilinear(node[idx - 1].value, node[idx].value, t);
	}

	bool instrument_envelope::past_end(double tick, sustain_loop_position &susloop)
	{
		if (susloop.still_running)
			return false;
		if (looping)
			return false;

		if (sustain_loop)
			tick -= susloop.exit_diff;

		return (tick > node.back().tick);
	}

	// struct playback_envelope
	playback_envelope::playback_envelope(const playback_envelope &other)
		: wrap(other.wrap ? new playback_envelope(*other.wrap) : nullptr),
			sample_ticks_per_envelope_tick(other.sample_ticks_per_envelope_tick),
			envelope_ticks_per_sample_tick(other.envelope_ticks_per_sample_tick),
			susloop(other.susloop), scale(other.scale), env(other.env),
			looping(other.looping)
	{
	}

	playback_envelope::playback_envelope(instrument_envelope &e, double ticks)
		: wrap(nullptr), env(e), sample_ticks_per_envelope_tick(ticks),
			envelope_ticks_per_sample_tick(1.0 / ticks),
			looping(e.looping || e.sustain_loop)
	{
	}

	playback_envelope::playback_envelope(playback_envelope *w, instrument_envelope &e, double ticks, bool scale_envelope)
		: wrap(w), env(e), sample_ticks_per_envelope_tick(ticks),
			envelope_ticks_per_sample_tick(1.0 / ticks), scale(scale_envelope),
			looping(e.looping || e.sustain_loop || w->looping)
	{
	}

	playback_envelope::~playback_envelope()
	{
		if (wrap)
			delete wrap;
	}

	void playback_envelope::begin_note(bool recurse/* = true*/)
	{
		env.begin_sustain_loop(susloop);
		if (wrap && recurse)
			wrap->begin_note();
	}

	void playback_envelope::note_off(double sample_offset)
	{
		double tick = sample_offset * envelope_ticks_per_sample_tick;
		env.exit_sustain_loop(tick, susloop);
		if (wrap)
			wrap->note_off(sample_offset);
	}

	void playback_envelope::note_off(long sample, double offset)
	{
		note_off(sample + offset);
	}

	double playback_envelope::get_value_at(double sample_offset)
	{
		double tick = sample_offset * envelope_ticks_per_sample_tick;

		if (!wrap)
			return env.get_value_at(tick, susloop);

		if (scale)
			return env.get_value_at(tick, susloop) * wrap->get_value_at(sample_offset);
		else
			return env.get_value_at(tick, susloop) + wrap->get_value_at(sample_offset);
	}

	double playback_envelope::get_value_at(long sample, double offset)
	{
		return get_value_at(sample + offset);
	}

	bool playback_envelope::past_end(double sample_offset)
	{
		double tick = sample_offset * envelope_ticks_per_sample_tick;

		if (env.past_end(tick, susloop))
			return true;

		if (wrap)
			return wrap->past_end(sample_offset);

		return false;
	}

	bool playback_envelope::past_end(long sample, double offset)
	{
		return past_end(sample + offset);
	}
}
