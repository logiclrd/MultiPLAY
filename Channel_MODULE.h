#ifndef CHANNEL_MODULE_H
#define CHANNEL_MODULE_H

#include "MultiPLAY.h"

#include "channel.h"
#include "module.h"
#include "pattern.h"

namespace MultiPLAY
{
	struct channel_MODULE : public channel
	{
		module_struct *module;
		int channel_number, unmapped_channel_number;
		int pattern_jump_target, row_jump_target;
		double original_intensity, previous_intensity, target_intensity;
		double portamento_start, portamento_end, portamento_end_t;
		int portamento_speed, portamento_target_znote;
		double vibrato_depth, vibrato_cycle_frequency, vibrato_cycle_offset, vibrato_start_t;
		double tremolo_middle_intensity, tremolo_depth, tremolo_cycle_frequency, tremolo_cycle_offset;
		double tremolo_start_t;
		double panbrello_middle, panbrello_depth, panbrello_cycle_frequency, panbrello_cycle_offset;
		double arpeggio_first_delta_offset, arpeggio_second_delta_offset, arpeggio_third_delta_offset;
		row *delayed_note;
		int note_delay_frames, note_cut_frames;
		int arpeggio_tick_offset;
		int tremor_ontime, tremor_offtime, tremor_frame_offset;
		int volume, target_volume;
		double panning_slide_start, panning_slide_end;
		double channel_volume_slide_start, channel_volume_slide_end;
		double global_volume_slide_start, global_volume_slide_end;
		double retrigger_factor, retrigger_bias;
		int retrigger_ticks;
		int original_tempo, tempo_change_per_frame;
		int pattern_delay_frames;
		bool volume_slide, portamento, vibrato, tremor, arpeggio, note_delay, retrigger, tremolo;
		bool note_cut, panning_slide, panbrello, channel_volume_slide, tempo_slide, global_volume_slide;
		bool pattern_delay_by_frames, p_volume_slide, p_portamento, p_vibrato, p_tremor, p_arpeggio;
		bool p_note_delay, p_retrigger, p_tremolo, p_note_cut, p_panning_slide, p_panbrello;
		bool p_channel_volume_slide, p_tempo_slide, p_global_volume_slide, p_pattern_delay_by_frames;
		int pattern_delay;
		double base_note_frequency;
		int set_offset_high;

		bool portamento_glissando;
		Waveform::Type vibrato_waveform, tremolo_waveform, panbrello_waveform;
		bool vibrato_retrig, tremolo_retrig, panbrello_retrig;

		bool it_effects, it_new_effects, it_portamento_link, it_linear_slides;

		effect_info_type last_param[256];

		channel_MODULE(int channel_number, module_struct *module, int channel_volume, bool looping);
		virtual ~channel_MODULE();

		virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true);
		virtual void get_playback_position(PlaybackPosition &position);
		virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile);
	};
}

#endif // CHANNEL_MODULE_H