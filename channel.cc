#include <cmath>

using namespace std;

#include "channel.h"

#include "MultiPLAY.h"

#include "Profile.h"

#include "one_sample.h"
#include "math.h"
#include "progress.h"

namespace MultiPLAY
{
	const Waveform::Type default_waveform = Waveform::Triangle;

	const double dropoff_min_length = 30.0 / 44100.0;
	const double dropoff_max_length = 70.0 / 44100.0;
	const double dropoff_proportion = 1.0 / (dropoff_min_length + dropoff_max_length);

	double inter_note = p2(1.0 / 12.0);

	// struct channel
	void channel::recalc(
		int znote,
		double duration_scale,
		bool calculate_length/* = true*/,
		bool reset_sample_offset/* = true*/,
		bool zero_means_no_note/* = true*/)
	{
		double seconds;

		if (calculate_length)
		{
			if (this_note_length_denominator != 0)
				seconds = (4.0 / this_note_length_denominator) * 60.0 / tempo;
		}

		current_znote = znote;

		if ((znote != 0) || (!zero_means_no_note))
		{
			if ((current_waveform == Waveform::Sample) && (current_sample != NULL))
				note_frequency = current_sample_context->samples_per_second * 2;
			else
				note_frequency = 987.766603;
			
			note_frequency *= pow(inter_note, znote - 49);

			if (reset_sample_offset)
			{
				offset = 0;
				offset_major = 0;
			}

			delta_offset_per_tick = note_frequency / ticks_per_second;
		}

		if (calculate_length)
		{
			if (this_note_length_denominator == 0)
				seconds = current_sample->num_samples / current_sample->samples_per_second;

			ticks_left = (int)(seconds * ticks_per_second * duration_scale);
			if (play_full_sample)
			{
				double note_seconds = seconds;
				if (this_note_length_denominator != 0)
					seconds = (4.0 / this_note_length_denominator) * 60.0 / tempo;

				cutoff_ticks = (int)((seconds - note_seconds) * ticks_per_second * duration_scale);
				rest_ticks = cutoff_ticks + (int)((ticks_left - cutoff_ticks) * rest_ticks_proportion);
			}
			else if ((znote != 0) || (!zero_means_no_note))
				rest_ticks = (int)(ticks_left * rest_ticks_proportion);
			else
				rest_ticks = ticks_left;

			ticks_total = ticks_left;

			double dropoff_time = seconds * dropoff_proportion;
			if (dropoff_time < dropoff_min_length)
				dropoff_time = dropoff_min_length;
			else if (dropoff_time > dropoff_max_length)
				dropoff_time = dropoff_max_length;

			int dropoff_ticks = (int)(dropoff_time * ticks_per_second * duration_scale);

			dropoff_start = rest_ticks + dropoff_ticks;
		}
	}

	/*virtual*/ void channel::note_off(bool calc_fade_per_tick/* = true*/, bool all_notes_off/* = true*/)
	{
		base_note_off(calc_fade_per_tick, all_notes_off);
	}

	void channel::base_note_off(bool calc_fade_per_tick/* = true*/, bool all_notes_off/* = true*/)
	{
		if (current_sample != NULL)
			current_sample->exit_sustain_loop(current_sample_context);

		if (all_notes_off)
		{
			if (volume_envelope != NULL)
				volume_envelope->note_off(offset_major, offset);
			if (panning_envelope != NULL)
				panning_envelope->note_off(offset_major, offset);
			if (pitch_envelope != NULL)
				pitch_envelope->note_off(offset_major, offset);
		}

		if ((volume_envelope == NULL) || volume_envelope->looping)
		{
			fading = true;
			fade_value = 1.0;
		}

		if (calc_fade_per_tick)
		{
			double fade_duration = dropoff_proportion * ticks_left;

			if (fade_duration < dropoff_min_length)
				fade_duration = dropoff_min_length;
			if (fade_duration > dropoff_max_length)
				fade_duration = dropoff_max_length;

			double fade_ticks = fade_duration * ticks_per_second;

			fade_per_tick = 1.0 / fade_ticks;
		}
	}

