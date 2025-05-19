#ifndef ENVELOPE_H
#define ENVELOPE_H

#include <vector>

namespace MultiPLAY
{
	namespace SustainLoopState
	{
		enum Type
		{
			Off, Running, Finishing, Finished,
		};
	}

	struct instrument_envelope_node
	{
		int tick;
		double value;

		instrument_envelope_node(int t, double v);
	};

	struct sustain_loop_position
	{
		bool still_running;
		double exit_diff;
	};

	struct instrument_envelope
	{
		bool enabled, looping, sustain_loop;

		int loop_begin_tick, loop_end_tick, sustain_loop_begin_tick, sustain_loop_end_tick;
		double loop_length, sustain_loop_length; // to reduce conversions

		std::vector<instrument_envelope_node> node;

		instrument_envelope();

		void begin_sustain_loop(sustain_loop_position &susloop);
		void exit_sustain_loop(double tick, sustain_loop_position &susloop);
		double get_value_at(double tick, sustain_loop_position &susloop);
		bool past_end(double tick, sustain_loop_position &susloop);
	};

	struct playback_envelope
	{
		instrument_envelope &env;
		double sample_ticks_per_envelope_tick, envelope_ticks_per_sample_tick;
		sustain_loop_position susloop;
		playback_envelope *wrap;
		bool scale, looping;

		playback_envelope(const playback_envelope &other);
		playback_envelope(instrument_envelope &e, double ticks);
		playback_envelope(playback_envelope *w, instrument_envelope &e, double ticks, bool scale_envelope);
		~playback_envelope();

		void begin_note(bool recurse = true);
		void note_off(double sample_offset);
		void note_off(long sample, double offset);
		double get_value_at(double sample_offset);
		double get_value_at(long sample, double offset);
		bool past_end(double sample_offset);
		bool past_end(long sample, double offset);
	};
}

#endif // ENVELOPE_H