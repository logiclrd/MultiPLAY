#ifndef SAMPLE_INSTRUMENT_H
#define SAMPLE_INSTRUMENT_H

#include "DSP.h"

#include "Channel_DYNAMIC.h"

#include "sample.h"
#include "notes.h"
#include "pattern.h"

namespace MultiPLAY
{
	struct sample_instrument_context : sample_context
	{
		sample *cur_sample;
		sample_context *cur_sample_context;
		double cur_volume;
		SustainLoopState::Type sustain_loop_state;
		channel *owner_channel;
		double effect_tick_length;
		int inote;

		sample_instrument_context(sample *cw);
		virtual ~sample_instrument_context();

		virtual bool is_sample_match(sample *other);

		virtual sample_context *create_new();
		virtual void copy_to(sample_context *other);
	};

	struct sample_instrument : sample
	{
		pan_value default_pan;
		bool use_default_pan;

		DSP dsp;
		bool use_dsp;

		int pitch_pan_separation, pitch_pan_center;

		double volume_variation_pctg, panning_variation;

		NewNoteAction::Type new_note_action;

		sample *note_sample[120];
		int tone_offset[120];

		instrument_envelope volume_envelope, panning_envelope, pitch_envelope;

		sample_instrument(int idx);

		virtual sample *get_root_sample(int inote);

		virtual const pan_value &get_default_pan(const pan_value &channel_default);
		virtual void begin_new_note(row *r = nullptr, channel *p = nullptr, sample_context **context = nullptr, double effect_tick_length = 0, bool top_level = true, int *znote = nullptr, bool is_primary = true);
		virtual void kill_note(sample_context *c = nullptr);
		virtual void exit_sustain_loop(sample_context *c = nullptr);
		virtual bool past_end(unsigned int sample, double offset, sample_context *c = nullptr);
		virtual one_sample get_sample(unsigned int sample, double offset, sample_context *c = nullptr);
	};
}

#endif // SAMPLE_INSTRUMENT_H