	bool channel::is_on_final_zero_volume_from_volume_envelope()
	{
		if (volume_envelope == NULL)
			return false;

		return
			volume_envelope->past_end(samples_this_note) &&
			(volume_envelope->env.node.size() > 0) &&
			(volume_envelope->env.node.back().value < 0.0000001);
	}

	/*virtual*/ void channel::get_playback_position(PlaybackPosition &position)
	{
		position.Order = 0;
		position.OrderCount = 0;
		position.Pattern = 0;
		position.PatternCount = 0;
		position.Row = 0;
		position.RowCount = 0;
		position.Offset = offset_major;
		position.OffsetCount = ticks_total;
		position.FormatString = "{Offset}/{OffsetCount}";
	}

	one_sample &channel::calculate_next_tick()
	{
		Profile profile;

		profile.push_back("start");

		return_sample.clear(output_channels);

		if (finished)
			return return_sample;

		profile.push_back("call advance_pattern");

		if (advance_pattern(return_sample, profile) == ChannelPlaybackState::Finished)
		{
			return_sample = panning * (channel_volume * return_sample);
			return return_sample;
		}

		profile.push_back("check for remaining ticks");

		if (ticks_left)
		{
			ticks_left--;
			if (ticks_left < rest_ticks)
			{
				return_sample.clear();
				return return_sample;
			}

			profile.push_back("generate/calculate sample for current waveform");

			switch (current_waveform)
			{
				case Waveform::Sine:
					return_sample = panning * (channel_volume * sin(6.283185 * offset));
					break;
				case Waveform::Square:
					return_sample = panning * (channel_volume * ((offset > 0.5) * 2 - 1));
					break;
				case Waveform::Sawtooth:
					if (offset < 0.5)
						return_sample = panning * (channel_volume * (2.0 * offset));
					else
						return_sample = panning * (channel_volume * (2.0 * (offset - 1.5)));
					break;
				case Waveform::RampDown:
					if (offset < 0.5)
						return_sample = panning * (channel_volume * (-2.0 * offset));
					else
						return_sample = panning * (channel_volume * ((1.0 - offset) * 2.0));
					break;
				case Waveform::Triangle:
					if (offset < 0.25)
						return_sample = panning * (channel_volume * (4.0 * offset));
					else if (offset < 0.75)
						return_sample = panning * (channel_volume * (4.0 * (0.5 - offset)));
					else
						return_sample = panning * (channel_volume * (4.0 * (offset - 1.0)));
					break;
				case Waveform::Sample:
					if (current_sample)
					{
						profile.push_back("interpolate sample from current_sample");
						return_sample = current_sample->get_sample(offset_major, offset, current_sample_context);
						profile.push_back("apply channel_volume");
						return_sample = channel_volume * return_sample;
						profile.push_back("apply panning");
						return_sample = panning * return_sample;
						profile.push_back("sample calculation complete");
					}
					else
						return_sample.clear(output_channels);
					break;
			}

			double sample_offset;
			if (volume_envelope || panning_envelope || pitch_envelope)
				sample_offset = samples_this_note;

			if (fading)
			{
				profile.push_back("perform fading calculations");

				if (fade_value <= 0)
				{
					fade_value = 0;
					fade_per_tick = 0;

					current_sample = NULL;

					if (current_sample_context != NULL)
					{
						delete current_sample_context;
						current_sample_context = NULL;
					}

					if (finish_with_fade)
						finished = true;
				}

				return_sample *= fade_value;

				fade_value -= fade_per_tick;
			}

			if (volume_envelope != NULL)
			{
				profile.push_back("perform volume_envelope calculations");

				return_sample *= volume_envelope->get_value_at(sample_offset);

				if (is_on_final_zero_volume_from_volume_envelope())
				{
					delete volume_envelope;

					volume_envelope = NULL;

					fading = true;
					fade_value = 1.0;

					if (!have_fade_per_tick)
					{
						base_note_off(true, false);
						have_fade_per_tick = true;
					}
				}
			}

			if (panning_envelope != NULL)
			{
				profile.push_back("perform panning_envelope calculations");
				return_sample *= pan_value(panning_envelope->get_value_at(sample_offset));
			}

			if (pitch_envelope != NULL)
			{
				profile.push_back("perform pitch_envelope calculations");

				double frequency = delta_offset_per_tick * ticks_per_second;
				double exponent = lg(frequency);

				exponent += pitch_envelope->get_value_at(sample_offset) * (16.0 / 12.0);
				if ((current_waveform == Waveform::Sample) && (current_sample->use_vibrato))
					exponent += current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

				frequency = p2(exponent);
				offset += (frequency / ticks_per_second);
			}
			else
			{
				if ((current_waveform == Waveform::Sample) && (current_sample && current_sample->use_vibrato))
				{
					profile.push_back("perform vibrato calculations");

					double frequency = delta_offset_per_tick * ticks_per_second;
					double exponent = lg(frequency);
					
					exponent += current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

					frequency = p2(exponent);
					offset += (frequency / ticks_per_second);
				}
				else
					offset += delta_offset_per_tick;
			}

			samples_this_note++;

			profile.push_back("move unitary part of offset into offset_major");

			if ((offset > 1.0) || (offset < 0.0))
			{
				if (offset > 2.0)
				{
					double offset_floor = floor(offset);

					offset_major += long(offset_floor);
					offset -= offset_floor;
				}
				else
				{
					offset_major++;
					offset -= 1.0;
				}
			}

			profile.push_back("apply dropoff");

			if (ticks_left < dropoff_start)
			{
				double volume = double(ticks_left - rest_ticks) / (dropoff_start - rest_ticks);
				return_sample *= volume;
			}

			if (ticks_left == cutoff_ticks)
				ticks_left = 0;

			profile.push_back("apply intensity");

			return_sample *= intensity;
		}

		profile.push_back("return");

		return return_sample;
	}

