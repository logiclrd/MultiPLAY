struct s3m_flags
{
  char flags;

  bool st2vibrato()  { return (0 != (flags &   1)); }
  bool st2tempo()    { return (0 != (flags &   2)); }
  bool amigaslides() { return (0 != (flags &   4)); }
  bool zerovol()     { return (0 != (flags &   8)); }
  bool amigalimits() { return (0 != (flags &  16)); }
  bool sbfiltersfx() { return (0 != (flags &  32)); }
  bool reserved()    { return (0 != (flags &  64)); }
  bool customdata()  { return (0 != (flags & 128)); }
};

struct s3m_created_with_tracker
{
  short value;

  int minorversion()
  {
    return static_cast<unsigned short>(value) & 255;
  }

  int majorversion()
  {
    return (static_cast<unsigned short>(value) >> 8) & 15;
  }

  int tracker()
  {
    return (static_cast<unsigned short>(value) >> 12);
  }
};

struct s3m_settings
{
  unsigned char global_volume;
  unsigned char initial_speed;
  unsigned char initial_tempo;
  unsigned char master_volume;
};

struct s3m_channel_description
{
  char value;

  unsigned int channel_type()
  {
    return static_cast<unsigned char>(value) & 127;
  }

  bool disabled()
  {
    return (0 != (value & 128));
  }
};

struct s3m_channel_default_pan
{
  char value;

  unsigned int default_pan()
  {
    return static_cast<unsigned int>(value) & 15;
  }

  bool default_specified()
  {
    return (0 != (value & 32));
  }

  bool default_specified(bool specified)
  {
    value = (value & ~32) | (specified ? 32 : 0);
    return specified;
  }
};

struct s3m_sample_flags
{
  char value;

  bool loop()
  {
    return (0 != (value & 1));
  }

  bool stereo()
  {
    return (0 != (value & 2));
  }

  bool sixteen_bit()
  {
    return (0 != (value & 4));
  }
};

