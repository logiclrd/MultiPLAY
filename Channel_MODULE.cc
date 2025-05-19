#include "Channel_MODULE.h"

#include <algorithm>
#include <sstream>

using namespace std;

#include "Channel_DYNAMIC.h"
#include "MultiPLAY.h"
#include "Load_MOD.h"

#include "mod_finetune.h"
#include "formatting.h"
#include "notes.h"
#include "lint.h"

namespace MultiPLAY
{
	// struct channel_MODULE
	channel_MODULE::channel_MODULE(vector<channel_MODULE *> *channel_group, unsigned int channel_number, module_struct *module, int channel_volume, bool looping, bool enabled)
		: channel(module->initial_panning[channel_number], looping, enabled),
		  channel_group(channel_group),
		  unmapped_channel_number(channel_number),
		  channel_number(module->channel_map[channel_number]),
		  module(module)
	{
		stringstream ss;

		ss << "module_" << module->filename << "_" << channel_number;

		this->identity = ss.str();

		ticks_per_fade_out_frame = (int)round(module->ticks_per_frame);

		panning.set_channels(output_channels);
		offset = 0.0;
		delta_offset_per_tick = 0.0;
		play_full_sample = true;
		pattern_jump_target = -1;
		row_jump_target = 0;
		note_length_denominator = 0;
		volume_slide = false;
		portamento = false;
		vibrato = false;
		vibrato_depth = 0;
		vibrato_cycle_frequency = 0;
		vibrato_cycle_offset = 0;
		vibrato_start_t = 0;
		tremor = false;
		arpeggio = false;
		note_delay = false;
		retrigger = false;
		tremolo = false;
		note_cut_at_frame = false;
		panning_slide = false;
		panbrello = false;
		channel_volume_slide = false;
		tempo_slide = false;
		global_volume_slide = false;
		pattern_delay_by_frames = false;
		pattern_delay = 0;
		portamento_glissando = false;
		vibrato_retrig = true;
		tremolo_retrig = true;
		vibrato_waveform = Waveform::Sine;
		tremolo_waveform = Waveform::Sine;
		current_waveform = Waveform::Sample;
		current_sample = NULL;
		original_intensity = intensity;
		target_volume = -1;
		volume = channel_volume;
		it_effects = module->it_module;
		amiga_panning = module->amiga_panning;
		it_new_effects = module->it_module_new_effects;
		it_portamento_link = module->it_module_portamento_link;
		it_linear_slides = module->it_module_linear_slides;
		set_offset_high = 0;
	}

	/*virtual*/ channel_MODULE::~channel_MODULE()
	{
	}

	void channel_MODULE::ticks_per_frame_changed()
	{
		this->ticks_per_fade_out_frame = (int)round(module->ticks_per_frame);

		for (size_t i = 0; i < my_ancillary_channels.size(); i++)
			my_ancillary_channels[i]->ticks_per_fade_out_frame = this->ticks_per_fade_out_frame;
	}

	/*virtual*/ void channel_MODULE::note_off(bool /*calc_fade_per_tick = true*/, bool exit_envelope_loops/* = true*/)
	{
		this->channel::note_off(false, exit_envelope_loops);
	}

	/*virtual*/ void channel_MODULE::note_fade()
	{
		if (!fading)
		{
			fading = true;
			fade_value = 1.0;

			if (current_sample != NULL)
				fade_per_tick = (current_sample->fade_out / 1024.0) / module->ticks_per_frame;
		}
	}

	/*virtual*/ void channel_MODULE::get_playback_position(PlaybackPosition &position)
	{
		position.Order = int(module->current_pattern);
		position.OrderCount = int(module->pattern_list_length);
		position.Pattern = module->pattern_list[module->current_pattern]->index;
		position.PatternCount = int(module->patterns.size());
		position.Row = module->current_row;
		position.RowCount = int(module->pattern_list[module->current_pattern]->row_list.size());
		position.Offset = offset_major;
		position.OffsetCount = ticks_total;
		position.FormatString = "order {Order}/{OrderCount} - {Pattern}:{Row:2}";
	}

