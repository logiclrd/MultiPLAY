struct mod_sample_description
{
  char name[23];
  int byte_length;
  char finetune;
  unsigned char volume;
  int repeat_start, repeat_length;
} sample_description[32];

module_struct *load_mod(ifstream *file, bool mud = false)
{
  char songname[21];
  songname[20] = 0;
  
  file->read(songname, 20);

  for (int i=0; i<31; i++)
  {
    unsigned char msb_chars[2];

    file->read(sample_description[i].name, 22);
    sample_description[i].name[22] = 0;

    file->read((char *)msb_chars, 2);
    sample_description[i].byte_length = 2 * from_msb2(msb_chars);
    
    sample_description[i].finetune = file->get();
    sample_description[i].volume = file->get();

    file->read((char *)msb_chars, 2);
    sample_description[i].repeat_start = 2 * from_msb2(msb_chars);

    file->read((char *)msb_chars, 2);
    sample_description[i].repeat_length = 2 * from_msb2(msb_chars);
  }
  int num_samples = 31, num_channels;

reinterpret:
  int order_list_length = file->get();
  if (order_list_length < 0)
    order_list_length += 256;

  int song_auto_loop_target = file->get();
  if (song_auto_loop_target < 0)
    song_auto_loop_target += 256;

  char order_table[128];
  file->read(&order_table[0], 128);

  char magic_str[4];
  file->read(magic_str, 4);
  string magic(magic_str, 4);

  bool weird_channel_format = false;

  if (isdigit(magic[0]) && (magic.substr(1, 3) == "CHN"))
    num_channels = magic[0] - '0';
  else if (isdigit(magic[0]) && isdigit(magic[1]) && (magic.substr(2, 2) == "CH"))
    num_channels = 10 * (magic[0] - '0') + (magic[1] - '0');
  else if ((magic == "M.K.") || (magic == "M!K!") || (magic == "FLT4"))
    num_channels = 4;
  else if ((magic == "OCTA") || (magic == "CD81"))
    num_channels = 8;
  else if (magic == "FLT8")
    num_channels = 8, weird_channel_format = true;
  else if ((magic.substr(0, 3) == "TDZ") && isdigit(magic[3]))
    num_channels = magic[3] - '0';
  else if (num_samples == 31)
  {
    num_samples = 15;
    file->seekg(470);
    goto reinterpret;
  }
  else
    throw "unable to deduce MOD file format";

  int max_pattern_index = 0;
  for (int i=0; i<order_list_length; i++)
    if (order_table[i] > max_pattern_index)
      max_pattern_index = order_table[i];

  int file_length;
  {
    int tmp = file->tellg();
    file->seekg(0, ios::end);
    file_length = file->tellg();
    file->seekg(tmp, ios::beg);
  }

  int header_bytes = 154 + num_samples * 30;
  int pattern_bytes = 256 * num_channels;
  int sample_bytes = 0;

  for (int i=0; i<num_samples; i++)
    sample_bytes += sample_description[i].byte_length;

  int bytes_left_over = (file_length - (header_bytes + sample_bytes));

  int num_patterns = bytes_left_over / pattern_bytes;

  if ((bytes_left_over % pattern_bytes) == 0)
  {
    if (num_patterns != 1 + max_pattern_index)
    {
      if (mud)
      {
        cerr << "Ignoring the fact that there appear to be unused patterns in this MOD file." << endl
             << "Using the largest pattern index in the order list to calculate the number of" << endl
             << "patterns." << endl;

        num_patterns = 1 + max_pattern_index;
      }
      else
        cerr << "Note: There appear to be unused patterns in this MOD file. The loader will" << endl
             << "      assume that this is the case. If the song sounds funny, try renaming the" << endl
             << "      file to the extension MUD instead of MOD. The loader will then use the" << endl
             << "      largest pattern index in the order list as an indicator of the number of" << endl
             << "      patterns." << endl;
    }
  }
  else
  {
    if (num_patterns <= max_pattern_index)
      cerr << "Warning: This file appears to be damaged. Some samples might not sound right." << endl;
    else if (num_patterns > 1 + max_pattern_index)
    {
      if (mud)
      {
        cerr << "Warning: There appears to be garbage at the end of this file." << endl
             << "Ignoring the fact that there appear to be unused patterns in this MOD file." << endl
             << "Using the largest pattern index in the order list to determine the number of" << endl
             << "patterns." << endl;

        num_patterns = 1 + max_pattern_index;
      }
      else
        cerr << "Warning: There appears to be garbage at the end of this file. If you want the" << endl
             << "         largest possible amount of this file to be interpreted as pattern data" << endl
             << "         rename the file to the extension MUD instead of MOD. The loader will" << endl
             << "         then use the largest pattern index in the order list to determine the" << endl
             << "         number of patterns." << endl;
    }
  }

  ArrayAllocator<unsigned char> pattern_data_buffer_allocator(pattern_bytes);
  unsigned char *pattern_data_buffer = pattern_data_buffer_allocator.getArray();
  vector<pattern> pats;

  vector<sample *> samps;

  for (int i=0; i<num_samples; i++) // pre-create samples
    samps.push_back(new sample_builtintype<signed char>(i, 1));

  for (int i=0; i<num_patterns; i++)
  {
    file->read((char *)&pattern_data_buffer[0], pattern_bytes);

    pattern new_pattern;

    if (!weird_channel_format)
    {
      for (int i=0, o=0; i<64; i++)
      {
        vector<row> rowdata(num_channels);

        for (int j=0; j<num_channels; j++,o+=4)
        {
          row &r = rowdata[j];

          int period = ((pattern_data_buffer[o] & 15) << 8)
                     | pattern_data_buffer[o + 1];

          if (period == 0)
            r.snote = -1;
          else
          {
            r.snote = snote_from_period(period);
            r.znote = znote_from_snote(r.snote);
          }

          int instrument = (pattern_data_buffer[o + 2] >> 4)
                         | (pattern_data_buffer[o] & 0xF0);
          if ((instrument > 0) && (instrument <= int(samps.size())))
            r.instrument = samps[instrument - 1];
          else
            r.instrument = NULL;

          r.effect.init(effect_type_mod,
                        pattern_data_buffer[o + 2] & 0xF,
                        pattern_data_buffer[o + 3], &r);
        }
        new_pattern.row_list.push_back(rowdata);
      }
    }
    else
    {
      for (int i=0, o=0; i<64; i++)
      {
        vector<row> rowdata(num_channels);

        for (int j=0; j<4; j++,o+=4)
        {
          {
            row &r = rowdata[j];

            int period = ((pattern_data_buffer[o] & 15) << 8)
                      | pattern_data_buffer[o + 1];

            if (period == 0)
              r.snote = -1;
            else
            {
              r.snote = snote_from_period(period);
              r.znote = znote_from_snote(r.snote);
            }

            int instrument = (pattern_data_buffer[o + 2] >> 4)
                          | (pattern_data_buffer[o] & 0xF0);
            if ((instrument > 0) && (instrument <= int(samps.size())))
              r.instrument = samps[instrument - 1];
            else
              r.instrument = NULL;

            r.effect.init(effect_type_mod,
                          pattern_data_buffer[o + 2] & 0xF,
                          pattern_data_buffer[o + 3], &r);
          }
          o += 1024;
          {
            row &r = rowdata[j + 4];

            int period = ((pattern_data_buffer[o] & 15) << 8)
                      | pattern_data_buffer[o + 1];

            if (period == 0)
              r.snote = -1;
            else
            {
              r.snote = snote_from_period(period);
              r.znote = znote_from_snote(r.snote);
            }

            int instrument = (pattern_data_buffer[o + 2] >> 4)
                          | (pattern_data_buffer[o] & 0xF0);
            if ((instrument > 0) && (instrument <= int(samps.size())))
              r.instrument = samps[instrument - 1];
            else
              r.instrument = NULL;

            r.effect.init(effect_type_mod,
                          pattern_data_buffer[o + 2] & 0xF,
                          pattern_data_buffer[o + 3], &r);
          }
          o -= 1024;
        }
        new_pattern.row_list.push_back(rowdata);
      }
    }
    
    pats.push_back(new_pattern);
  }

  for (int i=0; i<num_samples; i++)
  {
    sample_builtintype<signed char> *smp = (sample_builtintype<signed char> *)samps[i];

    smp->num_samples = sample_description[i].byte_length;

    if (sample_description[i].repeat_length > 2)
    {
      smp->loop_begin = sample_description[i].repeat_start;
      smp->loop_end = sample_description[i].repeat_start
                    + sample_description[i].repeat_length - 1;
    }

    smp->sample_data[0] = new signed char[smp->num_samples];
    file->read((char *)&smp->sample_data[0][0], smp->num_samples);

    smp->samples_per_second = mod_finetune[8 ^ sample_description[i].finetune];
    smp->default_volume = sample_description[i].volume / 64.0;
  }

  module_struct *ret = new module_struct();

  ret->name = songname;
  ret->num_channels = num_channels;
  ret->speed = 6;
  ret->tempo = 125;

  ret->stereo = true;

  for (int i=0; i<32; i++)
  {
    ret->channel_enabled[i] = (i < num_channels);
    ret->channel_map[i] = i;

    bool left = (i & 1) ^ (0 == (i & 2));

    if (left)
      ret->base_pan[i] = 0x3;
    else
      ret->base_pan[i] = 0xC;

    ret->initial_panning[i].from_s3m_pan(ret->base_pan[i]);
  }

  for (int i=32; i<MAX_MODULE_CHANNELS; i++)
    ret->channel_enabled[i] = false;

  ret->speed_change();

  ret->samples = samps;
  ret->patterns = pats;

  for (int i=0; i<order_list_length; i++)
    ret->pattern_list.push_back(&ret->patterns[order_table[i]]);

  ret->auto_loop_target = song_auto_loop_target;

  return ret;
}