module_struct *load_s3m(ifstream *file)
{
  unsigned char lsb_bytes[4];

  char songname[29];
  songname[28] = 0;
  file->read(songname, 28);

  int sig = file->get();

  if (sig != 0x1A)
    cerr << "Warning: missing 1A signature at byte 28" << endl;
    //throw "invalid file format";

  int type = file->get();

  if (type != 16)
    throw "unknown file type";

  file->ignore(2);

  file->read((char *)&lsb_bytes[0], 2);
  short pattern_list_length = from_lsb2(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 2);
  short num_samples = from_lsb2(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 2);
  short num_patterns = from_lsb2(lsb_bytes);

  s3m_flags flags;
  file->read((char *)&flags.flags, 1);
  file->ignore(1);

  s3m_created_with_tracker cwt;

  file->read((char *)&lsb_bytes[0], 2);
  cwt.value = lsb_bytes[0] | (lsb_bytes[1] << 8);

  short signed_samples;
  file->read((char *)&lsb_bytes[0], 2);
  signed_samples = lsb_bytes[0] | (lsb_bytes[1] << 8);
  bool unsigned_samples = (signed_samples == 2);

  char magic[4];
  file->read(magic, 4);
  
  if (string(magic, 4) != "SCRM")
    throw "invalid file format (missing 'SCRM' near start of file)";

  s3m_settings settings;

  file->read((char *)&settings.global_volume, 1);
  file->read((char *)&settings.initial_speed, 1);
  file->read((char *)&settings.initial_tempo, 1);
  file->read((char *)&settings.master_volume, 1);

  file->ignore(1); // ultraclick removal
  char default_panning = file->get();
  file->ignore(8);

/*  short special_ptr;
  file->read((char *)&special_ptr, 2); /* */
  file->ignore(2);

  if (int(file->tellg()) != 64)
    throw "internal error: not at the right place in the file";

  s3m_channel_description channels[32];
  for (int i=0; i<32; i++)
    channels[i].value = file->get();

  if (pattern_list_length & 1)
    throw "pattern list length is not even";

  ArrayAllocator<unsigned char> pattern_list_index_allocator(pattern_list_length);
  unsigned char *pattern_list_index = pattern_list_index_allocator.getArray();
  file->read((char *)pattern_list_index, pattern_list_length);

  ArrayAllocator<unsigned short> sample_parapointer_allocator(num_samples);
  unsigned short *sample_parapointer = sample_parapointer_allocator.getArray();
  for (int i=0; i<num_samples; i++)
  {
    file->read((char *)&lsb_bytes[0], 2);
    sample_parapointer[i] = from_lsb2_u(lsb_bytes);
  }

  ArrayAllocator<unsigned short> pattern_parapointer_allocator(num_patterns);
  unsigned short *pattern_parapointer = pattern_parapointer_allocator.getArray();
  for (int i=0; i<num_patterns; i++)
  {
    file->read((char *)&lsb_bytes[0], 2);
    pattern_parapointer[i] = from_lsb2_u(lsb_bytes);
  }

  s3m_channel_default_pan default_panning_value[32];
#pragma pack()

  if (default_panning == char(0xFC))
    for (int i=0; i<32; i++)
      default_panning_value[i].value = file->get();
  else
    for (int i=0; i<32; i++)
      default_panning_value[i].default_specified(false);

  vector<sample *> samps;

  for (int i=0; i<num_samples; i++)
  {
    file->seekg(sample_parapointer[i] * 16, ios::beg);

    int type = file->get();

    if (type != 1)
    {
      samps.push_back(new sample_builtintype<char>(i, 1));
      continue;
    }

    char dos_filename[13];
    dos_filename[12] = 0;
    file->read(dos_filename, 12);

    file->ignore(1);

    file->read((char *)&lsb_bytes[0], 2);
    int parapointer = from_lsb2_u(lsb_bytes);;

    unsigned long length, loop_begin, loop_end;

    file->read((char *)&lsb_bytes[0], 4);
    length = from_lsb4_lu(lsb_bytes);

    file->read((char *)&lsb_bytes[0], 4);
    loop_begin = from_lsb4_lu(lsb_bytes);

    file->read((char *)&lsb_bytes[0], 4);
    loop_end = from_lsb4_lu(lsb_bytes);

    char volume = file->get();

    file->ignore(1);

    bool packed;
    packed = (file->get() == 1);

    s3m_sample_flags flags;
    file->read((char *)&flags.value, 1);

    file->read((char *)&lsb_bytes[0], 4);
    unsigned long c2spd = from_lsb4_lu(lsb_bytes);

    file->ignore(4);

    file->ignore(2); // gravis ultrasound ptr
    file->ignore(2); // soundblaster loop expansion
    file->ignore(4); // soundblaster 'last used' position

    char sample_name[29];
    sample_name[28] = 0;
    file->read(sample_name, 28);

    char magic[4];
    file->read(magic, 4);

    if (string(magic, 4) != "SCRS")
      throw "invalid sample format";

    file->seekg(parapointer * 16, ios::beg);

    /*if (flags.stereo())
      length >>= 1; /* */

    if (flags.sixteen_bit())
    {
      //length >>= 1;
      if (unsigned_samples)
      {
        unsigned short *data = new unsigned short[length];
        unsigned short *data_right;
        
        if (flags.stereo())
        {
          data_right = new unsigned short[length];

          for (unsigned int j=0; j<length; j++)
          {
            file->read((char *)&data[j], 2);
            file->read((char *)&data_right[j], 2);
          }
        }
        else
          file->read((char *)data, 2 * length);

        unsigned char *uc_data = (unsigned char *)&data[0];
        unsigned char *uc_data_right = NULL;
        if (flags.stereo())
          uc_data_right = (unsigned char *)&data_right[0];

        for (unsigned int j=0; j<length; j++)
        {
          data[j] = from_lsb2_u(uc_data + 2*j);
          if (flags.stereo())
            data_right[j] = from_lsb2_u(uc_data_right + 2*j);
        }

        signed short *data_sgn = (signed short *)data;
        signed short *data_right_sgn = (signed short *)data_right;

        if (flags.stereo())
          for (unsigned int i=0; i<length; i++)
          {
            data_sgn[i] = (short)(int(data[i]) - 32768);
            data_right_sgn[i] = (short)(int(data_right[i]) - 32768);
          }
        else
          for (unsigned int i=0; i<length; i++)
            data_sgn[i] = (short)(int(data[i]) - 32768);

        sample_builtintype<signed short> *smp = new sample_builtintype<signed short>(i, flags.stereo() ? 2 : 1);

        smp->num_samples = length;
        smp->sample_data[0] = data_sgn;
        if (flags.stereo())
          smp->sample_data[1] = data_right_sgn;
        smp->samples_per_second = c2spd;
        if (flags.loop())
        {
          smp->loop_begin = loop_begin;
          smp->loop_end = loop_end - 1;
        }
        smp->default_volume = volume / 64.0;

        samps.push_back(smp);
      }
      else
      {
        signed short *data = new signed short[length];
        signed short *data_right;

        if (flags.stereo())
        {
          data_right = new signed short[length];

          for (unsigned int j=0; j<length; j++)
          {
            file->read((char *)&data[j], 2);
            file->read((char *)&data_right[j], 2);
          }
        }
        else
          file->read((char *)data, 2 * length);

        unsigned char *uc_data = (unsigned char *)&data[0];
        unsigned char *uc_data_right = NULL;
        if (flags.stereo())
          uc_data_right = (unsigned char *)&data_right[0];

        for (unsigned int j=0; j<length; j++)
        {
          data[j] = from_lsb2(uc_data + 2*j);
          if (flags.stereo())
            data_right[j] = from_lsb2(uc_data_right + 2*j);
        }

        sample_builtintype<signed short> *smp = new sample_builtintype<signed short>(i, flags.stereo() ? 2 : 1);

        smp->num_samples = length;
        smp->sample_data[0] = data;
        if (flags.stereo())
          smp->sample_data[1] = data_right;
        smp->samples_per_second = c2spd;
        if (flags.loop())
        {
          smp->loop_begin = loop_begin;
          smp->loop_end = loop_end - 1;
        }
        smp->default_volume = volume / 64.0;

        samps.push_back(smp);
      }
    }
    else
    {
      if (unsigned_samples)
      {
        unsigned char *data = new unsigned char[length];
        unsigned char *data_right;

        if (flags.stereo())
        {
          data_right = new unsigned char[length];

          for (unsigned int j=0; j<length; j++)
          {
            file->read((char *)&data[j], 1);
            file->read((char *)&data_right[j], 1);
          }
        }
        else
          file->read((char *)data, length);

        signed char *data_sgn = (signed char *)data;
        signed char *data_right_sgn = (signed char *)data_right;

        if (flags.stereo())
          for (unsigned int i=0; i<length; i++)
          {
            data_sgn[i] = (char)(int(data[i]) - 128);
            data_right_sgn[i] = (char)(int(data_right[i]) - 128);
          }
        else
          for (unsigned int i=0; i<length; i++)
            data_sgn[i] = (char)(int(data[i]) - 128);

        sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(i, flags.stereo() ? 2 : 1);

        smp->num_samples = length;
        smp->sample_data[0] = data_sgn;
        if (flags.stereo())
          smp->sample_data[1] = data_right_sgn;
        smp->samples_per_second = c2spd;
        if (flags.loop())
        {
          smp->loop_begin = loop_begin;
          smp->loop_end = loop_end - 1;
        }
        smp->default_volume = volume / 64.0;

        samps.push_back(smp);
      }
      else
      {
        signed char *data = new signed char[length];
        signed char *data_right;

        if (flags.stereo())
        {
          data_right = new signed char[length];

          for (unsigned int j=0; j<length; j++)
          {
            file->read((char *)&data[j], 1);
            file->read((char *)&data_right[j], 1);
          }
        }
        else
          file->read((char *)data, length);

        sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(i, flags.stereo() ? 2 : 1);

        smp->num_samples = length;
        smp->sample_data[0] = data;
        if (flags.stereo())
          smp->sample_data[1] = data_right;
        smp->samples_per_second = c2spd;
        if (flags.loop())
        {
          smp->loop_begin = loop_begin;
          smp->loop_end = loop_end - 1;
        }
        smp->default_volume = volume / 64.0;

        samps.push_back(smp);
      }
    }
  }

  vector<pattern> pats;

  for (int i=0; i<num_patterns; i++)
  {
    file->seekg(pattern_parapointer[i] * 16, ios::beg);

    file->read((char *)&lsb_bytes[0], 2);
    unsigned short pattern_data_length = from_lsb2_u(lsb_bytes);

    int bytes_left = pattern_data_length;

    pattern new_pattern;

    for (int row_number=0; row_number<64; row_number++)
    {
      vector<row> rowdata(32);

      while (true)
      {
        char what = file->get(); if (!--bytes_left) break;

        if (what == 0)
          break;

        int channel = (what & 31);

        if (what & 32)
        {
          int snote = file->get(); if (!--bytes_left) break;
          if (snote > 127)
            snote -= 256;
          rowdata[channel].snote = snote;
          rowdata[channel].znote = znote_from_snote(snote);

          int instrument = file->get();
          if ((instrument > 0) && (instrument <= int(samps.size())))
            rowdata[channel].instrument = samps[instrument - 1];
          else
            rowdata[channel].instrument = NULL;
          if (!--bytes_left) break;
        }

        if (what & 64)
        {
          rowdata[channel].volume = file->get(); if (!--bytes_left) break;
        }

        if (what & 128)
        {
          int command = file->get(); if (!--bytes_left) break;
          int info = file->get(); if (!--bytes_left) break;

          rowdata[channel].effect = effect_struct(effect_type_s3m, command, info);
        }
      }

      new_pattern.row_list.push_back(rowdata);

      if (!bytes_left)
        break;
    }
    pats.push_back(new_pattern);
  }

  module_struct *ret = new module_struct();

  ret->name = songname;

  ret->stereo = ((settings.master_volume & 128) != 0);

  ret->patterns = pats;
  ret->samples = samps;

  for (int i=0; i<pattern_list_length; i++)
  {
    if (pattern_list_index[i] == 254) //  ++ (skip)
    {
      ret->pattern_list.push_back(NULL);
      continue;
    }
    if (pattern_list_index[i] == 255) //  -- (end of tune)
      break;
    ret->pattern_list.push_back(&ret->patterns[pattern_list_index[i]]);
  }

  memset(ret->base_pan, 0, sizeof(ret->base_pan));

  {
    int i, j;

    for (i=0, j=0; i<32; i++)
    {
      ret->channel_enabled[i] = !channels[i].disabled();
      if (ret->channel_enabled[i])
        ret->channel_map[i] = j++;
      if (ret->stereo)
      {
        ret->initial_panning[i].set_channels(2);
        if (default_panning == char(0xFC))
        {
          if (default_panning_value[i].default_specified())
            ret->base_pan[i] = default_panning_value[i].default_pan();
          else
            ret->base_pan[i] = (i & 1) ? 0xC : 0x3;
          ret->initial_panning[i].from_s3m_pan(ret->base_pan[i]);
        }
        else
          switch (channels[i].channel_type())
          {
            case 0x0: case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
              ret->base_pan[i] = 0x3;
              ret->initial_panning[i].from_s3m_pan(0x3);
              break;
            case 0x8: case 0x9: case 0xA: case 0xB: case 0xC: case 0xD: case 0xE: case 0xF:
              ret->base_pan[i] = 0xC;
              ret->initial_panning[i].from_s3m_pan(0xC);
              break;
          }
      }
    }

    for (i=32; i<MAX_MODULE_CHANNELS; i++)
      ret->channel_enabled[i] = false;

    ret->num_channels = j;
  }

  ret->speed = settings.initial_speed;
  ret->tempo = settings.initial_tempo;

  return ret;
}