	channel::channel(bool looping)
		: return_sample(output_channels),
			panning(output_channels),
			looping(looping)
	{
		identity = "UNINITIALIZED";

		finished = false;
		ticks_left = 0;
		octave = 4;
		tempo = 120;
		note_length_denominator = 4;
		rest_ticks_proportion = 1.0 / 8.0;
		play_full_sample = false;
		intensity = 5000.0 / 32767.0;
		channel_volume = 1.0;
		current_waveform = default_waveform;
		current_sample = NULL;
		current_sample_context = NULL;
		volume_envelope = NULL;
		panning_envelope = NULL;
		pitch_envelope = NULL;
		fading = false;
		have_fade_per_tick = false;
	}

	channel::channel(pan_value &default_panning, bool looping)
		: return_sample(output_channels),
			panning(default_panning),
			looping(looping)
	{
		identity = "UNINITIALIZED";

		finished = false;
		ticks_left = 0;
		octave = 4;
		tempo = 120;
		note_length_denominator = 4;
		rest_ticks_proportion = 1.0 / 8.0;
		play_full_sample = false;
		intensity = 5000.0 / 32767.0;
		channel_volume = 1.0;
		current_waveform = default_waveform;
		current_sample = NULL;
		current_sample_context = NULL;
		volume_envelope = NULL;
		panning_envelope = NULL;
		pitch_envelope = NULL;
		fading = false;
		finish_with_fade = false;
		have_fade_per_tick = false;
	}

	/*virtual*/ channel::~channel()
	{
		if (volume_envelope)
			delete volume_envelope;
		if (panning_envelope)
			delete panning_envelope;
		if (pitch_envelope)
			delete pitch_envelope;
		if (current_sample_context)
			delete current_sample_context;
	}
}
