#include <cmath>

using namespace std;

#include <signal.h>

#include "channel.h"

#include "MultiPLAY.h"

#include "Channel_DYNAMIC.h"

#include "Profile.h"

#include "one_sample.h"
#include "progress.h"
#include "lint.h"
#include "math.h"

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
		double seconds = 0.0;

		if (calculate_length)
		{
			if (this_note_length_denominator != 0)
				seconds = (4.0 / this_note_length_denominator) * 60.0 / tempo;
		}

		current_znote = znote;

		if ((znote != 0) || (!zero_means_no_note))
		{
			if ((current_waveform == Waveform::Sample) && (current_sample != nullptr))
				note_frequency = current_sample_context->samples_per_second * 2;
			else
				note_frequency = 987.766603;

			note_frequency *= pow(inter_note, znote - 49);

			LINT_DOUBLE(note_frequency);

			if (reset_sample_offset)
			{
				offset = 0;
				offset_major = 0;
			}

			delta_offset_per_tick = note_frequency / ticks_per_second;
			LINT_DOUBLE(delta_offset_per_tick);
		}

		if (calculate_length)
		{
			if (this_note_length_denominator == 0)
				seconds = current_sample_context->num_samples / current_sample_context->samples_per_second;

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

	void channel::add_ancillary_channel(channel *ancillary_channel)
	{
		my_ancillary_channels.push_back(ancillary_channel);

		ancillary_channel->ticks_per_fade_out_frame = this->ticks_per_fade_out_frame;
	}

	void channel::remove_ancillary_channel(channel *ancillary_channel)
	{
		for (auto iter = my_ancillary_channels.begin(), l = my_ancillary_channels.end(); iter != l; iter++)
			if (*iter == ancillary_channel)
			{
				my_ancillary_channels.erase(iter);
				break;
			}
	}

	void channel::note_cut(bool capture_residue/* = true*/)
	{
		if (capture_residue)
		{
			residue += last_return_sample;
			residue_inertia += last_return_sample - last_last_return_sample;
		}

		current_sample = nullptr;

		if (current_sample_context != nullptr)
		{
			delete current_sample_context;
			current_sample_context = nullptr;
		}

		if (volume_envelope != nullptr)
		{
			delete volume_envelope;
			volume_envelope = nullptr;
		}

		if (panning_envelope != nullptr)
		{
			delete panning_envelope;
			panning_envelope = nullptr;
		}

		if (pitch_envelope != nullptr)
		{
			delete pitch_envelope;
			pitch_envelope = nullptr;
		}
	}

	/*virtual*/ void channel::note_off(bool calc_fade_per_tick/* = true*/, bool exit_envelope_loops/* = true*/)
	{
		base_note_off(calc_fade_per_tick, true, exit_envelope_loops);
	}

	void channel::base_note_off(bool calc_fade_per_tick/* = true*/, bool exit_sustain_loop/* = true*/, bool exit_envelope_loops/* = true*/)
	{
		if (exit_sustain_loop)
		{
			if (current_sample != nullptr)
				current_sample->exit_sustain_loop(current_sample_context);
		}

		if (exit_envelope_loops)
		{
			if (volume_envelope != nullptr)
				volume_envelope->note_off(envelope_offset);
			if (panning_envelope != nullptr)
				panning_envelope->note_off(envelope_offset);
			if (pitch_envelope != nullptr)
				pitch_envelope->note_off(envelope_offset);
		}

		if ((volume_envelope == nullptr) || volume_envelope->looping || (current_sample->fade_out > 0))
		{
			if (!fading)
			{
				fading = true;
				fade_value = 1.0;
			}

			if (fade_per_tick == 0)
				have_fade_per_tick = false;
		}

		if (calc_fade_per_tick)
		{
			double fade_duration_ticks;

			if ((current_sample != nullptr) && (current_sample->fade_out > 0) && (ticks_per_fade_out_frame > 0))
				fade_duration_ticks = ticks_per_fade_out_frame / current_sample->fade_out;
			else
			{
				double fade_duration = dropoff_proportion * ticks_left;

				if (fade_duration < dropoff_min_length)
					fade_duration = dropoff_min_length;
				if (fade_duration > dropoff_max_length)
					fade_duration = dropoff_max_length;

				fade_duration_ticks = fade_duration * ticks_per_second;
			}

			fade_per_tick = 1.0 / fade_duration_ticks;

			have_fade_per_tick = true;
		}
	}

	/*virtual*/ void channel::note_fade()
	{
		base_note_off(true, false, false);
	}

	void channel::occlude_note(
		sample *new_sample/* = nullptr*/,
		int new_znote/* = -1*/)
	{
		if (current_sample_context != nullptr)
		{
			NewNoteAction::Type effective_nna = new_note_action;

			bool is_duplicate = false;

			switch (duplicate_note_check)
			{
				case DuplicateCheck::Off:
					// Do nothing
					break;
				case DuplicateCheck::Note:
					is_duplicate = (current_sample_context->znote == new_znote);
					break;
				case DuplicateCheck::Sample:
					is_duplicate = current_sample_context->is_sample_match(new_sample->get_root_sample(inote_from_znote(new_znote)));
					break;
				case DuplicateCheck::Instrument:
					is_duplicate = (current_sample == new_sample);
					break;
			}

			if (is_duplicate)
			{
				switch (duplicate_note_action)
				{
					case DuplicateCheckAction::Cut:
						effective_nna = NewNoteAction::Cut;
						break;
					case DuplicateCheckAction::NoteFade:
						effective_nna = NewNoteAction::NoteFade;
						break;
					case DuplicateCheckAction::NoteOff:
						effective_nna = NewNoteAction::NoteOff;
						break;
				}
			}

			if (effective_nna == NewNoteAction::Cut)
				note_cut();
			else
			{
				switch (effective_nna)
				{
					case NewNoteAction::ContinueNote:
						// Do nothing.
						break;
					case NewNoteAction::NoteOff:
						note_off();
						break;
					case NewNoteAction::NoteFade:
						note_fade();
						break;
				}

				channel_DYNAMIC *ancillary = channel_DYNAMIC::assume_note(this);

				ancillary_channels.push_back(ancillary);
				add_ancillary_channel(ancillary);
			}
		}
	}

	bool channel::is_at_end_of_volume_envelope()
	{
		if (volume_envelope == nullptr)
			return false;

		return volume_envelope->past_end(envelope_offset);
	}

	bool channel::is_on_final_zero_volume_from_volume_envelope()
	{
		return
			is_at_end_of_volume_envelope() &&
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

	one_sample &channel::calculate_next_tick_core()
	{
		Profile profile;

		profile.push_back("start");

		return_sample.clear(output_channels);

		bool finish_exit = finished;

		if (!finish_exit)
		{
			profile.push_back("call advance_pattern");

			finish_exit = (finished || (advance_pattern(return_sample, profile) == ChannelPlaybackState::Finished));
		}

		if (finish_exit)
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

			if (enabled)
			{
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

				if (fading)
				{
					profile.push_back("perform fading calculations");

					if (fade_value <= 0)
					{
						fading = false;
						fade_complete = true;
						fade_value = 0;
						fade_per_tick = 0;
						have_fade_per_tick = false;

						note_cut();

						if (finish_with_fade)
							finished = true;
					}

					return_sample *= fade_value;

					fade_value -= fade_per_tick;
				}

				if (enable_volume_envelope && (volume_envelope != nullptr))
				{
					profile.push_back("perform volume_envelope calculations");

					return_sample *= volume_envelope->get_value_at(envelope_offset);

					if (!fading)
					{
						if (is_at_end_of_volume_envelope())
						{
							if (is_on_final_zero_volume_from_volume_envelope())
								note_cut();
							else
								note_off();
						}
					}
				}

				if (enable_panning_envelope && (panning_envelope != nullptr))
				{
					profile.push_back("perform panning_envelope calculations");
					return_sample *= pan_value(panning_envelope->get_value_at(envelope_offset));
				}

				if (enable_pitch_envelope && (pitch_envelope != nullptr))
				{
					profile.push_back("perform pitch_envelope calculations");

					double sweep = 1.0;
					double frequency = delta_offset_per_tick * ticks_per_second;
					double exponent = lg(frequency);

					if (samples_this_note < current_sample_context->vibrato_sweep_ticks)
						sweep = samples_this_note / (double)current_sample_context->vibrato_sweep_ticks;

					exponent += pitch_envelope->get_value_at(envelope_offset) * (16.0 / 12.0);
					if ((current_waveform == Waveform::Sample) && (current_sample->use_vibrato))
						exponent += sweep * current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

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

						if (LINT_DOUBLE_CHECK(offset))
						{
							LINT_DOUBLE(offset);

							// Repeat the calculation for use in a debug session.
							frequency = delta_offset_per_tick * ticks_per_second;
							exponent = lg(frequency);
							
							exponent += current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

							frequency = p2(exponent);
							offset += (frequency / ticks_per_second);
						}
					}
					else
						offset += delta_offset_per_tick;

					LINT_DOUBLE(offset);
				}

				samples_this_note++;
				envelope_offset++;

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
		}

		profile.push_back("return");

		return return_sample;
	}

	one_sample &channel::calculate_next_tick()
	{
		last_last_return_sample = last_return_sample;
		last_return_sample = return_sample;

		calculate_next_tick_core();

		if (residue.num_samples != return_sample.num_samples)
			residue.set_channels(return_sample.num_samples);
		if (residue_inertia.num_samples != return_sample.num_samples)
			residue_inertia.set_channels(return_sample.num_samples);

		return_sample += residue;

		residue += residue_inertia;

		residue *= RESIDUE_FADE;
		residue_inertia *= RESIDUE_FADE;

		return return_sample;
	}

	channel::channel(bool looping, bool enabled)
		: return_sample(output_channels),
			panning(output_channels),
			looping(looping),
			enabled(enabled)
	{
		identity = "UNINITIALIZED";

		init_fields();
	}

	channel::channel(pan_value &default_panning, bool looping, bool enabled)
		: return_sample(output_channels),
			default_panning(default_panning),
			panning(default_panning),
			looping(looping),
			enabled(enabled)
	{
		identity = "UNINITIALIZED";

		init_fields();
	}

	void channel::init_fields()
	{
		finished = false;
		note_frequency = 1;
		ticks_left = 0;
		octave = 4;
		tempo = 120;
		note_length_denominator = 4;
		rest_ticks_proportion = 1.0 / 8.0;
		play_full_sample = false;
		intensity = 5000.0 / 32767.0;
		channel_volume = 1.0;
		offset_major = 0;
		offset = 0;
		current_waveform = default_waveform;
		current_sample = nullptr;
		current_sample_context = nullptr;
		enable_volume_envelope = true;
		enable_panning_envelope = true;
		enable_pitch_envelope = true;
		volume_envelope = nullptr;
		panning_envelope = nullptr;
		pitch_envelope = nullptr;
		new_note_action = NewNoteAction::Cut;
		duplicate_note_check = DuplicateCheck::Off;
		duplicate_note_action = DuplicateCheckAction::Cut;
		fading = false;
		fade_complete = false;
		finish_with_fade = false;
		have_fade_per_tick = false;
		fade_per_tick = 0;
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
