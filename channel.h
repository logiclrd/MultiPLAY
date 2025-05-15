#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include <vector>

#include "MultiPLAY.h"

#include "Profile.h"

#include "math.h"
#include "progress.h"
#include "sample.h"

namespace MultiPLAY
{
	namespace ChannelPlaybackState
	{
		enum Type
		{
			Ongoing,
			Finished,
		};
	}

	struct channel
	{
		std::string identity;

		bool finished, looping;
		double offset, delta_offset_per_tick;
		double note_frequency;
		long offset_major;
		int ticks_total, ticks_left, dropoff_start, rest_ticks, cutoff_ticks;
		int current_znote;
		bool play_full_sample;
		double intensity, channel_volume;
		Waveform::Type current_waveform;
		sample *current_sample;
		sample_context *current_sample_context;
		pan_value panning;
		one_sample return_sample;
		long samples_this_note;
		playback_envelope *volume_envelope, *panning_envelope, *pitch_envelope;

		std::vector<channel *> my_ancillary_channels;

		bool fading, have_fade_per_tick, finish_with_fade;
		double fade_per_tick, fade_value;

		int tempo;
		int octave;
		int note_length_denominator, this_note_length_denominator;
		double rest_ticks_proportion;

		channel(bool looping);
		channel(pan_value &default_panning, bool looping);
		virtual ~channel();

		void recalc(int znote, double duration_scale, bool calculate_length = true, bool reset_sample_offset = true, bool zero_means_no_note = true);

		virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile) = 0;

		virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true);
		void base_note_off(bool calc_fade_per_tick = true, bool all_notes_off = true);

		bool is_on_final_zero_volume_from_volume_envelope();

		virtual void get_playback_position(PlaybackPosition &position);

		one_sample &calculate_next_tick();
	};
}

#endif // CHANNEL_H
