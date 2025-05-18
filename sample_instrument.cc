#include "sample_instrument.h"

#include <cstring>

using namespace std;

#include "DSP.h"

#include "Channel_DYNAMIC.h"

#include "sample.h"
#include "notes.h"
#include "pattern.h"

namespace MultiPLAY
{
	// struct sample_instrument_context
	sample_instrument_context::sample_instrument_context(sample *cw) : sample_context(cw) {}

	/*virtual*/ sample_instrument_context::~sample_instrument_context()
	{
		sample_context::~sample_context();
		if (cur_sample_context != NULL)
			delete cur_sample_context;
	}

	/*virtual*/ sample_context *sample_instrument_context::create_new()
	{
		return new sample_instrument_context(NULL);
	}

	/*virtual*/ void sample_instrument_context::copy_to(sample_context *other)
	{
		sample_instrument_context *target = dynamic_cast<sample_instrument_context *>(other);

		if (target == NULL)
			throw "Copy sample context to wrong type";

		sample_context::copy_to(target);

		target->cur_sample = this->cur_sample;;
		if (this->cur_sample_context != NULL)
			target->cur_sample_context = this->cur_sample_context->clone();
		else
			target->cur_sample_context = NULL;
		target->cur_volume = this->cur_volume;
		target->sustain_loop_state = this->sustain_loop_state;
		target->owner_channel = this->owner_channel;
		target->effect_tick_length = this->effect_tick_length;
		target->inote = this->inote;
	}

	// struct sample_instrument
	sample_instrument::sample_instrument(int idx)
		: sample(idx)
	{
		use_dsp = false;

		memset(&note_sample[0], 0, sizeof(note_sample));
		memset(&tone_offset[0], 0, sizeof(tone_offset));
	}

	/*virtual*/ void sample_instrument::occlude_note(
		channel *p/* = NULL*/,
		sample_context **context/* = NULL*/,
		sample *new_sample/* = NULL*/,
		row *r/* = NULL*/)
	{
		NewNoteAction::Type effective_nna = new_note_action;

		if (r->effect.present && (r->effect.is(Effect::S3MExtendedEffect, 7)))
		{
			switch (r->effect.info.low_nybble)
			{
				case 3: effective_nna = NewNoteAction::Cut;          break;
				case 4: effective_nna = NewNoteAction::ContinueNote; break;
				case 5: effective_nna = NewNoteAction::NoteOff;      break;
				case 6: effective_nna = NewNoteAction::NoteFade;     break;
			}
		}

		sample_instrument_context &c = *(sample_instrument_context *)*context;

		if ((*context != NULL) && (p != NULL))
		{
			bool is_duplicate = false;
			int new_inote = (r->snote >> 4) * 12 + (r->snote & 15);

			if (this == new_sample)
			{
				switch (duplicate_note_check)
				{
					case DuplicateCheck::Off:
						// Do nothing
						break;
					case DuplicateCheck::Note:
						is_duplicate = (c.inote == new_inote);
						break;
					case DuplicateCheck::Sample:
						is_duplicate = (c.cur_sample == note_sample[new_inote]);
						break;
					case DuplicateCheck::Instrument:
						is_duplicate = true;
						break;
				}
			}

			if (is_duplicate)
			{
				switch (duplicate_check_action)
				{
					case DuplicateCheckAction::Cut:
						return; // let the note get cut off
					case DuplicateCheckAction::NoteFade:
						effective_nna = NewNoteAction::NoteFade;
						break;
					case DuplicateCheckAction::NoteOff:
						effective_nna = NewNoteAction::NoteOff;
						break;
				}
			}

			double fade_per_tick;

			fade_per_tick = (c.created_with->fade_out / 1024.0) / c.effect_tick_length;

			channel_DYNAMIC *ancillary;
			
			ancillary = new channel_DYNAMIC(*p, (*context)->created_with, *context, fade_per_tick);

			if ((p->volume_envelope) && (effective_nna != NewNoteAction::Cut))
				ancillary->volume_envelope = new playback_envelope(*p->volume_envelope);
			if (p->panning_envelope)
				ancillary->panning_envelope = new playback_envelope(*p->panning_envelope);
			if (p->pitch_envelope)
				ancillary->pitch_envelope = new playback_envelope(*p->pitch_envelope);

			switch (effective_nna)
			{
				case NewNoteAction::ContinueNote:
					break;
				case NewNoteAction::NoteFade:
					ancillary->fading = true;
					ancillary->fade_value = 1.0;
					break;
				case NewNoteAction::Cut: // Cut has extra handling above (no volume envelope is copied)
				case NewNoteAction::NoteOff:
					ancillary->note_off();
					break;
			}

			ancillary_channels.push_back(ancillary);
			p->add_ancillary_channel(ancillary);
		}
	}

