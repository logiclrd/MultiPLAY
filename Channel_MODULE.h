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

  virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true)
  {
    this->channel::note_off(false, all_notes_off);
    if (((fading && !have_fade_per_tick) || calc_fade_per_tick) && (current_sample != NULL))
      fade_per_tick = (current_sample->fade_out / 1024.0) / module->ticks_per_frame;
  }

  virtual bool advance_pattern(one_sample &sample)
  {
    if (volume_slide)
    {
      double t = double(ticks_left) / module->ticks_per_module_row;
      double new_intensity = previous_intensity * t + target_intensity * (1.0 - t);
      if (new_intensity < 0)
        new_intensity = 0;
      else if (new_intensity > original_intensity)
        new_intensity = original_intensity;

      if (tremolo)
        tremolo_middle_intensity *= (new_intensity / intensity);

      intensity = new_intensity;
    }
    if (panning_slide)
    {
      double t = double(ticks_left) / module->ticks_per_module_row;
      double new_panning = panning_slide_start * t + panning_slide_end * (1.0 - t);
      if (new_panning < -1.0)
        new_panning = -1.0;
      else if (new_panning > 1.0)
        new_panning = 1.0;

      if (panbrello)
        panbrello_middle = new_panning;

      panning.from_linear_pan(new_panning, -1.0, +1.0);
    }
    if (channel_volume_slide)
    {
      double t = double(ticks_left) / module->ticks_per_module_row;
      double new_channel_volume = channel_volume_slide_start * t + channel_volume_slide_end * (1.0 - t);
      if (new_channel_volume < 0.0)
        new_channel_volume = 0.0;
      else if (new_channel_volume > 1.0)
        new_channel_volume = 1.0;

      channel_volume = new_channel_volume;
    }
    if (global_volume_slide)
    {
      double t = double(ticks_left) / module->ticks_per_module_row;
      double new_global_volume = global_volume_slide_start * t + global_volume_slide_end * (1.0 - t);
      if (new_global_volume < 0.0)
        new_global_volume = 0.0;
      else if (new_global_volume > 1.0)
        new_global_volume = 1.0;

      global_volume = new_global_volume;
    }
    if (portamento)
    {
      double t = double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;

      if (!smooth_portamento_effect)
        t = floor(t * module->speed) / module->speed;

      if (t > portamento_end_t)
        t = portamento_end_t;

      if (it_linear_slides)
      {
        double exponent = portamento_start * (1.0 - t) + portamento_end * t;
        note_frequency = p2(exponent);
      }
      else
      {
        double period = portamento_start * (1.0 - t) + portamento_end * t;
        note_frequency = 14317056.0 / period;
      }

      if (portamento_glissando)
      {
        double exponent = lg(note_frequency * 1.16363636363636363);
        long note = (long)exponent * 12;
        exponent = note / 12.0;
        note_frequency = p2(exponent) / 1.16363636363636363;
      }

      delta_offset_per_tick = note_frequency / ticks_per_second;
    }
    if (vibrato)
    {
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

        double note_frequency;
        
        if (it_linear_slides)
          note_frequency = p2(value);
        else
          note_frequency = 14317056.0 / value;

        delta_offset_per_tick = note_frequency / ticks_per_second;
      }
    }
    if (panbrello)
    {
      double t = double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;

      double t_panbrello = (panbrello_cycle_offset + t) * panbrello_cycle_frequency;
      double t_offset = t_panbrello - floor(t_panbrello);
      double value = panbrello_middle;
      
      switch (panbrello_waveform)
      {
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
    }
    if (tremor)
    {
      double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
      int frame_mod = (int(floor(frame)) + tremor_frame_offset) % (tremor_ontime + tremor_offtime);
      if (frame_mod >= tremor_ontime)
      {
        sample = 0.0;
        return true;
      }
    }
    if (arpeggio)
    {
      double fiftieths_of_second = 50.0 * double(module->ticks_per_module_row - ticks_left + arpeggio_tick_offset) / ticks_per_second;
      switch (int(floor(fiftieths_of_second)) % 3)
      {
        case 0: delta_offset_per_tick = arpeggio_first_delta_offset;  break;
        case 1: delta_offset_per_tick = arpeggio_second_delta_offset; break;
        case 2: delta_offset_per_tick = arpeggio_third_delta_offset;  break;
      }
    }
    if (note_delay)
    {
      double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
      if (frame >= note_delay_frames)
      {
        note_delay = false;

        if (delayed_note->volume >= 0)
        {
          intensity = original_intensity * delayed_note->volume / 64.0;
          volume = delayed_note->volume;
        }

        if (delayed_note->snote != -1)
        {
          if (delayed_note->snote == -2)
          {
            current_sample = NULL;
            current_sample_context = NULL;
          }
          else if (delayed_note->snote == -3)
            note_off();
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
                  current_sample->begin_new_note(delayed_note, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
                  recalc(translated_znote, 1.0, false);
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
      if (retrigger_ticks && (((module->ticks_per_module_row - ticks_left) % retrigger_ticks) == 0))
      {
        offset = 0.0;
        offset_major = 0;
        int ignored = 0;
        current_sample->begin_new_note(&module->pattern_list[module->current_pattern]->row_list[module->current_row][unmapped_channel_number], this, &current_sample_context, module->ticks_per_frame, true, &ignored);
        intensity = intensity * retrigger_factor + retrigger_bias;
        if (intensity > original_intensity)
          intensity = original_intensity;
        if (intensity < 0.0)
          intensity = 0.0;
        volume = int(intensity * 64.0 / original_intensity);
      }
    }
    if (tremolo)
    {
      double t = double(ticks_left) / module->ticks_per_module_row;

      if (t > tremolo_start_t)
      {
        t -= tremolo_start_t;
        double t_tremolo = (tremolo_cycle_offset + t) * tremolo_cycle_frequency;
        double t_offset = t_tremolo - floor(t_tremolo);
        intensity = tremolo_middle_intensity;

        switch (tremolo_waveform)
        {
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
    }
    if (note_cut)
    {
      double frame = module->speed * double(module->ticks_per_module_row - ticks_left) / module->ticks_per_module_row;
      if (frame >= note_cut_frames)
      {
        volume = 0;
        intensity = 0.0;
      }
    }
    if (tempo_slide)
    {
      int ticks_in = module->ticks_per_module_row - ticks_left;
      double frame = (module->speed - 1) * double(ticks_in) / module->ticks_per_module_row;
      tempo = (int)(original_tempo + frame * tempo_change_per_frame);
    }

    if (ticks_left)
      return false;

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

    p_volume_slide = volume_slide;                       volume_slide = false;
    p_portamento = p_portamento;                         portamento = false;
    p_vibrato = vibrato;                                 vibrato = false;
    p_tremor = tremor;                                   tremor = false;
    p_arpeggio = arpeggio;                               arpeggio = false;
    p_note_delay = note_delay;                           note_delay = false;
    p_retrigger = retrigger;                             retrigger = false;
    p_tremolo = tremolo;                                 tremolo = false;
    p_note_cut = note_cut;                               note_cut = false;
    p_panning_slide = panning_slide;                     panning_slide = false;
    p_channel_volume_slide = channel_volume_slide;       channel_volume_slide = false;
    p_panbrello = panbrello;                             panbrello = false;
    p_tempo_slide = tempo_slide;                         tempo_slide = false;
    p_global_volume_slide = global_volume_slide;         global_volume_slide = false;
    p_pattern_delay_by_frames = pattern_delay_by_frames; pattern_delay_by_frames = false;

    ticks_left = module->ticks_per_module_row;

    if (pattern_jump_target >= 0)
    {
      if (pattern_jump_target >= int(module->pattern_list.size()))
      {
        if (module->auto_loop_target >= 0)
        {
          module->current_pattern = module->auto_loop_target;
          module->current_row = -1;
          pattern_jump_target = -1;
          row_jump_target = 0;
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
        module->current_pattern = pattern_jump_target;
        module->current_row = row_jump_target - 1;
        pattern_jump_target = -1;
        row_jump_target = 0;
      }
    }

    if (pattern_delay)
    {
      pattern_delay--;
      ticks_left = module->ticks_per_module_row;
      dropoff_start = 0;
      rest_ticks = 0;
      cutoff_ticks = 0;
      return false;
    }
    
    if (channel_number == 0)
    {
      if (p_pattern_delay_by_frames)
        module->speed -= pattern_delay_frames;

      module->current_row++;
      if (module->current_row == module->pattern_list[module->current_pattern]->row_list.size())
      {
        module->current_row = 0;
        module->current_pattern++;

        if (module->current_pattern >= int(module->pattern_list.size()))
        {
          if (looping)
            module->current_pattern = 0;
          else
            module->finished = true;
        }
      }

      if (!trace_mod)
        cerr << "starting " << module->current_pattern << ":" <<
          (module->current_row / 10) << (module->current_row % 10) << "   " << string(79, (char)8);
    }

    if (module->finished)
    {
      finished = true;
      return true;
    }

    vector<row> &row_list = module->pattern_list[module->current_pattern]->row_list[module->current_row];

    if (channel_number == 0)
    {
      bool recalc = false;

      for (unsigned int i=0; i<MAX_MODULE_CHANNELS; i++)
      {
        if (i >= row_list.size())
          break;

        if (row_list[i].effect.present == false)
          continue;

        switch (row_list[i].effect.command)
        {
          case 'A': // speed
            if (row_list[i].effect.info.data)
              module->speed = row_list[i].effect.info.data;
            recalc = true;
            break;
          case 'B': // order jump
            pattern_jump_target = row_list[i].effect.info.data;
            break;
          case 'C': // pattern/row jump
            pattern_jump_target = module->current_pattern + 1;
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
          case 'S': // extended commands
            switch ((row_list[i].effect.info >> 4) & 15)
            {
              case 0x6: // pattern delay in ticks
                pattern_delay_by_frames = true;
                pattern_delay_frames = row_list[i].effect.info.low_nybble;
                module->speed += pattern_delay_frames;
                break;
              case 0xB: // pattern loop
                if (row_list[i].effect.info.low_nybble == 0)
                {
                  module->pattern_loop[i].row = module->current_row;
                  module->pattern_loop[i].need_repetitions = true;
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
                    module->current_row = module->pattern_loop[i].row - 1;
                    module->pattern_loop[i].repetitions--;
                  }
                }
                break;
            }
            break;
          case 'T': // tempo
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
          case 'V': // global volume
            if (row_list[i].effect.info > 64)
              global_volume = 1.0;
            else
              global_volume = row_list[i].effect.info / 64.0;
            break;
          case 'W': // slide global volume
            if (it_effects)
            {
              effect_info_type info = row_list[i].effect.info;
              bool fine;

              if (info.data == 0) // repeat
                info = last_param['W'];
              else
                last_param['W'] = info;

              global_volume_slide_start = global_volume;

              if (info.low_nybble == 0) // slide up
                global_volume_slide_end = global_volume_slide_start + (module->speed - 1) * info.high_nybble / 32.0;
              else if (info.high_nybble == 0) // slide down
                global_volume_slide_end = global_volume_slide_start - (module->speed - 1) * info.low_nybble / 32.0;
              else if (info.low_nybble == 0xF) // fine slide up
              {
                global_volume = global_volume_slide_start - info.high_nybble / 64.0;
                fine = true;
              }
              else if (info.high_nybble == 0xF) // fine slide down
              {
                global_volume = global_volume_slide_start + info.high_nybble / 64.0;
                fine = true;
              }

              if (!fine)
              {
                if (global_volume_slide_end == global_volume_slide_start)
                  break;

                global_volume_slide = true;
              }
            }
            else
              cerr << "Ignoring pre-IT command: W"
                << setfill('0') << setw(2) << hex << uppercase << int(row_list[i].effect.info.data) << nouppercase << dec << endl;
        }                      
      }
      
      if (recalc)
        module->speed_change();
    }

    row &row = row_list[channel_number];

    if (trace_mod)
    {
      if (channel_number == 0)
      {
        cerr << endl;
        if (module->current_row == 0)
          cerr << string(9 + 18 * module->num_channels, '-') << endl;

        cerr << setfill(' ') << setw(3) << module->current_pattern << ":"
             << setfill('0') << setw(req_digits(module->pattern_list[module->current_pattern]->row_list.size())) << module->current_row << " | ";
      }                                      // .- escaped for trigraph avoidance
      cerr << string("C-C#D-D#E-F-F#G-G#A-A#B-?\?==^^--").substr((row.snote & 15) * 2, 2)
           << char((row.snote >= 0) ? (48 + (row.snote >> 4)) : '-') << " ";
      if (row.instrument == NULL)
        cerr << "-- ";
      else
        cerr << setfill('0') << setw(2) << row.instrument->index << " ";
      if (row.volume >= 0)
      {
        if (row.volume > 64)
          cerr << "vXX";
        else
          cerr << "v" << setfill('0') << setw(2) << row.volume;
        if (row.secondary_effect.present)
          cerr << "*";
        else
          cerr << " ";
      }
      else if (row.secondary_effect.present)
      {
        cerr << char(row.secondary_effect.command)
          << hex << uppercase << setfill('0') << setw(2) << int(row.secondary_effect.info.data) << nouppercase << dec
          << " ";
      }
      else
        cerr << "--- ";
      if (row.effect.present)
        cerr << char(row.effect.command)
          << hex << uppercase << setfill('0') << setw(2) << int(row.effect.info.data) << nouppercase << dec;
      else
        cerr << "---";
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
      if (row.snote == -2)
      {
        if (anticlick && current_sample)
        {
          if (volume_envelope != NULL)
          {
            delete volume_envelope;
            volume_envelope = NULL;
          }
          note_off(true, false);
          channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);
          
          ancillary->panning_envelope = panning_envelope;
          ancillary->pitch_envelope = pitch_envelope;
          ancillary->fading = true;
          ancillary->fade_value = 1.0;

          panning_envelope = NULL;
          pitch_envelope = NULL;

          ancillary_channels.push_back(ancillary);
        }
        current_sample = NULL;
        if (current_sample_context != NULL)
        {
          delete current_sample_context;
          current_sample_context = NULL;
        }
      }
      else if (row.snote == -3)
        note_off();
      else
      {
        if ((row.instrument != NULL) || module->it_module)
        {
          if (!row.effect.keepNote())
          {
            if (anticlick && current_sample)
            {
              if (volume_envelope != NULL)
              {
                delete volume_envelope;
                volume_envelope = NULL;
              }
              note_off(true, false);
              channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);
              
              ancillary->panning_envelope = panning_envelope;
              ancillary->pitch_envelope = pitch_envelope;
              ancillary->fading = true;
              ancillary->fade_value = 1.0;

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
        if (volume_envelope != NULL)
        {
          delete volume_envelope;
          volume_envelope = NULL;
        }
        note_off(true, false);
        channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);
        
        ancillary->panning_envelope = panning_envelope;
        ancillary->pitch_envelope = pitch_envelope;
        ancillary->fading = true;
        ancillary->fade_value = 1.0;

        panning_envelope = NULL;
        pitch_envelope = NULL;

        ancillary_channels.push_back(ancillary);
      }

      if (!row.effect.keepNote())
      {
        current_sample = row.instrument;
        int translated_znote = current_znote;
        current_sample->begin_new_note(&row, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
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

    int second_note, third_note, target_offset;
    double portamento_target, before_finetune;

    bool fine = false;

    if (p_vibrato && row.effect.isnt('H'))
      delta_offset_per_tick = note_frequency / ticks_per_second;

    if (row.effect.present)
    {
      switch (row.effect.command)
      {
        case 0xE: // MOD extra effects
          if (info.high_nybble == 0xF) // invert loop
          {
            // TODO
            // delta_offset_per_tick = -info.low_nybble;
          }
          break;
        case 'A': // S3M set speed
        case 'B': // S3M order jump
        case 'C': // S3M pattern/row jump (jumps into next pattern, specified row
        case 'T': // tempo
        case 'V': // global volume
          break;
        case 'D': // S3M volume slide
          if (info.data == 0) // repeat
            info = last_param['D'];
          else
            last_param['D'] = info;

          if (info.low_nybble == 0) // slide up
            target_volume = volume + (module->speed - 1) * info.high_nybble;
          else if (info.high_nybble == 0) // slide down
            target_volume = volume - (module->speed - 1) * info.low_nybble;
          else if (info.low_nybble == 0xF) // fine slide up
          {
            volume += info.high_nybble;
            if (volume > 64)
              volume = 64;
            fine = true;
          }
          else if (info.high_nybble == 0xF) // fine slide down
          {
            volume -= info.low_nybble;
            if (volume < 0)
              volume = 0;
            fine = true;
          }

          if (fine)
            intensity = original_intensity * (volume / 64.0);
          else
          {
            if (target_volume < 0)
              target_volume = 0;
            if (target_volume > 64)
              target_volume = 64;

            if (target_volume == volume)
              break;

            target_intensity = original_intensity * (target_volume / 64.0);
            previous_intensity = intensity;
            volume_slide = true;
          }
          break;
        case 'E': // S3M portamento down
          if (it_linear_slides)
            portamento_start = lg(note_frequency);
          else
            portamento_start = 14317056.0 / note_frequency;

          if (info.data == 0)
            info = last_param['E'];
          else
            last_param['E'] = last_param['F'] = info;

          if (info.high_nybble == 0xF) // 'fine'
          {
            if (it_linear_slides)
              portamento_target = portamento_start - info.low_nybble / 192.0;
            else
              portamento_target = portamento_start + 4 * info.low_nybble;
            fine = true;
          }
          else if (info.high_nybble == 0xE) // 'extra fine'
          {
            if (it_linear_slides)
              portamento_target = portamento_start - info.low_nybble / 768.0;
            else
              portamento_target = portamento_start + info.low_nybble;
            fine = true;
          }

          if (fine)
          {
            if (it_linear_slides)
              note_frequency = p2(portamento_target);
            else
              note_frequency = 14317056.0 / portamento_target;

            delta_offset_per_tick = note_frequency / ticks_per_second;
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
        case 'F': // S3M portamento up
          if (it_linear_slides)
            portamento_start = lg(note_frequency);
          else
            portamento_start = 14317056.0 / note_frequency;

          if (info.data == 0)
            info = last_param['F'];
          else
            last_param['E'] = last_param['F'] = info;

          if (info.high_nybble == 0xF) // 'fine'
          {
            if (it_linear_slides)
              portamento_target = portamento_start + info.low_nybble / 192.0;
            else
              portamento_target = portamento_start - 4 * info.low_nybble;
            fine = true;
          }
          else if (info.high_nybble == 0xE) // 'extra fine'
          {
            if (it_linear_slides)
              portamento_target = portamento_start + info.low_nybble / 768.0;
            else
              portamento_target = portamento_start - info.low_nybble;
            fine = true;
          }

          if (fine)
          {
            if (it_linear_slides)
              note_frequency = p2(portamento_target);
            else
              note_frequency = 14317056.0 / portamento_target;
            delta_offset_per_tick = note_frequency / ticks_per_second;
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
        case 'G': // S3M tone portamento
          if (info.data != 0)
          {
            portamento_speed = info.data * 4;
            if (it_portamento_link)
              last_param['E'] = last_param['F'] = info;
            else
              last_param['G'] = info;
          }
          else if (it_portamento_link)
            portamento_speed = last_param['E'].data * 4;
          else
            portamento_speed = last_param['G'].data * 4;

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
          break;
        case 'H': // S3M vibrato, high = speed, low = depth
          if ((info.low_nybble == 0) || (info.high_nybble == 0))
          {
            if (!vibrato_retrig)
              vibrato_cycle_offset += (1.0 - vibrato_start_t);
          }
          else
          {
            if (p_vibrato) // already vibratoing
              vibrato_cycle_offset += (1.0 - vibrato_start_t);
            else
              vibrato_cycle_offset = 0.0;

            if (it_linear_slides)
              vibrato_depth = info.low_nybble * (255.0 / 128.0) / -192.0;
            else
              vibrato_depth = info.low_nybble * 4.0 * (255.0 / 128.0);

            if (it_new_effects)
              vibrato_depth *= 0.5;

            double new_vibrato_cycle_frequency = (module->speed * info.high_nybble) / 64.0;

            if (p_vibrato && (new_vibrato_cycle_frequency != vibrato_cycle_frequency))
              vibrato_cycle_offset *= (vibrato_cycle_frequency / new_vibrato_cycle_frequency);

            vibrato_cycle_frequency = new_vibrato_cycle_frequency;

            if (it_new_effects)
              vibrato_start_t = 0.0;
            else
              vibrato_start_t = 1.0 / module->speed;
          }
          vibrato = true;
          break;
        case 'I': // tremor, high = ontime, low = offtime
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
        case 'J': // S3M arpeggio, high = first addition, low = second addition
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
        case 'K': // S3M H00 and Dxy
          if (!vibrato_retrig)
            vibrato_cycle_offset += (1.0 - vibrato_start_t);
          vibrato = true;

          if (info.data == 0) // repeat
            info = last_param['D'];
          else
            last_param['D'] = info;

          if (info.low_nybble == 0) // slide up
            target_volume = volume + (module->speed - 1) * info.high_nybble;
          else if (info.high_nybble == 0) // slide down
            target_volume = volume - (module->speed - 1) * info.low_nybble;
          else
          {
            cerr << "Ambiguous/undefined S3M effect: K"
              << setfill('0') << setw(2) << hex << uppercase << info.data << nouppercase << dec << endl;
            break;
          }

          if (target_volume < 0)
            target_volume = 0;
          if (target_volume > 64)
            target_volume = 64;
          target_intensity = original_intensity * (target_volume / 64.0);
          previous_intensity = intensity;
          volume_slide = true;
          break;
        case 'L': // S3M G00 and Dxy
          if (row.snote >= 0)
            portamento_target_znote = row.znote;
          
          old_frequency = note_frequency;
          old_delta_offset_per_tick = delta_offset_per_tick;
          old_current_znote = current_znote;

          temp_sc = current_sample_context;
          current_sample_context = NULL; // protect the existing context

          current_sample->begin_new_note(&row, NULL, &current_sample_context, module->ticks_per_frame, true, &portamento_target_znote);
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

          if (info.data == 0) // repeat
            info = last_param['D'];
          else
            last_param['D'] = info;

          if (info.low_nybble == 0) // slide up
            target_volume = volume + (module->speed - 1) * info.high_nybble;
          else if (info.high_nybble == 0) // slide down
            target_volume = volume - (module->speed - 1) * info.low_nybble;
          else
          {
            cerr << "Ambiguous/undefined S3M effect: L"
              << setfill('0') << setw(2) << hex << uppercase << info.data << nouppercase << dec << endl;
            break;
          }

          if (target_volume < 0)
            target_volume = 0;
          if (target_volume > 64)
            target_volume = 64;
          target_intensity = original_intensity * (target_volume / 64.0);
          previous_intensity = intensity;
          volume_slide = true;
          break;
        case 'M': // set channel volume
          if (it_effects)
          {
            if (info.data > 64)
              channel_volume = 1.0;
            else
              channel_volume = info.data / 64.0;
          }
          else
            cerr << "Ignoring pre-IT command: M"
              << setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
          break;
        case 'N': // channel volume slide
          if (it_effects)
          {
            if (info.data == 0) // repeat
              info = last_param['N'];
            else
              last_param['N'] = info;

            channel_volume_slide_start = channel_volume;

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
        case 'O': // sample offset
          if (info.data == 0)
            info = last_param['O'];
          else
            last_param['O'] = info;

          target_offset = (info.data << 8) + set_offset_high;

          if (it_new_effects)
          {
            if (current_sample_context && (target_offset > current_sample_context->num_samples))
              break;
          }

          offset_major = target_offset;
          break;
        case 'P': // pan slide
          if (it_effects)
          {
            if (info.data == 0) // repeat
              info = last_param['P'];
            else
              last_param['P'] = info;

            panning_slide_start = panning.to_linear_pan(-1.0, +1.0);

            if (info.low_nybble == 0) // pan left
              panning_slide_end = panning_slide_start - (module->speed - 1) * info.high_nybble / 32.0;
            else if (info.high_nybble == 0) // pan right
              panning_slide_end = panning_slide_start + (module->speed - 1) * info.low_nybble / 32.0;
            else if (info.low_nybble == 0xF) // fine pan left
            {
              panning.from_linear_pan(panning_slide_start - info.high_nybble / 32.0, -1.0, +1.0);
              fine = true;
            }
            else if (info.high_nybble == 0xF) // fine pan right
            {
              panning.from_linear_pan(panning_slide_start + info.high_nybble / 32.0, -1.0, +1.0);
              fine = true;
            }

            if (!fine)
            {
              if (panning_slide_end == panning_slide_start)
                break;

              panning_slide = true;
            }
          }
          else
            cerr << "Ignoring pre-IT command: " << row.effect.command
              << setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
          break;
        case 'Q': // retrigger
          if (info.data == 0)
            info = last_param['Q'];
          else
            last_param['Q'] = info;

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
        case 'R': // S3M tremolo
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
              vibrato_start_t = 1.0 / module->speed;
          }
          tremolo = true;
          break;
        case 'S': // S3M extended commands
          switch (info.high_nybble)
          {
            case 0x6: // pattern delay in frames    ignore song-wide commands
            case 0xB: // pattern loop
              break;
            case 0x1: // set glissando
              if (info.low_nybble == 0)
                portamento_glissando = false;
              else if (info.low_nybble == 1)
                portamento_glissando = true;
              else
                cerr << "Invalid parameter value: S3M command S1"
                  << hex << uppercase << info.low_nybble << nouppercase << dec << ")" << endl;
              break;
            case 0x2: // set MOD finetune
              if (current_sample == NULL)
                cerr << "No instrument for set finetune effect" << endl;
              else
              {
                before_finetune = current_sample->samples_per_second;

                current_sample->samples_per_second = mod_finetune[info.low_nybble];

                note_frequency *= (current_sample->samples_per_second / before_finetune);
                delta_offset_per_tick = note_frequency / ticks_per_second;
              }
              break;
            case 0x3: // set vibrato waveform
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
            case 0x4: // set tremolo waveform
              switch (info.low_nybble & 0x3)
              {
                case 0x0: tremolo_waveform = Waveform::Sine;     break;
                case 0x1: tremolo_waveform = Waveform::RampDown; break;
                case 0x2: tremolo_waveform = Waveform::Square;   break;
                case 0x3: tremolo_waveform = Waveform::Random;   break;
              }
              if (info.low_nybble & 0x4)
                tremolo_retrig = false;
              else
                tremolo_retrig = true;

              if (info.low_nybble & 0x8)
                cerr << "Warning: bit 3 ignored in effect S4"
                  << hex << uppercase << info.low_nybble << nouppercase << dec << endl;
              break;
            case 0x5: // set panbrello waveform
              if (it_effects)
              {
                switch (info.low_nybble & 0x3)
                {
                  case 0x0: panbrello_waveform = Waveform::Sine;     break;
                  case 0x1: panbrello_waveform = Waveform::RampDown; break;
                  case 0x2: panbrello_waveform = Waveform::Square;   break;
                  case 0x3: panbrello_waveform = Waveform::Random;   break;
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
            case 0x7: // past note cut/off/fade, temporary new note action
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
                    for (int i = ancillary_channels.size() - 1; i >= 0; i--)
                    {
                      iter_t my_own = find(my_ancillary_channels.begin(), my_ancillary_channels.end(), ancillary_channels[i]);

                      if (my_own != my_ancillary_channels.end())
                      {
                        ancillary_channels.erase(ancillary_channels.begin() + i);
                        my_ancillary_channels.erase(my_own);
                        delete *my_own;
                      }
                    }
                    break;
                  case 1: // note off
                    for (iter_t i = my_ancillary_channels.begin(), l = my_ancillary_channels.end();
                         i != l;
                         ++i)
                      (*i)->note_off();
                    break;
                  case 2: // fade
                    for (iter_t i = my_ancillary_channels.begin(), l = my_ancillary_channels.end();
                         i != l;
                         ++i)
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
            case 0x8: // set pan position
              if (module->stereo)
                panning.from_s3m_pan(module->base_pan[unmapped_channel_number] + info.low_nybble - 8);
              break;
            case 0x9: // extended IT effects
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
            case 0xA: // old ST panning, IT set high offset
              if (it_effects)
                set_offset_high = info.low_nybble << 16;
              else
                if (module->stereo)
                {
                  if (info.low_nybble > 7)
                    panning.from_s3m_pan(module->base_pan[unmapped_channel_number] - info.low_nybble);
                  else
                    panning.from_s3m_pan(module->base_pan[unmapped_channel_number] + info.low_nybble);
                }
              break;
            case 0xC: // note cut
              note_cut = true;
              note_cut_frames = info.low_nybble;
              break;
            case 0xD: // note delay
              if (info.low_nybble <= static_cast<unsigned int>(module->speed))
              {
                note_delay = true;
                note_delay_frames = info.low_nybble;
                delayed_note = &row;
                current_sample = NULL;
                current_sample_context = NULL;
              }
              break;
            case 0xE: // pattern delay
              pattern_delay = info.low_nybble;
              break;
            default:
              cerr << "Unimplemented S3M/IT command: " << row.effect.command
                << setfill('0') << setw(2) << hex << uppercase << int(info.data) << nouppercase << dec << endl;
          }
          break;
        case 'U': // fine vibrato
          if ((info.low_nybble == 0) || (info.high_nybble == 0))
          {
            if (!vibrato_retrig)
              vibrato_cycle_offset += 1.0;
          }
          else
          {
            if (p_vibrato) // already vibratoing
              vibrato_cycle_offset += 1.0;
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
              vibrato_cycle_offset *= (vibrato_cycle_frequency / new_vibrato_cycle_frequency);

            vibrato_cycle_frequency = new_vibrato_cycle_frequency;
          }
          vibrato = true;
          break;
        case 'X': // amiga panning
          if (module->stereo)
          {
            if (it_effects)
              panning.from_mod_pan(info.data);
            else
              panning.from_amiga_pan(info.data);
          }
          break;
        case 'Y': // panbrello, high = speed, low - depth
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

    ticks_left = module->ticks_per_module_row;
    dropoff_start = 0;
    rest_ticks = 0;
    cutoff_ticks = 0;

    return false;
  }

  channel_MODULE(int channel_number, module_struct *module, int channel_volume, bool looping)
    : channel(module->initial_panning[channel_number], looping),
      unmapped_channel_number(channel_number),
      channel_number(module->channel_map[channel_number]),
      module(module)
  {
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
    tremor = false;
    arpeggio = false;
    note_delay = false;
    retrigger = false;
    tremolo = false;
    note_cut = false;
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
    it_new_effects = module->it_module_new_effects;
    it_portamento_link = module->it_module_portamento_link;
    it_linear_slides = module->it_module_linear_slides;
    set_offset_high = 0;
  }

  virtual ~channel_MODULE()
  {
  }
};
