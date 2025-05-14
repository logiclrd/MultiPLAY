struct channel_PLAY : public channel
{
  istream *in;
  double actual_intensity;
  bool overlap_notes;

  channel_PLAY(istream *input, bool looping, bool overlap = false)
    : channel(looping),
      in(input)
  {
    if (in->get() < 0)
      finished = true;
    
    in->seekg(0, ios::beg);

    actual_intensity = intensity;
    intensity = 0;
    overlap_notes = overlap;
  }

  virtual ~channel_PLAY()
  {
  }
  
  virtual bool advance_pattern(one_sample &sample)
  {
    intensity = overlap_notes ? 0 : actual_intensity;

    while (!ticks_left)
    {
      int ch = in->get();

      if (ch < 0)
      {
        if (looping)
        {
          in->seekg(0, ios::beg);
          ch = in->get();
        }
        else
        {
          finished = true;
          sample = 0.0;
          return false;
        }
      }

      while (ch == '\'')
      {
        while (true)
        {
          ch = in->get();

          if (ch < 0)
          {
            finished = true;
            sample = 0.0;
            return true;
          }

          if (ch == '\n')
            break;
        }

        ch = in->get();

        if (ch < 0)
        {
          if (looping)
          {
            in->seekg(0, ios::beg);
            ch = in->get();
          }
          else
          {
            finished = true;
            sample = 0.0;
            return true;
          }
        }
      }

      if (isspace(ch))
        continue;

      int znote, rests, sample_number;
      double duration_scale = 1.0;

      switch (ch)
      {
        case 'o':
        case 'O':
          octave = expect_int(in);
          if (octave < 0)
          {
            cerr << "Warning: Syntax error in input: O" << octave << endl;
            octave = 0;
          }
          if (octave > 6)
          {
            cerr << "Warning: Syntax error in input: O" << octave << endl;
            octave = 6;
          }
          break;
        case '<':
          if (octave > 0)
            octave--;
          break;
        case '>':
          if (octave < 6)
            octave++;
          break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
          znote = note_by_name[tolower(ch) - 'a'] + 12*octave + accidental(in) + 1;
          this_note_length_denominator = note_length_denominator;
          duration_scale = expect_duration(in, this_note_length_denominator);
          if (note_length_denominator && (this_note_length_denominator == 0))
          {
            cerr << "Warning: invalid note length denominator " << this_note_length_denominator << endl;
            this_note_length_denominator = note_length_denominator;
          }

          if ((current_waveform == Waveform::Sample) && (current_sample != NULL))
          {
            current_sample->begin_new_note(NULL, this, &current_sample_context, 0.02 * ticks_per_second, true, &znote);

            recalc(znote, duration_scale);

            if (overlap_notes)
            {
              channel_DYNAMIC *ancillary = new channel_DYNAMIC(*this, current_sample, current_sample_context, fade_per_tick);

              ancillary->intensity = actual_intensity;

              ancillary_channels.push_back(ancillary);
            }
          }
          break;
        case 'n':
        case 'N':
          znote = expect_int(in);
          duration_scale = expect_duration(in, *(int *)NULL);
          this_note_length_denominator = note_length_denominator;
          if (znote < 0)
          {
            cerr << "Warning: Syntax error in input: N" << znote << endl;
            znote = 0;
          }
          if (octave > 84)
          {
            cerr << "Warning: Syntax error in input: N" << znote << endl;
            znote = 84;
          }
          if ((znote != 0) && (current_waveform == Waveform::Sample) && (current_sample != NULL))
            current_sample->begin_new_note(NULL, this, &current_sample_context, 0.02 * ticks_per_second, true, &znote);
          recalc(znote, duration_scale);
          break;
        case 'l':
        case 'L':
          note_length_denominator = expect_int(in);
          if (note_length_denominator < 1)
          {
            if ((note_length_denominator < 0) || (current_waveform != Waveform::Sample))
            {
              cerr << "Warning: Invalid note length specified: 1/" << note_length_denominator << endl;
              note_length_denominator = 1;
            }
          }
          else if (note_length_denominator > 64)
          {
            cerr << "Warning: Invalid note length specified: 1/" << note_length_denominator << endl;
            note_length_denominator = 64;
          }
          break;
        case 'm':
        case 'M':
          ch = in->get();
          if (ch < 0)
          {
            finished = true;
            sample = 0.0;
            return true;
          }

          switch (ch)
          {
            case 'l':
            case 'L':
              rest_ticks_proportion = 0.0 / 8.0;
              break;
            case 'n':
            case 'N':
              rest_ticks_proportion = 1.0 / 8.0;
              break;
            case 's':
            case 'S':
              rest_ticks_proportion = 2.0 / 8.0;
              break;
            case 'b':
            case 'B':
            case 'f':
            case 'F':
              cerr << "Warning: Ignoring input: M" << char(toupper(ch)) << endl;
              break;
            default:
              cerr << "Warning: Syntax error in input: M" << char(toupper(ch)) << endl;
          }
          break;
        case 'p':
        case 'P':
          rests = expect_int(in);
          duration_scale = expect_duration(in, *(int *)NULL);
          if (rests < 1)
          {
            cerr << "Warning: Syntax error in input: P" << rests << endl;
            rests = 1;
          }
          if (rests > 64)
          {
            cerr << "Warning: Syntax error in input: P" << rests << endl;
            rests = 64;
          }
          this_note_length_denominator = rests;
          recalc(0, 1.0);
          break;
        case 't':
        case 'T':
          tempo = expect_int(in);
          if (tempo < 32)
          {
            cerr << "Warning: Syntax error in input: T" << tempo << endl;
            tempo = 32;
          }
          if (tempo > 255)
          {
            cerr << "Warning: Syntax error in input: T" << tempo << endl;
            tempo = 255;
          }
          break;
        case 'w': // extended option
        case 'W':
          ch = in->get();
          if (ch < 0)
          {
            finished = true;
            sample = 0.0;
            return true;
          }

          switch (ch)
          {
            case 's':
            case 'S':
              current_waveform = Waveform::Sine;
              break;
            case 'q':
            case 'Q':
              current_waveform = Waveform::Square;
              break;
            case 'w':
            case 'W':
              current_waveform = Waveform::Sawtooth;
              break;
            case 'r':
            case 'R':
              current_waveform = Waveform::RampDown;
              break;
            case 't':
            case 'T':
              current_waveform = Waveform::Triangle;
              break;
            case 'f':
            case 'F':
              play_full_sample = true;
              break;
            case 'n':
            case 'N':
              play_full_sample = false;
              break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
              in->putback(ch);
              sample_number = expect_int(in);

              if ((sample_number < 0) || (sample_number >= int(samples.size())))
                cerr << "Warning: Sample number " << sample_number << " out of range" << endl;
              else
              {
                current_sample = samples[sample_number];
                if (current_sample == NULL)
                  current_sample_context = NULL;
                current_waveform = Waveform::Sample;
              }
              break;
            default:
              cerr << "Warning: Syntax error in input: W" << char(toupper(ch)) << endl;
          }
          break;
      }
    }
    return false;
  }
};