	/*virtual*/ void sample_instrument::begin_new_note(
		row *r/* = NULL*/,
		channel *p/* = NULL*/,
		sample_context **context/* = NULL*/,
		double effect_tick_length/* = 0*/,
		bool top_level/* = true*/,
		int *znote/* = NULL*/)
	{
		if (context == NULL)
			throw "need context for instrument";

		if (r->snote >= 0)
		{
			if (*context)
			{
				(*context)->created_with->occlude_note(p, context, this, r);
				delete *context;
			}

			sample_instrument_context *c = new sample_instrument_context(this);

			*context = c;
			int inote = (r->snote >> 4) * 12 + (r->snote & 15);
			c->inote = inote;
			c->cur_sample = note_sample[inote];
			c->cur_sample_context = NULL;
			c->effect_tick_length = effect_tick_length;
			(*znote) += tone_offset[inote];

			bool volume_envelope_override_on = false;
			bool volume_envelope_override_off = false;

			if (r->effect.is(Effect::S3MExtendedEffect, 7))
			{
				volume_envelope_override_off = (r->effect.info.low_nybble == 7);
				volume_envelope_override_on = (r->effect.info.low_nybble == 8);
			}

			if (c->cur_sample != NULL)
			{
				if (p != NULL)
				{
					if (top_level)
					{
						if (p->volume_envelope != NULL)
						{
							delete p->volume_envelope;
							p->volume_envelope = NULL;
						}
						if (volume_envelope_override_on || (volume_envelope.enabled && (!volume_envelope_override_off)))
						{
							p->volume_envelope = new playback_envelope(volume_envelope, effect_tick_length);
							p->volume_envelope->begin_note();
						}

						if (p->panning_envelope != NULL)
						{
							delete p->panning_envelope;
							p->panning_envelope = NULL;
						}
						if (panning_envelope.enabled)
						{
							p->panning_envelope = new playback_envelope(panning_envelope, effect_tick_length);
							p->panning_envelope->begin_note();
						}

						if (p->pitch_envelope != NULL)
						{
							delete p->pitch_envelope;
							p->pitch_envelope = NULL;
						}
						if (pitch_envelope.enabled)
						{
							p->pitch_envelope = new playback_envelope(pitch_envelope, effect_tick_length);
							p->pitch_envelope->begin_note();
						}
					}
					else
					{
						if (volume_envelope.enabled)
						{
							p->volume_envelope = new playback_envelope(p->volume_envelope, volume_envelope, effect_tick_length, true);
							p->volume_envelope->begin_note(false);
						}

						if (panning_envelope.enabled)
						{
							p->panning_envelope = new playback_envelope(p->panning_envelope, panning_envelope, effect_tick_length, false);
							p->panning_envelope->begin_note(false);
						}

						if (pitch_envelope.enabled)
						{
							p->pitch_envelope = new playback_envelope(p->pitch_envelope, pitch_envelope, effect_tick_length, false);
							p->pitch_envelope->begin_note(false);
						}
					}
				}

				c->cur_sample->begin_new_note(r, p, &c->cur_sample_context, effect_tick_length, false, znote);
				c->samples_per_second = c->cur_sample_context->samples_per_second;
				c->default_volume = c->cur_sample_context->default_volume;
				c->num_samples = c->cur_sample_context->num_samples;
			}
		}

		if (p != NULL)
		{
			p->samples_this_note = 0;
			p->envelope_offset = 0;
		}
	}

	/*virtual*/ void sample_instrument::kill_note(sample_context *c/* = NULL*/)
	{
		if (c == NULL)
			throw "need context for instrument";

		sample_instrument_context &context = *(sample_instrument_context *)c;

		if (context.cur_sample != NULL)
		{
			context.cur_sample->kill_note(context.cur_sample_context);
			context.cur_sample = NULL; // doesn't get much more killed than this
		}
	}

	/*virtual*/ void sample_instrument::exit_sustain_loop(sample_context *c/* = NULL*/)
	{
		if (c == NULL)
			throw "need context for instrument";

		sample_instrument_context &context = *(sample_instrument_context *)c;

		if (context.cur_sample != NULL)
		{
			context.cur_sample->exit_sustain_loop(context.cur_sample_context);

			context.sustain_loop_state = SustainLoopState::Finishing;
		}
	}

	/*virtual*/ bool sample_instrument::past_end(unsigned int sample, double offset, sample_context *c/* = NULL*/)
	{
		if (c == NULL)
			throw "need context for instrument";

		sample_instrument_context &context = *(sample_instrument_context *)c;

		if (context.cur_sample != NULL)
			return context.cur_sample->past_end(sample, offset, c);
		else
			return true;
	}

	/*virtual*/ one_sample sample_instrument::get_sample(unsigned int sample, double offset, sample_context *c/* = NULL*/)
	{
		if (c == NULL)
			throw "need context for instrument";

		sample_instrument_context &context = *(sample_instrument_context *)c;

		one_sample ret(output_channels);

		if (context.cur_sample != NULL)
			ret = global_volume * context.cur_sample->get_sample(sample, offset, context.cur_sample_context);

		if (use_dsp)
			ret = dsp.compute_next(ret);

		return ret;
	}
}
