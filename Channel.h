struct channel
{
	bool finished, looping;
	double offset, delta_offset_per_tick;
	double note_frequency;
	long offset_major;
	int ticks_left, dropoff_start, rest_ticks, cutoff_ticks;
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

	vector<channel *> my_ancillary_channels;

	bool fading, have_fade_per_tick;
	double fade_per_tick, fade_value;

	int tempo;
	int octave;
	int note_length_denominator, this_note_length_denominator;
	double rest_ticks_proportion;

	void recalc(int znote, double duration_scale, bool calculate_length = true, bool reset_sample_offset = true, bool zero_means_no_note = true)
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

			double dropoff_time = seconds * dropoff_proportion;
			if (dropoff_time < dropoff_min_length)
				dropoff_time = dropoff_min_length;
			else if (dropoff_time > dropoff_max_length)
				dropoff_time = dropoff_max_length;

			int dropoff_ticks = (int)(dropoff_time * ticks_per_second * duration_scale);

			dropoff_start = rest_ticks + dropoff_ticks;
		}
	}

	virtual bool advance_pattern(one_sample &sample)
	{
		throw "no implementation for advance_pattern";
	}

	virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true)
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
			double fade_ticks = dropoff_proportion * ticks_left;
			if (fade_ticks < dropoff_min_length)
				fade_ticks = dropoff_min_length;
			if (fade_ticks > dropoff_max_length)
				fade_ticks = dropoff_max_length;
			fade_per_tick = 1.0 / fade_ticks;
		}
	}

	one_sample &calculate_next_tick()
	{
		return_sample.clear(output_channels);

		if (finished)
			return return_sample;

		if (advance_pattern(return_sample))
		{
			return_sample = panning * (channel_volume * return_sample);
			return return_sample;
		}

		if (ticks_left)
		{
			ticks_left--;
			if (ticks_left < rest_ticks)
			{
				return_sample.clear();
				return return_sample;
			}

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
						return_sample = panning * (channel_volume * current_sample->get_sample(offset_major, offset, current_sample_context));
					else
						return_sample.clear(output_channels);
			}

			double sample_offset;
			if (volume_envelope || panning_envelope || pitch_envelope)
				sample_offset = samples_this_note;

			if (fading)
			{
				if (fade_value > 0.0)
					return_sample *= fade_value;
				else
					return_sample.clear();
				fade_value -= fade_per_tick;
			}
			else if (volume_envelope != NULL)
			{
				if (volume_envelope->past_end(sample_offset))
				{
					fading = true;
					fade_value = 1.0;

					if (!have_fade_per_tick)
					{
						note_off(true, false);
						have_fade_per_tick = true;
					}
				}
				return_sample *= volume_envelope->get_value_at(sample_offset);
			}

			if (panning_envelope != NULL)
				return_sample *= pan_value(panning_envelope->get_value_at(sample_offset));

			if (pitch_envelope != NULL)
			{
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

			if (ticks_left < dropoff_start)
			{
				double volume = double(ticks_left - rest_ticks) / (dropoff_start - rest_ticks);
				return_sample *= volume;
			}

			if (ticks_left == cutoff_ticks)
				ticks_left = 0;

			return_sample *= intensity;
		}

		return return_sample;
	}

	channel(bool looping)
		: return_sample(output_channels),
			panning(output_channels),
			looping(looping)
	{
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

	channel(pan_value &default_panning, bool looping)
		: return_sample(output_channels),
			panning(default_panning),
			looping(looping)
	{
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

	virtual ~channel()
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
};
