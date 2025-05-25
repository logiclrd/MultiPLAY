#ifndef CHANNEL_H
#define CHANNEL_H

#include <string>
#include <vector>

#include "MultiPLAY.h"

#include "Profile.h"

#include "progress.h"
#include "sample.h"
#include "notes.h"
#include "math.h"

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

	#define RESIDUE_FADE 0.93

	struct channel
	{
		std::string identity;

		bool enabled, finished;
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
		one_sample return_sample, last_return_sample, last_last_return_sample;
		one_sample residue, residue_inertia;
		pan_value default_panning, panning;
		bool looping;
		long samples_this_note;
		long envelope_offset;
		bool enable_volume_envelope, enable_panning_envelope, enable_pitch_envelope;
		playback_envelope *volume_envelope, *panning_envelope, *pitch_envelope;
		NewNoteAction::Type new_note_action;
		DuplicateCheck::Type duplicate_note_check;
		DuplicateCheckAction::Type duplicate_note_action;

		bool enable_filter;
		double filter_cutoff, filter_resonance;
		one_sample filter_y[2];
		double filter_a0, filter_b0, filter_b1;

		std::vector<channel *> my_ancillary_channels;

		bool fading, fade_complete, have_fade_per_tick, finish_with_fade;
		int ticks_per_fade_out_frame;
		double fade_per_tick, fade_value;

		int tempo;
		int octave;
		int note_length_denominator, this_note_length_denominator;
		double rest_ticks_proportion;

		channel(bool looping, bool enabled);
		channel(pan_value &default_panning, bool looping, bool enabled);
		void init_fields();
		virtual ~channel();

		void clear_filter();
		void set_filter(double cutoff, double resonance);

		void recalc(int znote, double duration_scale, bool calculate_length = true, bool reset_sample_offset = true, bool zero_means_no_note = true);

		virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile) = 0;

		void add_ancillary_channel(channel *channel);
		void remove_ancillary_channel(channel *channel);

		void note_cut(bool capture_residue = true);
		virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true);
		void base_note_off(bool calc_fade_per_tick = true, bool exit_sustain_loop = true, bool exit_envelope_loops = true);
		virtual void note_fade();

		void occlude_note(sample *new_sample = nullptr, int znote = -1);

		bool is_at_end_of_volume_envelope();
		bool is_on_final_zero_volume_from_volume_envelope();

		virtual void get_playback_position(PlaybackPosition &position);

		one_sample &calculate_next_tick();
		one_sample &calculate_next_tick_core();
	};
}

#endif // CHANNEL_H