	/*virtual*/ ChannelPlaybackState::Type channel_MODULE::advance_pattern(one_sample &sample, Profile &profile)
	{
		if (volume_slide)
		{
			profile.push_back("start volume_slide");

			double t = double(ticks_left) / module->ticks_per_module_row;
			double new_intensity = previous_intensity * t + target_intensity * (1.0 - t);
			if (new_intensity < 0)
				new_intensity = 0;
			else if (new_intensity > original_intensity)
				new_intensity = original_intensity;

			if (tremolo)
				tremolo_middle_intensity *= (new_intensity / intensity);

			intensity = new_intensity;

			profile.push_back("end volume_slide");
		}

		if (panning_slide)
		{
			profile.push_back("start panning_side");

			double t = double(ticks_left) / module->ticks_per_module_row;
			double new_panning = panning_slide_start * t + panning_slide_end * (1.0 - t);
			if (new_panning < -1.0)
				new_panning = -1.0;
			else if (new_panning > 1.0)
				new_panning = 1.0;

			if (panbrello)
				panbrello_middle = new_panning;

			panning.from_linear_pan(new_panning, -1.0, +1.0);

			profile.push_back("end panning_side");
		}

		if (channel_volume_slide)
		{
			profile.push_back("start channel_volume_slide");

			double t = double(ticks_left) / module->ticks_per_module_row;
			double new_channel_volume = channel_volume_slide_start * t + channel_volume_slide_end * (1.0 - t);
			if (new_channel_volume < 0.0)
				new_channel_volume = 0.0;
			else if (new_channel_volume > 1.0)
				new_channel_volume = 1.0;

			channel_volume = new_channel_volume;

			profile.push_back("end channel_volume_slide");
		}

		if (global_volume_slide)
		{
			profile.push_back("start global_volume_slide");

			double t = double(ticks_left) / module->ticks_per_module_row;
			double new_global_volume = global_volume_slide_start * t + global_volume_slide_end * (1.0 - t);
			if (new_global_volume < 0.0)
				new_global_volume = 0.0;
			else if (new_global_volume > 1.0)
				new_global_volume = 1.0;

			global_volume = new_global_volume;

			profile.push_back("end global_volume_slide");
		}

		if (portamento)
		{
			profile.push_back("start portamento");

			double t = double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;

			if (!smooth_portamento_effect)
				t = floor(t * module->speed) / module->speed;

			if (t > portamento_end_t)
				t = portamento_end_t;

			if (it_linear_slides)
			{
				double exponent = portamento_start * (1.0 - t) + portamento_end * t;
				note_frequency = p2(exponent);
				LINT_DOUBLE(note_frequency);
			}
			else
			{
				double period = portamento_start * (1.0 - t) + portamento_end * t;
				note_frequency = 14317056.0 / period;
				LINT_DOUBLE(note_frequency);
			}

			if (portamento_glissando)
			{
				double exponent = lg(note_frequency * 1.16363636363636363);
				long note = (long)exponent * 12;
				exponent = note / 12.0;
				note_frequency = p2(exponent) / 1.16363636363636363;
				LINT_DOUBLE(note_frequency);
			}

			delta_offset_per_tick = note_frequency / ticks_per_second;
			LINT_DOUBLE(delta_offset_per_tick);

			profile.push_back("end portamento");
		}

		if (vibrato)
		{
			profile.push_back("start vibrato");

			double t = double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;

			if (t >= vibrato_start_t)
			{
				t -= vibrato_start_t;
				double t_vibrato = (vibrato_cycle_offset + t) * vibrato_cycle_frequency;
				double t_offset = t_vibrato - floor(t_vibrato);
				double value;
				
				if (it_linear_slides)
					value = lg(note_frequency);
				else
					value = 14317056.0 / note_frequency;
				
				switch (vibrato_waveform)
				{
					case Waveform::Triangle:
					case Waveform::Sawtooth:
					case Waveform::Sample:
						// Undefined.
						break;
					case Waveform::Sine:
						value += vibrato_depth * sin(t_vibrato * 6.283185);
						break;
					case Waveform::RampDown:
						if (t_offset < 0.5)
							value -= t_offset * 2.0 * vibrato_depth;
						else
							value += (1.0 - t_offset) * 2.0 * vibrato_depth;
						break;
					case Waveform::Square:
						if (t_offset < 0.5)
							value += vibrato_depth;
						else
							value -= vibrato_depth;
						break;
				}

				if (value < 1)
					value = 1;

				double this_note_frequency;
				
				if (it_linear_slides)
					this_note_frequency = p2(value);
				else
					this_note_frequency = 14317056.0 / value;

				delta_offset_per_tick = this_note_frequency / ticks_per_second;
				LINT_DOUBLE(delta_offset_per_tick);
			}

			profile.push_back("end vibrato");
		}

		if (panbrello)
		{
			profile.push_back("start panbrello");

			double t = double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;

			double t_panbrello = (panbrello_cycle_offset + t) * panbrello_cycle_frequency;
			double t_offset = t_panbrello - floor(t_panbrello);
			double value = panbrello_middle;
			
			switch (panbrello_waveform)
			{
				case Waveform::Triangle:
				case Waveform::Sawtooth:
				case Waveform::Sample:
					// Undefined.
					break;
				case Waveform::Sine:
					value += panbrello_depth * sin(t_panbrello * 6.283185);
					break;
				case Waveform::RampDown:
					if (t_offset < 0.5)
						value -= t_offset * 2.0 * panbrello_depth;
					else
						value += (1.0 - t_offset) * 2.0 * panbrello_depth;
					break;
				case Waveform::Square:
					if (t_offset < 0.5)
						value += panbrello_depth;
					else
						value -= panbrello_depth;
					break;
			}

			panning.from_linear_pan(value, -1.0, +1.0);

			profile.push_back("end panbrello");
		}

		if (tremor)
		{
			profile.push_back("start tremor");

			double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
			int frame_mod = (int(floor(frame)) + tremor_frame_offset) % (tremor_ontime + tremor_offtime);
			if (frame_mod >= tremor_ontime)
			{
				sample = 0.0;
				return ChannelPlaybackState::Ongoing;
			}

			profile.push_back("end tremor");
		}

		if (arpeggio)
		{
			profile.push_back("start arpeggio");

			double fiftieths_of_second = 50.0 * double(module->ticks_per_module_row - ticks_left + arpeggio_tick_offset) / ticks_per_second;
			switch (int(floor(fiftieths_of_second)) % 3)
			{
				case 0: delta_offset_per_tick = arpeggio_first_delta_offset;  break;
				case 1: delta_offset_per_tick = arpeggio_second_delta_offset; break;
				case 2: delta_offset_per_tick = arpeggio_third_delta_offset;  break;
			}

			LINT_DOUBLE(delta_offset_per_tick);

			profile.push_back("end arpeggio");
		}

		if (note_delay)
		{
			profile.push_back("start note_delay");

			double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
			if (frame >= note_delay_frames)
			{
				note_delay = false;

				if (delayed_note->volume >= 0)
				{
					intensity = original_intensity * delayed_note->volume / 64.0;
					volume = delayed_note->volume;
				}

				if (delayed_note->snote != SNOTE_EMPTY)
				{
					if (delayed_note->snote == SNOTE_NOTE_CUT)
						note_cut();
					else if (delayed_note->snote == SNOTE_NOTE_OFF)
						note_off();
					else if (delayed_note->snote == SNOTE_NOTE_FADE)
						note_fade();
					else
					{
						if ((delayed_note->instrument != NULL) || module->it_module)
						{
							if (!delayed_note->effect.keepNote())
							{
								if (delayed_note->instrument != NULL)
									current_sample = delayed_note->instrument;

								if (current_sample != NULL)
								{
									int translated_znote = delayed_note->znote;
									profile.push_back("note_delay: call begin_new_note");
									current_sample->begin_new_note(delayed_note, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
									fading = false;
									profile.push_back("note_delay: call recalc");
									recalc(translated_znote, 1.0, false);
									profile.push_back("note_delay: call recalc returned");
								}
								else
									current_sample_context = NULL;
							}
							if (delayed_note->volume < 0)
							{
								if (current_sample_context != NULL)
									volume = (int)(current_sample_context->default_volume * 64.0);
								else
									volume = 64;

								intensity = original_intensity * (volume / 64.0);
							}
						}
					}
				}
			}
		}

		if (retrigger)
		{
			profile.push_back("start retrigger");

			if (current_sample && retrigger_ticks && (module->current_row >= 0) && (((module->ticks_per_module_row - ticks_left) % retrigger_ticks) == 0))
			{
				offset = 0.0;
				offset_major = 0;
				int ignored = 0;
				current_sample->begin_new_note(&module->pattern_list[module->current_pattern]->row_list[unsigned(module->current_row)][unsigned(unmapped_channel_number)], this, &current_sample_context, module->ticks_per_frame, true, &ignored);
				fading = false;
				intensity = intensity * retrigger_factor + retrigger_bias;
				if (intensity > original_intensity)
					intensity = original_intensity;
				if (intensity < 0.0)
					intensity = 0.0;
				volume = int(intensity * 64.0 / original_intensity);
			}

			profile.push_back("end retrigger");
		}

		if (tremolo)
		{
			profile.push_back("start tremolo");

			double t = double(ticks_left) / module->ticks_per_module_row;

			if (t > tremolo_start_t)
			{
				t -= tremolo_start_t;
				double t_tremolo = (tremolo_cycle_offset + t) * tremolo_cycle_frequency;
				double t_offset = t_tremolo - floor(t_tremolo);
				intensity = tremolo_middle_intensity;

				switch (tremolo_waveform)
				{
					case Waveform::Triangle:
					case Waveform::Sawtooth:
					case Waveform::Sample:
						// Undefined.
						break;

					case Waveform::Sine:
						intensity += tremolo_depth * sin(t_tremolo * 6.283185);
						break;
					case Waveform::RampDown:
						if (t_offset < 0.5)
							intensity -= t_offset * 2.0 * tremolo_depth;
						else
							intensity += (1.0 - t_offset) * 2.0 * tremolo_depth;
						break;
					case Waveform::Square:
						if (t_offset < 0.5)
							intensity += tremolo_depth;
						else
							intensity -= tremolo_depth;
						break;
				}

				if (intensity < 0.0)
					intensity = 0.0;
				if (intensity > original_intensity)
					intensity = original_intensity;
			}

			profile.push_back("end tremolo");
		}

		if (note_cut_at_frame)
		{
			profile.push_back("note cut");

			double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
			if (frame >= note_cut_frames)
			{
				volume = 0;
				intensity = 0.0;
			}
		}

		if (tempo_slide)
		{
			profile.push_back("tempo slide");

			int ticks_in = module->ticks_per_module_row - ticks_left;
			double frame = (module->speed - 1) * double(ticks_in) / module->ticks_per_module_row;
			tempo = (int)(original_tempo + frame * tempo_change_per_frame);
		}

		profile.push_back("check ticks left");

		if (ticks_left)
		{
			profile.push_back("ticks are left");
			return ChannelPlaybackState::Ongoing;
		}

		if (volume_slide)
		{
			if (target_volume < 0)
				volume = 0;
			else if (target_volume > 64)
				volume = 64;
			else
				volume = target_volume;

			intensity = original_intensity * volume / 64.0;
		}

		profile.push_back("save state");

		p_volume_slide = volume_slide;                       volume_slide = false;
		p_portamento = portamento;                           portamento = false;
		p_vibrato = vibrato;                                 vibrato = false;
		p_tremor = tremor;                                   tremor = false;
		p_arpeggio = arpeggio;                               arpeggio = false;
		p_note_delay = note_delay;                           note_delay = false;
		p_retrigger = retrigger;                             retrigger = false;
		p_tremolo = tremolo;                                 tremolo = false;
		p_note_cut_at_frame = note_cut_at_frame;             note_cut_at_frame = false;
		p_panning_slide = panning_slide;                     panning_slide = false;
		p_channel_volume_slide = channel_volume_slide;       channel_volume_slide = false;
		p_panbrello = panbrello;                             panbrello = false;
		p_tempo_slide = tempo_slide;                         tempo_slide = false;
		p_global_volume_slide = global_volume_slide;         global_volume_slide = false;
		p_pattern_delay_by_frames = pattern_delay_by_frames; pattern_delay_by_frames = false;

		ticks_left = module->ticks_per_module_row;

		if (pattern_jump_target >= 0)
		{
			profile.push_back("check for pattern jump");

			if (pattern_jump_target >= int(module->pattern_list_length))
			{
				if (module->auto_loop_target >= 0)
				{
					module->current_pattern = unsigned(module->auto_loop_target);
					module->current_row = -1;
					pattern_jump_target = -1;
					row_jump_target = 0;

					if (module->current_pattern >= module->pattern_list_length)
						module->finished = true;
				}
				else if (looping)
				{
					module->current_pattern = 0;
					module->current_row = -1;
					pattern_jump_target = -1;
					row_jump_target = 0;
				}
				else
					module->finished = true;
			}
			else
			{
				module->current_pattern = unsigned(pattern_jump_target);
				module->current_row = row_jump_target - 1;
				pattern_jump_target = -1;
				row_jump_target = 0;
			}

			profile.push_back("pattern jump end");
		}

		profile.push_back("check for pattern delay");

		if (pattern_delay)
		{
			profile.push_back("there is pattern delay");

			pattern_delay--;
			ticks_left = module->ticks_per_module_row;
			dropoff_start = 0;
			rest_ticks = 0;
			cutoff_ticks = 0;

			return ChannelPlaybackState::Ongoing;
		}

		bool just_looped = false;

		if (unmapped_channel_number == 0)
		{
			profile.push_back("we are on channel 0, so advance current_row/current_pattern");

			if (p_pattern_delay_by_frames)
				module->speed -= pattern_delay_frames;

			if (module->override_next_row >= 0)
			{
				module->current_row = module->override_next_row;
				module->override_next_row = -1;

				just_looped = true;
			}
			else
				module->current_row++;

			if (module->current_row == int(module->pattern_list[module->current_pattern]->row_list.size()))
			{
				module->current_row = 0;
				module->current_pattern++;

				while (module->pattern_list[module->current_pattern]->is_skip_marker)
					module->current_pattern++;

				if (module->pattern_list[module->current_pattern]->is_end_marker)
				{
					if (looping)
						module->current_pattern = 0;
					else
						module->finished = true;
				}
			}

			stringstream ss;

			ss << module->current_pattern << ':' << setw(3) << setfill('0') << module->current_row;

			notify_new_pattern_row_started(ss.str().c_str());
		}

		if (module->finished)
		{
			finished = true;

			for (size_t i=0; i < ancillary_channels.size(); i++)
				ancillary_channels[i]->finished = true;

			return ChannelPlaybackState::Finished;
		}

		vector<row> &row_list = module->pattern_list[module->current_pattern]->row_list[unsigned(module->current_row)];

		if (unmapped_channel_number == 0)
		{
			// Scan through all channels and apply global effects up-front. We only want to do this
			// once, so the scan is done at the start of processing channel 0.

			bool recalc = false;

			for (unsigned int i=0; i<MAX_MODULE_CHANNELS; i++)
			{
				if (i >= row_list.size())
					break;

				if (row_list[i].effect.present == false)
					continue;

				switch (row_list[i].effect.command)
				{
					case Effect::SetSpeed: // 'A'
						if (row_list[i].effect.info.data)
							module->speed = row_list[i].effect.info.data;
						recalc = true;
						break;
					case Effect::OrderJump: // 'B'
						pattern_jump_target = row_list[i].effect.info.data;
						break;
					case Effect::PatternJump: // 'C'
						pattern_jump_target = int(module->current_pattern + 1);
						if (it_effects)
							row_jump_target = row_list[i].effect.info;
						else
							row_jump_target = row_list[i].effect.info.high_nybble * 10 + row_list[i].effect.info.low_nybble;
						while (row_jump_target >= 64)
						{
							pattern_jump_target++;
							row_jump_target -= 64;
						}
						break;
					case Effect::S3MExtendedEffect: // 'S'
						switch (row_list[i].effect.info.high_nybble)
						{
							case S3MExtendedEffect::FinePatternDelay: // 0x6, pattern delay in ticks
								// If multiple columns do this, they stack.
								if (!pattern_delay_by_frames)
								{
									pattern_delay_by_frames = true;
									pattern_delay_frames = row_list[i].effect.info.low_nybble;
								}
								else
									pattern_delay_frames += (int)row_list[i].effect.info.low_nybble;
								module->speed += pattern_delay_frames;
								break;
							case S3MExtendedEffect::PatternDelay: // 0xE, pattern delay in frames
								// If multiple columns do this, only the first one in the row takes effect
								if (pattern_delay == 0)
									pattern_delay = row_list[i].effect.info.low_nybble;
								break;

							case S3MExtendedEffect::PatternLoop: // 0xB
								if (row_list[i].effect.info.low_nybble == 0)
								{
									if (!just_looped)
									{
										module->pattern_loop[i].row = module->current_row;
										module->pattern_loop[i].need_repetitions = true;
									}
								}
								else
								{
									if (module->pattern_loop[i].need_repetitions)
									{
										module->pattern_loop[i].repetitions = row_list[i].effect.info.low_nybble;
										module->pattern_loop[i].need_repetitions = false;
									}

									if (module->pattern_loop[i].repetitions)
									{
										module->override_next_row = module->pattern_loop[i].row;
										module->pattern_loop[i].repetitions--;
									}
								}
								break;
						}
						break;
					case Effect::Tempo: // 'T'
						if ((row_list[i].effect.info.data < 0x20) && it_effects)
						{
							tempo_slide = true;
							original_tempo = tempo;
							if (row_list[i].effect.info.high_nybble)
								tempo_change_per_frame = row_list[i].effect.info.low_nybble;
							else
								tempo_change_per_frame = -(signed)row_list[i].effect.info.low_nybble;
						}
						else
							module->tempo = row_list[i].effect.info;
						recalc = true;
						break;
					case Effect::GlobalVolume: // 'V'
						if (row_list[i].effect.info > 64)
							global_volume = 1.0;
						else
							global_volume = row_list[i].effect.info / 64.0;

						break;
					case Effect::GlobalVolumeSlide: // 'W'
					{
						effect_info_type info = row_list[i].effect.info;

						if (info.data == 0) // repeat
							info = last_param['W'];
						else
							last_param['W'] = info;

						global_volume_slide_start = global_volume;

						if (info.low_nybble == 0) // slide up
							global_volume_slide_end = global_volume_slide_start + (module->speed - 1) * info.high_nybble / 32.0;
						else if (info.high_nybble == 0) // slide down
							global_volume_slide_end = global_volume_slide_start - (module->speed - 1) * info.low_nybble / 32.0;
						else if (it_effects)
						{
							if (info.low_nybble == 0xF) // fine slide up
								global_volume_slide_end = global_volume_slide_start + info.high_nybble / 2000.0;
							else if (info.high_nybble == 0xF) // fine slide down
								global_volume_slide_end = global_volume_slide_start - info.high_nybble / 2000.0;
						}

						if (global_volume_slide_end == global_volume_slide_start)
							break;

						global_volume_slide = true;

						break;
					}
					case Effect::SetEnvelopePosition:
					{
						effect_info_type info = row_list[i].effect.info;

						if (module->xm_module)
							envelope_offset = (long)(info.data * module->ticks_per_frame);
						else
							cerr << "Ignoring XM-specific effect Effect::SetEnvelopePosition in non-XM module" << endl;

						break;
					}
				}
			}

			if (recalc)
			{
				module->speed_change();

				for (size_t i=0; i < channel_group->size(); i++)
					channel_group->at(i)->ticks_per_frame_changed();
			}
		}

		if (enabled)
		{
			row &row = row_list[channel_number];

			if (trace_mod)
			{
				if (channel_number == 0)
				{
					cerr << endl;
					if (module->current_row == 0)
						cerr << string(9 + 18 * module->num_channels, '-') << endl;

					cerr << setfill(' ') << setw(3) << module->current_pattern << ":"
								<< setfill('0') << setw(req_digits(int(module->pattern_list[module->current_pattern]->row_list.size()))) << module->current_row << " | ";
				}

				format_pattern_note(cerr, row);

				cerr << "    ";
			}

			if (row.volume >= 0)
			{
				if (row.volume > 64)
				{
					intensity = original_intensity;
					volume = 64;
				}
				else
				{
					intensity = original_intensity * row.volume / 64.0;
					volume = row.volume;
				}
			}

			if (row.snote != -1)
			{
				if (row.snote == SNOTE_NOTE_CUT)
				{
					if (anticlick && current_sample)
					{
						note_off(true, false);

						channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);

						ancillary->volume_envelope = volume_envelope;
						ancillary->panning_envelope = panning_envelope;
						ancillary->pitch_envelope = pitch_envelope;
						ancillary->fading = true;
						ancillary->fade_value = 1.0;

						add_ancillary_channel(ancillary);

						volume_envelope = NULL;
						panning_envelope = NULL;
						pitch_envelope = NULL;

						ancillary_channels.push_back(ancillary);
					}

					note_cut();
				}
				else if (row.snote == SNOTE_NOTE_OFF)
					note_off();
				else if (row.snote == SNOTE_NOTE_FADE)
					note_fade();
				else
				{
					if ((row.instrument != NULL) || module->it_module)
					{
						if (!row.effect.keepNote() || (current_sample == NULL))
						{
							if (anticlick && current_sample)
							{
								note_off(true, false);

								channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);

								ancillary->volume_envelope = volume_envelope;
								ancillary->panning_envelope = panning_envelope;
								ancillary->pitch_envelope = pitch_envelope;
								ancillary->fading = true;
								ancillary->fade_value = 1.0;

								add_ancillary_channel(ancillary);

								volume_envelope = NULL;
								panning_envelope = NULL;
								pitch_envelope = NULL;

								ancillary_channels.push_back(ancillary);
							}

							if (row.instrument)
								current_sample = row.instrument;
							if (current_sample != NULL)
							{
								int translated_znote = row.znote;
								current_sample->begin_new_note(&row, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
								fading = false;
								recalc(translated_znote, 1.0, false);
							}
							else if (current_sample_context != NULL)
							{
								delete current_sample_context;
								current_sample_context = NULL;
							}
						}
						if (row.volume < 0)
						{
							if (current_sample_context != NULL)
								volume = (int)(current_sample_context->default_volume * 64.0);
							else
								volume = 64;

							intensity = original_intensity * (volume / 64.0);
						}
					}
				}
			}
			else if (row.instrument != NULL)
			{
				if (anticlick && current_sample)
				{
					note_off(true, false);

					channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);

					ancillary->volume_envelope = volume_envelope;
					ancillary->panning_envelope = panning_envelope;
					ancillary->pitch_envelope = pitch_envelope;
					ancillary->fading = true;
					ancillary->fade_value = 1.0;

					add_ancillary_channel(ancillary);

					volume_envelope = NULL;
					panning_envelope = NULL;
					pitch_envelope = NULL;

					ancillary_channels.push_back(ancillary);
				}

				if (!row.effect.keepNote())
				{
					current_sample = row.instrument;
					int translated_znote = current_znote;
					current_sample->begin_new_note(&row, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
					fading = false;
					recalc(translated_znote, 1.0, false);
				}
				if (row.volume < 0)
				{
					if (current_sample_context != NULL)
						volume = (int)(current_sample_context->default_volume * 64.0);
					else
						volume = 64;

					intensity = original_intensity * (volume / 64.0);
				}
			}

			effect_info_type info = row.effect.info;

			double old_frequency, old_delta_offset_per_tick;
			int old_current_znote;
			sample_context *temp_sc;

			int second_note, third_note;
			unsigned int target_offset;
			double portamento_target, before_finetune;

			if (p_vibrato && row.effect.isnt(Effect::Vibrato))
			{
				delta_offset_per_tick = note_frequency / ticks_per_second;
				LINT_DOUBLE(delta_offset_per_tick);
			}

			if (row.effect.present || row.secondary_effect.present)
			{
				effect_info_type secondary_info = row.secondary_effect.info;

				// Effects that can be doubled up.
				if (row.effect.is(Effect::ChannelVolume) || row.secondary_effect.is(Effect::ChannelVolume))
				{
					// When volume is doubled, effect wins over secondary effect.
					auto data = row.effect.is(Effect::ChannelVolume)
						? row.effect.info.data
						: row.secondary_effect.info.data;

					if (data > 64)
						channel_volume = 1.0;
					else
						channel_volume = data / 64.0;
				}

				bool primary_is_volume_slide = row.effect.is(Effect::VolumeSlide) || row.effect.is(Effect::VibratoAndVolumeSlide) || row.effect.is(Effect::TonePortamentoAndVolumeSlide);
				bool secondary_is_volume_slide = row.secondary_effect.is(Effect::VolumeSlide) || row.secondary_effect.is(Effect::VibratoAndVolumeSlide) || row.secondary_effect.is(Effect::TonePortamentoAndVolumeSlide);

				if (primary_is_volume_slide || secondary_is_volume_slide)
				{
					// When doubled, this effect compounds.
					if (primary_is_volume_slide)
					{
						if (info.data == 0) // repeat
							info = last_param[Effect::VolumeSlide];
						else
							last_param[Effect::VolumeSlide] = info;
					}

					int frame_delta = 0;

					if (secondary_is_volume_slide)
					{
						// Secondary needs to be processed first to satisfy XM, as it is used for the XM volume column.
						if (secondary_info.low_nybble == 0) // slide up
							frame_delta += (module->speed - 1) * secondary_info.high_nybble;
						else if (secondary_info.high_nybble == 0) // slide down
							frame_delta -= (module->speed - 1) * secondary_info.low_nybble;
						else if (secondary_info.low_nybble == 0xF) // fine adjustment
							volume = min(64, volume + int(secondary_info.high_nybble));
						else if (secondary_info.high_nybble == 0xF) // fine adjustment
							volume = max(0, volume - int(secondary_info.low_nybble));
					}

					if (primary_is_volume_slide)
					{
						if (info.low_nybble == 0) // slide up
							frame_delta += (module->speed - 1) * info.high_nybble;
						else if (info.high_nybble == 0) // slide down
							frame_delta -= (module->speed - 1) * info.low_nybble;
						else if (info.low_nybble == 0xF) // fine adjustment
							volume = min(64, volume + int(info.high_nybble));
						else if (info.high_nybble == 0xF) // fine adjustment
							volume = max(0, volume - int(info.low_nybble));
					}

					if (frame_delta != 0)
					{
						target_volume = volume + frame_delta;

						if (target_volume < 0)
							target_volume = 0;
						if (target_volume > 64)
							target_volume = 64;

						if (target_volume != volume)
						{
							target_intensity = original_intensity * (target_volume / 64.0);
							previous_intensity = intensity;
							volume_slide = true;
						}
					}
				}

				bool primary_is_vibrato = row.effect.is(Effect::Vibrato) || row.effect.is(Effect::SetVibratoSpeed) || row.effect.is(Effect::VibratoAndVolumeSlide);
				bool secondary_is_vibrato = row.secondary_effect.is(Effect::Vibrato) || row.secondary_effect.is(Effect::SetVibratoSpeed);

				if (primary_is_vibrato || secondary_is_vibrato)
				{
					bool primary_is_full_vibrato = primary_is_vibrato && row.effect.isnt(Effect::SetVibratoSpeed);
					bool secondary_is_full_vibrato = secondary_is_vibrato && row.secondary_effect.isnt(Effect::SetVibratoSpeed);

					// When doubled, this effect compounds.
					if (secondary_is_vibrato)
					{
						if (secondary_info.high_nybble != 0)
						{
							double new_vibrato_cycle_frequency = (module->speed * secondary_info.high_nybble) / 64.0;

							if (p_vibrato && (new_vibrato_cycle_frequency != vibrato_cycle_frequency))
							{
								vibrato_cycle_offset *= (vibrato_cycle_frequency / new_vibrato_cycle_frequency);
								LINT_DOUBLE(vibrato_cycle_frequency);
							}

							vibrato_cycle_frequency = new_vibrato_cycle_frequency;
							LINT_DOUBLE(vibrato_cycle_frequency);
						}
					}

					if (primary_is_vibrato)
					{
						auto last = last_param[Effect::Vibrato];
						auto next = last;

						if (info.high_nybble != 0)
							next = (unsigned char)((next & 0x0F) | (info.high_nybble << 4));
						else
							info.high_nybble = next >> 4;

						if (info.low_nybble != 0)
							next = (unsigned char)((next & 0xF0) | info.low_nybble);
						else
							info.low_nybble = next & 0xF;

						last_param[Effect::Vibrato] = next;

						int new_frequency_index = next >> 4;

						if (new_frequency_index != 0)
						{
							double new_vibrato_cycle_frequency = (module->speed * new_frequency_index) / 64.0;

							if (p_vibrato && (new_vibrato_cycle_frequency != vibrato_cycle_frequency))
							{
								vibrato_cycle_offset *= (vibrato_cycle_frequency / new_vibrato_cycle_frequency);
								LINT_DOUBLE(vibrato_cycle_offset);
							}

							vibrato_cycle_frequency = new_vibrato_cycle_frequency;
							LINT_DOUBLE(vibrato_cycle_frequency);
						}
					}

					bool restart_primary = primary_is_full_vibrato && ((info.low_nybble != 0) && (info.high_nybble != 0));
					bool restart_secondary = secondary_is_full_vibrato && ((secondary_info.low_nybble != 0) && (secondary_info.high_nybble != 0));

					auto depth_param = (primary_is_full_vibrato && (info.low_nybble != 0))
						? info.low_nybble : (secondary_is_full_vibrato ? secondary_info.low_nybble : 0);

					if (depth_param > 0)
					{
						if (it_linear_slides)
							vibrato_depth = depth_param * (255.0 / 128.0) / -192.0;
						else
							vibrato_depth = depth_param * 4.0 * (255.0 / 128.0);

						if (it_new_effects)
							vibrato_depth *= 0.5;
					}

					if (!restart_primary && !restart_secondary)
					{
						if (!vibrato_retrig)
						{
							vibrato_cycle_offset += (1.0 - vibrato_start_t);
							LINT_DOUBLE(vibrato_cycle_offset);
						}
					}
					else
					{
						if (p_vibrato) // already vibratoing
							vibrato_cycle_offset += (1.0 - vibrato_start_t);
						else
							vibrato_cycle_offset = 0.0;

						LINT_DOUBLE(vibrato_cycle_offset);

						if (it_new_effects)
							vibrato_start_t = 0.0;
						else
						{
							vibrato_start_t = 1.0 / module->speed;
							LINT_DOUBLE(vibrato_start_t);
						}
					}
					vibrato = true;
				}

				if (row.effect.is(Effect::Panning) || row.secondary_effect.is(Effect::Panning)
				|| row.effect.is(Effect::PanSlide) || row.secondary_effect.is(Effect::PanSlide))
				{
					// When doubled, the set goes to the primary, and slides compound.
					if (module->stereo)
					{
						if (row.effect.is(Effect::Panning))
						{
							if (amiga_panning)
								panning.from_amiga_pan(info.data);
							else
								panning.from_mod_pan(info.data);
						}
						else if (row.secondary_effect.is(Effect::Panning))
						{
							if (amiga_panning)
								panning.from_amiga_pan(secondary_info.data);
							else
								panning.from_mod_pan(secondary_info.data);
						}

						if (info.data == 0) // repeat
							info = last_param[Effect::PanSlide];
						else
							last_param[Effect::PanSlide] = info;

						panning_slide_start = panning.to_linear_pan(-1.0, +1.0);

						bool fine = false;

						if (it_effects && ((info.high_nybble == 0xF) || (info.low_nybble == 0xF)))
						{
							if (info.low_nybble == 0xF) // fine pan left
							{
								panning.from_linear_pan(panning_slide_start - info.high_nybble / 32.0, -1.0, +1.0);
								fine = true;
							}
							else if (info.high_nybble == 0xF) // fine pan right
							{
								panning.from_linear_pan(panning_slide_start + info.high_nybble / 32.0, -1.0, +1.0);
								fine = true;
							}
						}
						else
						{
							int delta = 0;

							if (row.effect.is(Effect::PanSlide))
							{
								if (info.low_nybble != 0)
									delta += int(info.low_nybble);
								else
									delta -= int(info.high_nybble);
							}

							if (row.secondary_effect.is(Effect::PanSlide))
							{
								if (secondary_info.low_nybble != 0)
									delta += int(secondary_info.low_nybble);
								else
									delta -= int(secondary_info.high_nybble);
							}

							panning_slide_end = panning_slide_start + (module->speed - 1) * delta / 32.0;
						}

						if (!fine)
						{
							if (panning_slide_end != panning_slide_start)
								panning_slide = true;
						}
					}
				}

				bool primary_is_tone_portamento = row.effect.is(Effect::TonePortamento) || row.effect.is(Effect::TonePortamentoAndVolumeSlide);
				bool secondary_is_tone_portamento = row.secondary_effect.is(Effect::TonePortamento) || row.secondary_effect.is(Effect::TonePortamentoAndVolumeSlide);

				if (primary_is_tone_portamento || secondary_is_tone_portamento)
				{
					portamento_speed = 0;

					if (primary_is_tone_portamento)
					{
						if (info.data != 0)
						{
							portamento_speed += info.data * 4;
							if (it_portamento_link)
								last_param[Effect::PortamentoDown] = last_param[Effect::PortamentoUp] = info;
							else
								last_param[Effect::TonePortamento] = info;
						}
						else if (it_portamento_link)
							portamento_speed += last_param[Effect::PortamentoDown].data * 4;
						else
							portamento_speed += last_param[Effect::TonePortamento].data * 4;
					}

					if (secondary_is_tone_portamento)
						portamento_speed += secondary_info.data * 4;

					if (row.snote >= 0)
						portamento_target_znote = row.znote;

					if (it_portamento_link && (row.instrument != NULL))
					{
						if (current_sample != row.instrument)
						{
							double ratio = row.instrument->samples_per_second / current_sample->samples_per_second;
							note_frequency *= ratio;
							delta_offset_per_tick /= ratio;
							current_sample->kill_note(current_sample_context);
						}

						current_sample = row.instrument;
						int ignored;
						current_sample->begin_new_note(&row, this, &current_sample_context, module->ticks_per_frame, true, &ignored);
						fading = false;
					}

					old_frequency = note_frequency;
					old_delta_offset_per_tick = delta_offset_per_tick;
					old_current_znote = current_znote;

					recalc(portamento_target_znote, 1.0, false, false);

					if (it_linear_slides)
						portamento_target = lg(note_frequency);
					else
						portamento_target = 14317056.0 / note_frequency;

					note_frequency = old_frequency;
					delta_offset_per_tick = old_delta_offset_per_tick;
					current_znote = old_current_znote;

					if (it_linear_slides)
					{
						portamento_start = lg(note_frequency);
						if (portamento_target > portamento_start)
							portamento_end = portamento_start - portamento_speed * (module->speed - 1) / 768.0;
						else
							portamento_end = portamento_start + portamento_speed * (module->speed - 1) / 768.0;
					}
					else
					{
						portamento_start = 14317056.0 / note_frequency;
						if (portamento_target > portamento_start)
							portamento_end = portamento_start + portamento_speed * (module->speed - 1);
						else
							portamento_end = portamento_start - portamento_speed * (module->speed - 1);
					}

					portamento_end_t = (portamento_target - portamento_start) / (portamento_end - portamento_start);

					portamento = true;
				}

				switch (row.effect.command)
				{
					case Effect::MODExtraEffects: // 0xE
						if (info.high_nybble == 0xF) // invert loop
						{
							// TODO
							// delta_offset_per_tick = -info.low_nybble;
						}
						break;

					// Global effects, processed up-front; ignore when we get to them here during the per-channel processing.
					case Effect::SetSpeed:          // 'A'
					case Effect::OrderJump:         // 'B'
					case Effect::PatternJump:       // 'C' -- jumps into next pattern, specified row
					case Effect::Tempo:             // 'T'
					case Effect::GlobalVolume:      // 'V'
					case Effect::GlobalVolumeSlide: // 'W'
						break;

					// Effects that could be in secondary effect; processed up-front to handle the case where it's specified in both.
					case Effect::VolumeSlide:    // 'D'
					case Effect::TonePortamento: // 'G'
					case Effect::Vibrato:        // 'H'
					case Effect::ChannelVolume:  // 'M'
					case Effect::PanSlide:       // 'P'
					case Effect::Panning:        // 'X'
						break;

					case Effect::PortamentoDown: // 'E'
						if (it_linear_slides)
							portamento_start = lg(note_frequency);
						else
							portamento_start = 14317056.0 / note_frequency;

						if (info.data == 0)
							info = last_param[Effect::PortamentoDown];
						else
							last_param[Effect::PortamentoDown] = last_param[Effect::PortamentoUp] = info;

						if ((info.high_nybble == 0xF) || (info.high_nybble == 0xE))
						{
							if (info.high_nybble == 0xF) // 'fine'
							{
								if (it_linear_slides)
									portamento_target = portamento_start - info.low_nybble / 192.0;
								else
									portamento_target = portamento_start + 4 * info.low_nybble;
							}
							else // E - 'extra fine'
							{
								if (it_linear_slides)
									portamento_target = portamento_start - info.low_nybble / 768.0;
								else
									portamento_target = portamento_start + info.low_nybble;
							}

							if (it_linear_slides)
								note_frequency = p2(portamento_target);
							else
								note_frequency = 14317056.0 / portamento_target;

							LINT_DOUBLE(note_frequency);

							delta_offset_per_tick = note_frequency / ticks_per_second;
							LINT_DOUBLE(delta_offset_per_tick);
						}
						else
						{
							if (it_linear_slides)
								portamento_end = portamento_start - info.data * (module->speed - 1) / 192.0;
							else
								portamento_end = portamento_start + 4 * info.data * (module->speed - 1);

							portamento_end_t = 1.0;
							portamento = true;
						}
						break;
					case Effect::PortamentoUp: // 'F'
						if (it_linear_slides)
							portamento_start = lg(note_frequency);
						else
							portamento_start = 14317056.0 / note_frequency;

						if (info.data == 0)
							info = last_param[Effect::PortamentoUp];
						else
							last_param[Effect::PortamentoDown] = last_param[Effect::PortamentoUp] = info;

						if ((info.high_nybble == 0xF) || (info.high_nybble == 0xE))
						{
							if (info.high_nybble == 0xF) // 'fine'
							{
								if (it_linear_slides)
									portamento_target = portamento_start + info.low_nybble / 192.0;
								else
									portamento_target = portamento_start - 4 * info.low_nybble;
							}
							else // E -- 'extra fine'
							{
								if (it_linear_slides)
									portamento_target = portamento_start + info.low_nybble / 768.0;
								else
									portamento_target = portamento_start - info.low_nybble;
							}

							if (it_linear_slides)
								note_frequency = p2(portamento_target);
							else
								note_frequency = 14317056.0 / portamento_target;

							LINT_DOUBLE(note_frequency);

							delta_offset_per_tick = note_frequency / ticks_per_second;
							LINT_DOUBLE(delta_offset_per_tick);
						}
						else
						{
							if (it_linear_slides)
								portamento_end = portamento_start + info.data * (module->speed - 1) / 192.0;
							else
								portamento_end = portamento_start - 4 * info.data * (module->speed - 1);

							portamento_end_t = 1.0;
							portamento = true;
						}
						break;
					case Effect::Tremor: // 'I', high = ontime, low = offtime
						if ((info.low_nybble == 0) || (info.high_nybble == 0)) // repeat
							tremor_frame_offset += module->speed;
						else
						{
							if (p_tremor) // already tremoring
								tremor_frame_offset += module->speed;
							else
								tremor_frame_offset = 0;
							tremor_ontime = info.high_nybble;
							tremor_offtime = info.low_nybble;
						}
						tremor = true;
						break;
					case Effect::Arpeggio: // 'J', high = first addition, low = second addition
						old_frequency = note_frequency;
						old_delta_offset_per_tick = delta_offset_per_tick;
						old_current_znote = current_znote;

						if (p_arpeggio) // already arpeggioing
							arpeggio_tick_offset += module->previous_ticks_per_module_row;
						else
							arpeggio_tick_offset = 0;

						if (current_sample == NULL)
							break;

						temp_sc = current_sample_context;
						current_sample_context = NULL; // protect the existing context

						arpeggio_first_delta_offset = delta_offset_per_tick;

						second_note = row.znote + info.high_nybble;
						current_sample->begin_new_note(&row, NULL, &current_sample_context, module->ticks_per_frame, true, &second_note);
						recalc(second_note, 1.0, false);
						if (current_sample_context)
						{
							delete current_sample_context;
							current_sample_context = NULL;
						}
						arpeggio_second_delta_offset = delta_offset_per_tick;

						third_note = row.znote + info.low_nybble;
						current_sample->begin_new_note(&row, NULL, &current_sample_context, module->ticks_per_frame, true, &third_note);
						recalc(third_note, 1.0, false);
						if (current_sample_context)
						{
							delete current_sample_context;
							current_sample_context = NULL;
						}
						arpeggio_third_delta_offset = delta_offset_per_tick;

						current_sample_context = temp_sc;

						note_frequency = old_frequency;
						delta_offset_per_tick = old_delta_offset_per_tick;
						current_znote = old_current_znote;

						arpeggio = true;
						break;
					case Effect::VibratoAndVolumeSlide: // 'K'
						if (!vibrato_retrig)
						{
							vibrato_cycle_offset += (1.0 - vibrato_start_t);
							LINT_DOUBLE(vibrato_cycle_offset);
						}

						vibrato = true;
						break;
					case Effect::TonePortamentoAndVolumeSlide: // 'L'
						if (row.snote >= 0)
							portamento_target_znote = row.znote;
						
						old_frequency = note_frequency;
						old_delta_offset_per_tick = delta_offset_per_tick;
						old_current_znote = current_znote;

						temp_sc = current_sample_context;
						current_sample_context = NULL; // protect the existing context

						current_sample->begin_new_note(&row, NULL, &current_sample_context, module->ticks_per_frame, true, &portamento_target_znote);
						fading = false;
						recalc(portamento_target_znote, 1.0, false, false);
						if (current_sample_context)
						{
							delete current_sample_context;
							current_sample_context = NULL;
						}

						current_sample_context = temp_sc;

						if (it_linear_slides)
							portamento_target = lg(note_frequency);
						else
							portamento_target = 14317056.0 / note_frequency;

						note_frequency = old_frequency;
						delta_offset_per_tick = old_delta_offset_per_tick;
						current_znote = old_current_znote;

						if (it_linear_slides)
						{
							portamento_start = lg(note_frequency);
							if (portamento_target_znote > current_znote)
								portamento_end = portamento_start + portamento_speed * (module->speed - 1) / 768.0;
							else
								portamento_end = portamento_start - portamento_speed * (module->speed - 1) / 768.0;
						}
						else
						{
							portamento_start = 14317056.0 / note_frequency;
							if (portamento_target_znote > current_znote)
								portamento_end = portamento_start - portamento_speed * (module->speed - 1);
							else
								portamento_end = portamento_start + portamento_speed * (module->speed - 1);
						}

						portamento_end_t = (portamento_target - portamento_start) / (portamento_end - portamento_start);

						portamento = true;

						break;
					case Effect::ChannelVolumeSlide: // channel volume slide
						if (it_effects)
						{
							if (info.data == 0) // repeat
								info = last_param[Effect::ChannelVolumeSlide];
							else
								last_param[Effect::ChannelVolumeSlide] = info;

							channel_volume_slide_start = channel_volume;

							bool fine = false;

							if (info.low_nybble == 0) // slide up
								channel_volume_slide_end = channel_volume_slide_start + (module->speed - 1) * info.high_nybble / 32.0;
							else if (info.high_nybble == 0) // slide down
								channel_volume_slide_end = channel_volume_slide_start - (module->speed - 1) * info.low_nybble / 32.0;
							else if (info.low_nybble == 0xF) // fine slide up
							{
								channel_volume = channel_volume_slide_start - info.high_nybble / 64.0;
								fine = true;
							}
							else if (info.high_nybble == 0xF) // fine slide down
							{
								channel_volume = channel_volume_slide_start + info.high_nybble / 64.0;
								fine = true;
							}

							if (!fine)
							{
								if (channel_volume_slide_end == channel_volume_slide_start)
									break;

								channel_volume_slide = true;
							}
						}
						else
							cerr << "Ignoring pre-IT command: " << row.effect.command
								<< setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
						break;
					case Effect::SampleOffset: // 'O'
						if (info.data == 0)
							info = last_param[Effect::SampleOffset];
						else
							last_param[Effect::SampleOffset] = info;

						target_offset = unsigned((unsigned(info.data) << 8) + set_offset_high);

						if (it_new_effects)
						{
							if (current_sample_context && (target_offset > current_sample_context->num_samples))
								break;
						}

						offset_major = target_offset;
						break;
					case Effect::Retrigger: // 'Q'
						if (info.data == 0)
							info = last_param[Effect::Retrigger];
						else
							last_param[Effect::Retrigger] = info;

						if (info.low_nybble != 0)
						{
							switch (info.high_nybble)
							{
								case 0:  retrigger_factor = 1.0;       retrigger_bias =   0.0; break;
								case 1:  retrigger_factor = 1.0;       retrigger_bias =  -1.0; break;
								case 2:  retrigger_factor = 1.0;       retrigger_bias =  -2.0; break;
								case 3:  retrigger_factor = 1.0;       retrigger_bias =  -4.0; break;
								case 4:  retrigger_factor = 1.0;       retrigger_bias =  -8.0; break;
								case 5:  retrigger_factor = 1.0;       retrigger_bias = -16.0; break;
								case 6:  retrigger_factor = 2.0 / 3.0; retrigger_bias =   0.0; break;
								case 7:  retrigger_factor = 1.0 / 2.0; retrigger_bias =   0.0; break;
								case 9:  retrigger_factor = 1.0;       retrigger_bias =   1.0; break;
								case 10: retrigger_factor = 1.0;       retrigger_bias =   2.0; break;
								case 11: retrigger_factor = 1.0;       retrigger_bias =   4.0; break;
								case 12: retrigger_factor = 1.0;       retrigger_bias =   8.0; break;
								case 13: retrigger_factor = 1.0;       retrigger_bias =  16.0; break;
								case 14: retrigger_factor = 3.0 / 2.0; retrigger_bias =   0.0; break;
								case 15: retrigger_factor = 2.0 / 1.0; retrigger_bias =   0.0; break;
							}

							retrigger_bias *= (original_intensity / 64.0);

							retrigger_ticks = module->ticks_per_module_row * info.low_nybble / module->speed;

							retrigger = true;
						}
						break;
					case Effect::Tremolo: // 'R'
						if ((info.low_nybble == 0) || (info.high_nybble == 0)) // repeat
						{
							if (row.snote >= 0)
								tremolo_middle_intensity = original_intensity * (volume / 64.0);
							if (tremolo_retrig)
								tremolo_cycle_offset += (1.0 - tremolo_start_t);
						}
						else
						{
							if (p_tremolo) // already tremoloing
								tremolo_cycle_offset += (1.0 - tremolo_start_t);
							else
								tremolo_cycle_offset = 0.0;
							tremolo_middle_intensity = intensity;
							tremolo_depth = original_intensity * (info.low_nybble * 4.0 * (255.0 / 64.0)) / 64.0;
							if (it_new_effects)
								tremolo_depth *= 0.5;
							double new_tremolo_cycle_frequency = double(info.high_nybble * module->speed) / 64.0;

							if (p_tremolo && (new_tremolo_cycle_frequency != tremolo_cycle_frequency))
								tremolo_cycle_offset *= (tremolo_cycle_frequency / new_tremolo_cycle_frequency);

							tremolo_cycle_frequency = new_tremolo_cycle_frequency;

							if (it_new_effects)
								tremolo_start_t = 0.0;
							else
							{
								vibrato_start_t = 1.0 / module->speed;
								LINT_DOUBLE(vibrato_start_t);
							}
						}
						tremolo = true;
						break;
					case Effect::S3MExtendedEffect: // 'S'
						switch (info.high_nybble)
						{
							// ignore song-wide commands
							case S3MExtendedEffect::FinePatternDelay: // 0x6
							case S3MExtendedEffect::PatternLoop:      // 0xB
								break;
							case S3MExtendedEffect::GlissandoControl: // 0x1
								if (info.low_nybble == 0)
									portamento_glissando = false;
								else if (info.low_nybble == 1)
									portamento_glissando = true;
								else
									cerr << "Invalid parameter value: S3M command S1"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << ")" << endl;
								break;
							case S3MExtendedEffect::SetFineTune: // 0x2
								if (current_sample == NULL)
									cerr << "No instrument for set finetune effect" << endl;
								else
								{
									before_finetune = current_sample->samples_per_second;

									current_sample->samples_per_second = mod_finetune[info.low_nybble];

									note_frequency *= (current_sample->samples_per_second / before_finetune);
									LINT_DOUBLE(note_frequency);

									delta_offset_per_tick = note_frequency / ticks_per_second;
									LINT_DOUBLE(delta_offset_per_tick);
								}
								break;
							case S3MExtendedEffect::SetVibratoWaveform: // 0x3
								switch (info.low_nybble & 0x3)
								{
									case 0x0: vibrato_waveform = Waveform::Sine;     break;
									case 0x1: vibrato_waveform = Waveform::RampDown; break;
									case 0x2: vibrato_waveform = Waveform::Square;   break;
									case 0x3:
										switch ((rand() >> 2) % 3)
										{
											case 0: vibrato_waveform = Waveform::Sine;     break;
											case 1: vibrato_waveform = Waveform::RampDown; break;
											case 2: vibrato_waveform = Waveform::Square;   break;
										}
								}
								if (info.low_nybble & 0x4)
									vibrato_retrig = false;
								else
									vibrato_retrig = true;

								if (info.low_nybble & 0x8)
									cerr << "Warning: bit 3 ignored in effect S3"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								break;
							case S3MExtendedEffect::SetTremoloWaveform: // 0x4
								switch (info.low_nybble & 0x3)
								{
									case 0x3:
									case 0x0: tremolo_waveform = Waveform::Sine;     break;
									case 0x1: tremolo_waveform = Waveform::RampDown; break;
									case 0x2: tremolo_waveform = Waveform::Square;   break;
									// case 0x3: tremolo_waveform = Waveform::Random;   break;
								}
								if (info.low_nybble & 0x4)
									tremolo_retrig = false;
								else
									tremolo_retrig = true;

								if (info.low_nybble & 0x8)
									cerr << "Warning: bit 3 ignored in effect S4"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								break;
							case S3MExtendedEffect::SetPanbrelloWaveform: // 0x5
								if (it_effects)
								{
									switch (info.low_nybble & 0x3)
									{
										case 0x3:
										case 0x0: panbrello_waveform = Waveform::Sine;     break;
										case 0x1: panbrello_waveform = Waveform::RampDown; break;
										case 0x2: panbrello_waveform = Waveform::Square;   break;
										// case 0x3: panbrello_waveform = Waveform::Random;   break;
									}
									if (info.low_nybble & 0x4)
										panbrello_retrig = false;
									else
										panbrello_retrig = true;

									if (info.low_nybble & 0x8)
										cerr << "Warning: bit 3 ignored in effect S5"
											<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								}
								else
									cerr << "Ignoring pre-IT command: S5"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								break;
							case S3MExtendedEffect::OverrideNewNoteAction: // 0x7, past note cut/off/fade, temporary new note action
								if (it_effects)
								{
									switch (info.low_nybble)
									{
										case 3: // set NNA to cut       ignore instrument-interpreted values
										case 4: // set NNA to continue 
										case 5: // set NNA to note off
										case 6: // set NNA to fade
										case 7: // disable volume envelope for this note
										case 8: // disable volume envelope for this note
											break;
										case 0: // note cut
											for (int i = int(ancillary_channels.size() - 1); i >= 0; i--)
											{
												auto my_own = find(my_ancillary_channels.begin(), my_ancillary_channels.end(), ancillary_channels[unsigned(i)]);

												if (my_own != my_ancillary_channels.end())
												{
													ancillary_channels.erase(ancillary_channels.begin() + i);
													my_ancillary_channels.erase(my_own);
													delete *my_own;
												}
											}
											break;
										case 1: // note off
											for (auto i = my_ancillary_channels.begin(), l = my_ancillary_channels.end(); i != l; ++i)
												(*i)->note_off();
											break;
										case 2: // fade
											for (auto i = my_ancillary_channels.begin(), l = my_ancillary_channels.end(); i != l; ++i)
											{
												channel &t = **i;
												t.fading = true;
												t.fade_value = 1.0;
												if (!t.have_fade_per_tick)
												{
													t.note_off(true, false);
													t.have_fade_per_tick = true;
												}
											}
											break;
										default:
											cerr << "Warning: argument meaning unspecified in effect S7"
												<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
									}
								}
								else
									cerr << "Ignoring pre-IT command: S7"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								break;
							case S3MExtendedEffect::RoughPanning: // 0x8
								if (module->stereo)
									panning.from_s3m_pan(module->base_pan[unmapped_channel_number] + info.low_nybble - 8);
								break;
							case S3MExtendedEffect::ExtendedITEffect:
								if (it_effects)
								{
									switch (info.low_nybble)
									{
										case 1: // set surround sound
											panning.to_surround_sound();
											break;
										default:
											cerr << "Warning: argument meaning unspecified in effect S9"
												<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
									}
								}
								else
									cerr << "Ignoring pre-IT command: S7"
										<< hex << uppercase << info.low_nybble << nouppercase << dec << endl;
								break;
							case S3MExtendedEffect::Panning:
								if (it_effects)
									set_offset_high = unsigned(info.low_nybble << 16);
								else
									if (module->stereo)
									{
										if (info.low_nybble > 7)
											panning.from_s3m_pan(module->base_pan[unmapped_channel_number] - info.low_nybble);
										else
											panning.from_s3m_pan(module->base_pan[unmapped_channel_number] + info.low_nybble);
									}
								break;
							case S3MExtendedEffect::NoteCut: // 0xC
								note_cut_at_frame = true;
								note_cut_frames = info.low_nybble;
								break;
							case S3MExtendedEffect::NoteDelay: // 0xD
								if (info.low_nybble <= static_cast<unsigned int>(module->speed))
								{
									note_delay = true;
									note_delay_frames = info.low_nybble;
									delayed_note = &row;

									note_cut();
								}
								break;
							default:
								cerr << "Unimplemented S3M/IT command: " << row.effect.command
									<< setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
						}
						break;
					case Effect::FineVibrato: // 'U'
						if ((info.low_nybble == 0) || (info.high_nybble == 0))
						{
							if (!vibrato_retrig)
							{
								vibrato_cycle_offset += 1.0;
								LINT_DOUBLE(vibrato_cycle_offset);
							}
						}
						else
						{
							if (p_vibrato) // already vibratoing
							{
								vibrato_cycle_offset += 1.0;
								LINT_DOUBLE(vibrato_cycle_offset);
							}
							else
								vibrato_cycle_offset = 0.0;

							if (it_linear_slides)
								vibrato_depth = info.low_nybble * (255.0 / 128.0) / -768.0;
							else
								vibrato_depth = info.low_nybble * (255.0 / 128.0);

							if (it_new_effects)
								vibrato_depth *= 0.5;

							double new_vibrato_cycle_frequency = (module->speed * info.high_nybble) / 64.0;

							if (p_vibrato && (new_vibrato_cycle_frequency != vibrato_cycle_frequency))
							{
								vibrato_cycle_offset *= (vibrato_cycle_frequency / new_vibrato_cycle_frequency);
								LINT_DOUBLE(vibrato_cycle_offset);
							}

							vibrato_cycle_frequency = new_vibrato_cycle_frequency;
							LINT_DOUBLE(vibrato_cycle_frequency);
						}
						vibrato = true;
						break;
					case Effect::Panbrello: // 'Y', high = speed, low - depth
						if (it_effects)
						{
							if ((info.low_nybble == 0) || (info.high_nybble == 0))
							{
								if (!panbrello_retrig)
									panbrello_cycle_offset += 1.0;
							}
							else
							{
								if (p_panbrello) // already panbrelloing
									panbrello_cycle_offset += 1.0;
								else
									panbrello_cycle_offset = 0.0;

								if (it_linear_slides)
									panbrello_depth = info.low_nybble * (255.0 / 128.0) / -192.0;
								else
									panbrello_depth = info.low_nybble * 4.0 * (255.0 / 128.0);

								if (it_effects)
									panbrello_depth *= 0.5;

								double new_panbrello_cycle_frequency = (module->speed * info.high_nybble) / 64.0;

								if (p_panbrello && (new_panbrello_cycle_frequency != panbrello_cycle_frequency))
									panbrello_cycle_offset *= (panbrello_cycle_frequency / new_panbrello_cycle_frequency);

								panbrello_cycle_frequency = new_panbrello_cycle_frequency;
							}
							panbrello = true;
						}
						else
							cerr << "Ignoring pre-IT command: " << row.effect.command
								<< setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
						break;

					default:
						cerr << "Unimplemented S3M/IT command: " << row.effect.command
							<< setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
				}
			}
		}

		ticks_left = module->ticks_per_module_row;
		dropoff_start = 0;
		rest_ticks = 0;
		cutoff_ticks = 0;

		return ChannelPlaybackState::Ongoing;
	}
}