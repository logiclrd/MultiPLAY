#include "bit_memory_stream.h"

struct it_created_with_tracker
{
  int major_version, minor_version;
  int compatible_with_major_version, compatible_with_minor_version;

  bool compatible_exceeds(double version)
  {
    double compatible = compatible_with_major_version + (compatible_with_minor_version / 100.0);

    return (compatible >= version);
  }
};

struct it_flags
{
  char value;

  bool stereo()                 { return (0 != (value &   1)); }
  bool vol0mixopt()             { return (0 != (value &   2)); }
  bool use_instruments()        { return (0 != (value &   4)); }
  bool linear_slides()          { return (0 != (value &   8)); }
  bool old_effects()            { return (0 != (value &  16)); }
  bool portamento_memory_link() { return (0 != (value &  32)); }
  bool use_midi_pitch_ctrl()    { return (0 != (value &  64)); }
  bool embedded_midi_config()   { return (0 != (value & 128)); }
};

struct it_settings
{
  unsigned char global_volume, mix_volume, initial_speed, initial_tempo;
};

struct it_envelope_node
{
  int tick;
  double yValue;

  it_envelope_node(unsigned char t, double y)
    : tick(t), yValue(y)
  {
  }
};

struct it_envelope_description
{
  bool enabled, looping, sustain_loop;

  int loop_start_node, loop_end_node;
  int sustain_loop_start_node, sustain_loop_end_node;

  vector<it_envelope_node> envelope;
};

struct it_instrument_description
{
  double global_volume;
  int default_pan;
  bool use_default_pan;
  
  char pitch_pan_center;
  unsigned char pitch_pan_separation;

  double volume_variation_pctg, panning_variation;

  int fade_out;

  DuplicateCheck::Type duplicate_note_check;
  DuplicateCheckAction::Type duplicate_check_action;
  NewNoteAction::Type new_note_action;

  int note_sample[120];
  int tone_offset[120];

  it_envelope_description volume_envelope, pan_envelope, pitch_envelope;
};

struct it_instrument_flags
{
  char value;

  bool use_volume_envelope()     { return (0 != (value & 1)); }
  bool use_volume_loop()         { return (0 != (value & 2)); }
  bool use_sustain_volume_loop() { return (0 != (value & 4)); }
};

struct it_envelope_flags
{
  char value;

  bool enabled()      { return (0 != (value & 1)); }
  bool looping()      { return (0 != (value & 2)); }
  bool sustain_loop() { return (0 != (value & 4)); }
};

void load_it_old_instrument(ifstream *file, it_instrument_description &desc, int i)
{
  char magic[4];

  file->read(magic, 4);

  if (string(magic, 4) != "IMPI")
    throw "invalid instrument (missing 'IMPI' at start of header)";

  char dos_filename[13];
  file->read(dos_filename, 13);

  if (dos_filename[12] != 0)
  {
    dos_filename[12] = 0;
    cerr << "Warning: DOS filename not properly null-terminated in instrument #" << i << endl;
  }

  it_instrument_flags instrument_flags;
  file->read(&instrument_flags.value, 1);

  unsigned char volume_loop_start_node, volume_loop_end_node;
  unsigned char sustain_loop_start_node, sustain_loop_end_node;

  file->read((char *)&volume_loop_start_node, 1);
  file->read((char *)&volume_loop_end_node, 1);
  file->read((char *)&sustain_loop_start_node, 1);
  file->read((char *)&sustain_loop_end_node, 1);

  unsigned char lsb_bytes[2];

  file->read((char *)&lsb_bytes, 2);
  int fade_out = from_lsb2_u(lsb_bytes) * 2; // fadeout is twice as fine in the new format

  char new_note_action = file->get();
  char duplicate_note_check = file->get();

  file->ignore(3);

  char instrument_name[27];
  file->read(instrument_name, 26);
  instrument_name[26] = 0;

  file->ignore(6);

  unsigned char instrument_for_note[120] = { 0 }; // empty array
  int note_difference[120] = { 0 }; // empty

  for (int i=0; i<120; i++)
  {
    unsigned char instrument, note;

    file->read((char *)&note, 1);
    file->read((char *)&instrument, 1);

    instrument_for_note[i] = instrument;
    note_difference[i] = note - i;
  }

  char rendered_volume_envelope[200];
  file->read(rendered_volume_envelope, 200);

  vector<it_envelope_node> volume_envelope;
  for (int i=0; i<25; i++)
  {
    unsigned char tick, magnitude;

    file->read((char *)&tick, 1);
    file->read((char *)&magnitude, 1);

    if (tick != 0xFF)
      volume_envelope.push_back(it_envelope_node(tick, magnitude / 64.0));
  }

  desc.global_volume = 1.0;
  desc.use_default_pan = false;

  desc.volume_variation_pctg = 0.0;
  desc.panning_variation = 0;

  desc.fade_out = fade_out;

  desc.new_note_action = (NewNoteAction::Type)new_note_action;

  if (duplicate_note_check == 1)
    desc.duplicate_note_check = DuplicateCheck::Instrument;
  else
    desc.duplicate_note_check = DuplicateCheck::Off;
  desc.duplicate_check_action = DuplicateCheckAction::Cut;

  for (int i=0; i<120; i++)
  {
    desc.note_sample[i] = instrument_for_note[i];
    desc.tone_offset[i] = note_difference[i];
  }

  desc.volume_envelope.enabled = instrument_flags.use_volume_envelope();
  desc.volume_envelope.looping = instrument_flags.use_volume_loop();
  desc.volume_envelope.sustain_loop = instrument_flags.use_sustain_volume_loop();

  desc.volume_envelope.loop_start_node = volume_loop_start_node;
  desc.volume_envelope.loop_end_node = volume_loop_end_node;

  desc.volume_envelope.sustain_loop_start_node = sustain_loop_start_node;
  desc.volume_envelope.sustain_loop_end_node = sustain_loop_end_node;

  desc.volume_envelope.envelope = volume_envelope;

  desc.pan_envelope.enabled = false;
  desc.pitch_envelope.enabled = false;
}

void load_it_new_instrument_envelope(ifstream *file, it_envelope_description &desc, bool signed_magnitude)
{
  it_envelope_flags flags;
  flags.value = file->get();

  int num_node_points = file->get();

  int loop_begin, loop_end;
  int sustain_loop_begin, sustain_loop_end;

  loop_begin = file->get();
  loop_end = file->get();
  sustain_loop_begin = file->get();
  sustain_loop_end = file->get();

  vector<it_envelope_node> nodes;

  unsigned char lsb_bytes[2];

  for (int i=0; i<25; i++)
  {
    int magnitude, tick;

    if (signed_magnitude)
      magnitude = file->get();
    else
    {
      unsigned char mag;
      file->read((char *)&mag, 1);
      magnitude = mag;
    }

    file->read((char *)&lsb_bytes[0], 2);
    tick = from_lsb2(lsb_bytes);

    if (i >= num_node_points)
      continue;

    if (signed_magnitude)
      nodes.push_back(it_envelope_node(tick, magnitude / 32.0));
    else
      nodes.push_back(it_envelope_node(tick, magnitude / 64.0));
  }

  desc.enabled = flags.enabled();
  desc.looping = flags.looping();
  desc.sustain_loop = flags.sustain_loop();

  desc.loop_start_node = loop_begin;
  desc.loop_end_node = loop_end;
  desc.sustain_loop_start_node = sustain_loop_begin;
  desc.sustain_loop_end_node = sustain_loop_end;

  desc.envelope = nodes;
}

void load_it_new_instrument(ifstream *file, it_instrument_description &desc, int i)
{
  char magic[4];

  file->read(magic, 4);

  if (string(magic, 4) != "IMPI")
    throw "invalid instrument (missing 'IMPI' at start of header)";

  char dos_filename[13];
  file->read(dos_filename, 13);

  if (dos_filename[12] != 0)
  {
    dos_filename[12] = 0;
    cerr << "Warning: DOS filename not properly null-terminated in instrument #" << i << endl;
  }

  char new_note_action = file->get();
  char duplicate_check_type = file->get();
  char duplicate_check_action = file->get();

  unsigned char lsb_bytes[2];

  file->read((char *)&lsb_bytes[0], 2);
  int fade_out = from_lsb2(lsb_bytes);

  char pitch_pan_separation = file->get();
  unsigned char pitch_pan_center;
  file->read((char *)&pitch_pan_center, 1);

  unsigned char global_volume, default_pan;
  file->read((char *)&global_volume, 1);
  file->read((char *)&default_pan, 1);

  unsigned char volume_variation;
  char panning_variation;
  file->read((char *)&volume_variation, 1);
  file->read((char *)&panning_variation, 1);

  file->ignore(4); // TrkVers (2), NoS and an unused byte

  char instrument_name[27];
  file->read(instrument_name, 26);
  instrument_name[26] = 0;

  char ipc, ipr;
  ipc = file->get();
  ipr = file->get();

  char midi_channel, midi_program;
  int midi_bank;
  midi_channel = file->get();
  midi_program = file->get();
  file->read((char *)&lsb_bytes[0], 2);
  midi_bank = from_lsb2_u(lsb_bytes);

  unsigned char instrument_for_note[120] = { 0 }; // empty array
  int note_difference[120] = { 0 }; // empty

  for (int i=0; i<120; i++)
  {
    unsigned char instrument, note;

    file->read((char *)&note, 1);
    file->read((char *)&instrument, 1);

    instrument_for_note[i] = instrument;
    note_difference[i] = note - i;
  }

  it_envelope_description volume_envelope, pan_envelope, pitch_envelope;

  load_it_new_instrument_envelope(file, volume_envelope, false);
  load_it_new_instrument_envelope(file, pan_envelope, true);
  load_it_new_instrument_envelope(file, pitch_envelope, true);

  desc.global_volume = global_volume / 128.0;

  desc.use_default_pan = (0 == (default_pan & 128));
  if (desc.use_default_pan)
    desc.default_pan = default_pan;

  desc.volume_variation_pctg = volume_variation / 100.0;
  desc.panning_variation = panning_variation;

  desc.fade_out = fade_out;

  desc.pitch_pan_separation = pitch_pan_separation;
  desc.pitch_pan_center = pitch_pan_center;

  desc.new_note_action = (NewNoteAction::Type)new_note_action;
  desc.duplicate_note_check = (DuplicateCheck::Type)duplicate_check_type;
  desc.duplicate_check_action = (DuplicateCheckAction::Type)duplicate_check_action;

  for (int i=0; i<120; i++)
  {
    desc.note_sample[i] = instrument_for_note[i];
    desc.tone_offset[i] = note_difference[i];
  }

  desc.volume_envelope = volume_envelope;
  desc.pan_envelope = pan_envelope;
  desc.pitch_envelope = pitch_envelope;
}

struct it_sample_flags
{
  char value;

  bool sample_present()        { return (0 != (value &   1)); }
  bool sixteen_bit()           { return (0 != (value &   2)); }
  bool stereo()                { return (0 != (value &   4)); }
  bool compressed()            { return (0 != (value &   8)); }
  bool looping()               { return (0 != (value &  16)); }
  bool sustain_loop()          { return (0 != (value &  32)); }
  bool pingpong_loop()         { return (0 != (value &  64)); }
  bool pingpong_sustain_loop() { return (0 != (value & 128)); }
};

struct it_sample_conversion_flags
{
  char value;

  bool signed_samples()    { return (0 != (value &  1)); }
  bool msb_order()         { return (0 != (value &  2)); }
  bool delta_values()      { return (0 != (value &  4)); }
  bool byte_delta_values() { return (0 != (value &  8)); }
  bool tx_wave_12bit()     { return (0 != (value & 16)); }
  bool stereo_prompt()     { return (0 != (value & 32)); }
};

/*
 * NOTICE: The following code is based on 'cheesetracker's sample loader, which is in turn based
 *         on XMP (http://xmp.helllabs.org), which is itself based on openCP's code.
 */
template <class T>
T *load_it_sample_compressed(ifstream *file, long sample_length, bool double_delta)
{
  int max_bit_width = sizeof(T) * 8 + 1, bits_to_store_bit_width = int(log(max_bit_width - 1) / log(2.0));
  unsigned char lsb_bytes[2];

  int delta2, delta3;
  int bit_width;

  T *ret = new T[sample_length];
  int offset = 0;

  while (offset < sample_length)
  {
    file->read((char *)&lsb_bytes[0], 2);
    int block_size = from_lsb2_u(lsb_bytes);

    ArrayAllocator<char> block_allocator(block_size);
    char *block = block_allocator.getArray();

    file->read(block, block_size);

    bit_memory_stream block_bits(block, block_size * 8);

    delta2 = delta3 = 0;
    bit_width = max_bit_width;

    while (!block_bits.eof())
    {
      int aux = block_bits.read_int(bit_width);

      // check for changes in the bit width
      if (bit_width < 7) // method 1: 1 <= bit_width <= 6
      {
        if (aux == bit_value[bit_width - 1])
        {
          aux = block_bits.read_int(bits_to_store_bit_width) + 1;
          if (aux >= bit_width)
            aux++;
          bit_width = aux;
          continue;
        }
      }
      else if (bit_width < max_bit_width) // method 2: 7 <= bit_width < max_bit_width
      {
        int border;

        switch (bit_width)
        {
          case  7: border = 0x003F; break;
          case  8: border = 0x007F; break;
          case  9: border = 0x00FF; break;
          case 10: border = 0x01FF; break;
          case 11: border = 0x03FF; break;
          case 12: border = 0x07FF; break;
          case 13: border = 0x0FFF; break;
          case 14: border = 0x1FFF; break;
          case 15: border = 0x3FFF; break;
          case 16: border = 0x7FFF; break;
        }

        border -= (max_bit_width >> 1);

        aux -= border;
        if (aux >= bit_width)
          aux++;

        if ((aux > 0) && (aux <= max_bit_width))
        {
          bit_width = aux;
          continue;
        }
      }
      else if (bit_width == max_bit_width) // method 3: bit_width == max_bit_width
      {
        if (aux & bit_value[max_bit_width - 1])
        {
          bit_width = (aux + 1) & 255;
          continue;
        }
      }
      else
        throw "invalid bit width";

      int delta;

      if (bit_width == max_bit_width)
        delta = aux;
      else
      {
        int sign_bit = bit_value[bit_width - 1];

        if (0 == (aux & sign_bit)) // positive
        {
          delta = aux;
        }
        else // negative (sign bit set)
        {
          int positive = ((~aux) & (sign_bit - 1)) + 1;
          delta = -positive;
        }
      }

      delta2 += delta;

      if (double_delta)
      {
        delta3 += delta2;
        ret[offset++] = delta3;
      }
      else
        ret[offset++] = delta2;
    }
  }

  return ret;
}

template <class T>
void load_it_sample_uncompressed(ifstream *file, int channels, long sample_length, it_sample_conversion_flags &conversion, T *data[])
{
  for (int i=0; i<channels; i++)
    data[i] = new T[sample_length];

  long total_byte_length = sample_length * channels * sizeof(T);

  ArrayAllocator<unsigned char> data_block_allocator(total_byte_length);
  unsigned char *data_block = data_block_allocator.getArray();

  file->read((char *)&data_block[0], total_byte_length);

  long offset = 0;

  for (long i=0; i<sample_length; i++)
    for (int j=0; j<channels; j++)
    {
      switch (sizeof(T))
      {
        case 1:
          if (conversion.signed_samples())
            data[j][i] = (signed char)data_block[offset++];
          else
            data[j][i] = data_block[offset++];
          break;
        case 2:
          if (conversion.msb_order())
            if (conversion.signed_samples())
              data[j][i] = from_msb2(&data_block[offset]);
            else
              data[j][i] = from_msb2_u(&data_block[offset]);
          else
            if (conversion.signed_samples())
              data[j][i] = from_lsb2(&data_block[offset]);
            else
              data[j][i] = from_lsb2_u(&data_block[offset]);
          offset += 2;
          break;
      }
    }
}

sample *load_it_sample(ifstream *file, int i, it_created_with_tracker &cwt)
{
  bool new_format = cwt.compatible_exceeds(2.15);

  char magic[4];

  file->read(magic, 4);
  if (string(magic, 4) != "IMPS")
  {
    cerr << "Warning: sample #" << i << " does not have a valid header" << endl;
    return new sample_builtintype<signed char>(i, 1);
  }

  char dos_filename[13];
  file->read(dos_filename, 13);

  if (dos_filename[12] != 0)
  {
    dos_filename[12] = 0;
    cerr << "Warning: DOS filename not properly null-terminated in sample #" << i << endl;
  }

  int global_volume = file->get();

  it_sample_flags flags;
  flags.value = file->get();

  if (flags.sample_present() == false)
    return new sample_builtintype<signed char>(i, 1);
    
  int default_volume = file->get();

  char sample_name[27];
  file->read(sample_name, 26);
  sample_name[26] = 0;

  it_sample_conversion_flags conversion;
  conversion.value = file->get();

  char default_panning = file->get();

  unsigned long sample_length, loop_begin, loop_end, c5spd;
  unsigned long susloop_begin, susloop_end, sample_pointer;

  unsigned char lsb_bytes[4];

  file->read((char *)&lsb_bytes[0], 4);
  sample_length = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  loop_begin = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  loop_end = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  c5spd = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  susloop_begin = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  susloop_end = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  sample_pointer = from_lsb4_lu(lsb_bytes);

  char auto_vibrato_speed = file->get();
  char auto_vibrato_depth = file->get();
  char auto_vibrato_waveform_char = file->get();
  char auto_vibrato_rate = file->get();

  Waveform::Type auto_vibrato_waveform;

  switch (auto_vibrato_waveform_char)
  {
    case 0: auto_vibrato_waveform = Waveform::Sine;     break;
    case 1: auto_vibrato_waveform = Waveform::RampDown; break;
    case 2: auto_vibrato_waveform = Waveform::Square;   break;
    case 3: auto_vibrato_waveform = Waveform::Random;   break;
  }

  sample *ret;

  int channels = flags.stereo() ? 2 : 1;

  if (flags.looping() == false)
    loop_end = -1; // special value to mean 'no loop'
  if (flags.sustain_loop() == false)
    susloop_end = -1; // special value to mean 'no loop'

  file->seekg(sample_pointer);

  if (flags.compressed())
  {
    if (flags.stereo())
    {
      cerr << "Unable to load sample #" << i << ": compression on stereo samples is not defined" << endl;
      return new sample_builtintype<signed char>(i, 1);
    }

    if (flags.sixteen_bit())
    {
      signed short *data;
      try
      {
        data = load_it_sample_compressed<signed short>(file, sample_length, new_format);
      }
      catch (const char *msg)
      {
        cerr << "Unable to load sample #" << i << ": cannot decode compressed format (" << msg << ")" << endl;
        return new sample_builtintype<signed char>(i, 1);
      }

      ret = new sample_builtintype<signed short>(i, 1, &data, sample_length, loop_begin, loop_end, susloop_begin, susloop_end);
      ((sample_builtintype<signed short> *)ret)->default_volume = default_volume / 64.0;
    }
    else
    {
      signed char *data;
      try
      {
        data = load_it_sample_compressed<signed char>(file, sample_length, new_format);
      }
      catch (const char *msg)
      {
        cerr << "Unable to load sample #" << i << ": cannot decode compressed format (" << msg << ")" << endl;
        return new sample_builtintype<signed char>(i, 1);
      }

      ret = new sample_builtintype<signed char>(i, 1, &data, sample_length, loop_begin, loop_end, susloop_begin, susloop_end);
      ((sample_builtintype<signed char> *)ret)->default_volume = default_volume / 64.0;
    }
  }
  else
  {
    if (flags.sixteen_bit())
    {
      signed short *data[MAX_CHANNELS];
      load_it_sample_uncompressed<signed short>(file, channels, sample_length, conversion, data);
      ret = new sample_builtintype<signed short>(i, channels, data, sample_length, loop_begin, loop_end, susloop_begin, susloop_end);
      ((sample_builtintype<signed short> *)ret)->default_volume = default_volume / 64.0;
    }
    else
    {
      signed char *data[MAX_CHANNELS];
      load_it_sample_uncompressed<signed char>(file, channels, sample_length, conversion, data);
      ret = new sample_builtintype<signed char>(i, channels, data, sample_length, loop_begin, loop_end, susloop_begin, susloop_end);
      ((sample_builtintype<signed char> *)ret)->default_volume = default_volume / 64.0;
    }
  }

  ret->samples_per_second = c5spd / 2.0; // impulse tracker octaves go lower than we can display!

  return ret;
}

struct it_pattern_mask
{
  char value;

  bool has_note()                { return (0 != (value &   1)); }
  bool has_instrument()          { return (0 != (value &   2)); }
  bool has_volume_panning()      { return (0 != (value &   4)); }
  bool has_effect()              { return (0 != (value &   8)); }
  bool use_last_note()           { return (0 != (value &  16)); }
  bool use_last_instrument()     { return (0 != (value &  32)); }
  bool use_last_volume_panning() { return (0 != (value &  64)); }
  bool use_last_effect()         { return (0 != (value & 128)); }

  it_pattern_mask()
  {
    value = 0;
  }
};

struct it_pattern_slot
{
  int note, instrument, volume_panning;
  int effect_command, effect_data;

  it_pattern_slot()
  {
    note = instrument = volume_panning = -1;
    effect_command = effect_data = 0;
  }
};

int snote_from_inote(int inote)
{
  if (inote < 120)
  {
    int octave = inote / 12;
    int note = inote % 12;

    return (octave << 4) | note;
  }

  if (inote == 254)
    return -2;

  if (inote == 255)
    return -3;

  return -1; // oh well, just ignore it, I guess
}

const unsigned char it_pattern_slide_table[10]
  = { 0, 1, 4, 8, 16, 32, 64, 96, 128, 255 };

void load_it_pattern(ifstream *file, pattern &p, vector<sample *> &samps, bool has_note_events[MAX_MODULE_CHANNELS])
{
  unsigned char lsb_data[2];

  file->read((char *)&lsb_data[0], 2);
  int pattern_bytes = from_lsb2_u(lsb_data);

  file->read((char *)&lsb_data[0], 2);
  int pattern_rows = from_lsb2_u(lsb_data);

  file->ignore(4);

  ArrayAllocator<char> data_allocator(pattern_bytes);
  char *data = data_allocator.getArray();

  file->read(&data[0], pattern_bytes);

  it_pattern_mask masks[64];

  it_pattern_slot last_row[64], cur_row[64];

  cerr << ".";

  for (int i=0; i<pattern_rows; i++)
  {
    vector<row> rowdata(64);

    for (int j=0; j<64; j++)
    {
      cur_row[j].note = -1;
      cur_row[j].instrument = -1;
      cur_row[j].volume_panning = -1;
      cur_row[j].effect_command = -1;
    }

    while (true)
    {
      char channel_byte = *(data++);
      if (channel_byte == 0) // end of row
        break;

      int channel = (channel_byte - 1) & 63;

      it_pattern_mask &mask = masks[channel];

      if (channel_byte & 128)
        mask.value = *(data++); // else use old value

      it_pattern_slot &c = cur_row[channel], &l = last_row[channel];

      if (mask.has_note())
        c.note = (unsigned char)*(data++);
      if (mask.has_instrument())
        c.instrument = *(data++);
      if (mask.has_volume_panning())
        c.volume_panning = *(data++);
      if (mask.has_effect())
      {
        c.effect_command = (unsigned char)*(data++);
        c.effect_data = (unsigned char)*(data++);
      }
      if (mask.use_last_note())
        c.note = l.note;
      if (mask.use_last_instrument())
        c.instrument = l.instrument;
      if (mask.use_last_volume_panning())
        c.volume_panning = l.volume_panning;
      if (mask.use_last_effect())
      {
        c.effect_command = l.effect_command;
        c.effect_data = l.effect_data;
      }

      row &r = rowdata[channel];

      r.snote = snote_from_inote(c.note);
      r.znote = znote_from_snote(r.snote);

      if (r.snote >= 0)
        has_note_events[channel] = true;

      if ((c.instrument > 0) && (c.instrument <= int(samps.size())))
        r.instrument = samps[c.instrument - 1];
      else
        r.instrument = NULL;
      
      if (c.volume_panning >= 0)
      {
        if (c.volume_panning <= 64)
          r.volume = c.volume_panning;
        else if (c.volume_panning <= 74)
        {
          int param = c.volume_panning - 64;
          r.secondary_effect = effect_struct(effect_type_it, 'D', (param << 4) | 0xF);
        }
        else if (c.volume_panning <= 84)
        {
          int param = c.volume_panning - 74;
          r.secondary_effect = effect_struct(effect_type_it, 'D', param | 0xF0);
        }
        else if (c.volume_panning <= 94)
        {
          int param = c.volume_panning - 84;
          r.secondary_effect = effect_struct(effect_type_it, 'D', (param << 4));
        }
        else if (c.volume_panning <= 104)
        {
          int param = c.volume_panning - 94;
          r.secondary_effect = effect_struct(effect_type_it, 'D', param);
        }
        else if (c.volume_panning <= 114)
        {
          int param = c.volume_panning - 104;
          r.secondary_effect = effect_struct(effect_type_it, 'E', it_pattern_slide_table[param]);
        }
        else if (c.volume_panning <= 124)
        {
          int param = c.volume_panning - 114;
          r.secondary_effect = effect_struct(effect_type_it, 'F', it_pattern_slide_table[param]);
        }
        else if (c.volume_panning >= 128)
        {
          if (c.volume_panning <= 192)
          {
            int param = c.volume_panning - 128;
            r.secondary_effect = effect_struct(effect_type_it, 'X', param);
          }
          else if (c.volume_panning <= 202)
          {
            int param = c.volume_panning - 192;
            r.secondary_effect = effect_struct(effect_type_it, 'G', param);
          }
          else if (c.volume_panning <= 212)
          {
            int param = c.volume_panning - 202;
            r.secondary_effect = effect_struct(effect_type_it, 'H', param); // TODO
          }
        }
      }

      r.effect = effect_struct(effect_type_it, char(c.effect_command), c.effect_data);

      if (r.effect.present)
        has_note_events[channel] = true;
    }

    for (int i=0; i<64; i++)
    {
      if (cur_row[i].note != -1)
        last_row[i].note = cur_row[i].note;
      if (cur_row[i].instrument != -1)
        last_row[i].instrument = cur_row[i].instrument;
      if (cur_row[i].volume_panning != -1)
        last_row[i].volume_panning = cur_row[i].volume_panning;
      if (cur_row[i].effect_command != -1)
        last_row[i].effect_command = cur_row[i].effect_command,
        last_row[i].effect_data = cur_row[i].effect_data;
    }

    p.row_list.push_back(rowdata);
  }
}

void load_it_convert_envelope(instrument_envelope &target, it_envelope_description &source)
{
  target.enabled = source.enabled;

  if (target.enabled)
  {
    target.looping = source.looping;
    target.loop_begin_tick = source.envelope[source.loop_start_node].tick;
    target.loop_end_tick = source.envelope[source.loop_end_node].tick;

    target.sustain_loop = source.sustain_loop;
    target.sustain_loop_begin_tick = source.envelope[source.sustain_loop_start_node].tick;
    target.sustain_loop_end_tick = source.envelope[source.sustain_loop_end_node].tick;

    for (int i=0; i < int(source.envelope.size()); i++)
      target.node.push_back(instrument_envelope_node(source.envelope[i].tick, source.envelope[i].yValue));
  }
}

module_struct *load_it(ifstream *file, bool modplug_style = false)
{       // 12345678901234567890123456789012345678901234567890123456789012345678901234567890
  cerr << "Sorry the IT loader takes so long! It's something to do with pattern loading," << endl
       << "but I'm not sure what exactly the bottleneck is. I made a progress indicator to" << endl
       << "distract you from the wait :-)" << endl << endl;

  char magic[4];
  file->read(magic, 4);

  if (string(magic, 4) != "IMPM")
    throw "invalid file format (missing 'IMPM' at start of file)";

  char songname[27];
  file->read(songname, 26);
  songname[26] = 0;

  file->ignore(2); // pattern highlight info

  unsigned char lsb_bytes[4];

  file->read((char *)&lsb_bytes[0], 2);
  int order_list_length = from_lsb2_u(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 2);
  int num_instruments = from_lsb2_u(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 2);
  int num_samples = from_lsb2_u(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 2);
  int num_patterns = from_lsb2_u(lsb_bytes);

  it_created_with_tracker cwt;

  file->read((char *)&lsb_bytes[0], 4);
  cwt.major_version = lsb_bytes[1];
  cwt.minor_version = lsb_bytes[0];
  cwt.compatible_with_major_version = lsb_bytes[3];
  cwt.compatible_with_minor_version = lsb_bytes[2];

  it_flags flags;

  file->read(&flags.value, 1);
  file->ignore(1);

  file->read((char *)&lsb_bytes[0], 2);
  int special = from_lsb2_u(lsb_bytes);

  it_settings settings;

  file->read((char *)&settings.global_volume, 1);
  file->read((char *)&settings.mix_volume, 1);
  file->read((char *)&settings.initial_speed, 1);
  file->read((char *)&settings.initial_tempo, 1);

  unsigned char panning_separation;
  file->read((char *)&panning_separation, 1);

  unsigned char midi_pitch_wheel_depth;
  file->read((char *)&midi_pitch_wheel_depth, 1);

  file->read((char *)&lsb_bytes[0], 2);
  int message_length = from_lsb2_u(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  unsigned long message_offset = from_lsb4_lu(lsb_bytes);

  file->read((char *)&lsb_bytes[0], 4);
  unsigned long reserved = from_lsb4_lu(lsb_bytes);

  unsigned char initial_channel_pan[64];
  unsigned char initial_channel_volume[64];

  file->read((char *)&initial_channel_pan[0], 64);
  file->read((char *)&initial_channel_volume[0], 64);

  vector<unsigned char> order_table(order_list_length);

  for (int i=0; i<order_list_length; i++)
  {
    unsigned char this_order;

    file->read((char *)&this_order, 1);
    order_table[i] = this_order;
  }
  
  vector<unsigned long> instrument_offset(num_instruments);
  vector<unsigned long> sample_header_offset(num_samples);
  vector<unsigned long> pattern_offset(num_patterns);

  for (int i=0; i<num_instruments; i++)
  {
    file->read((char *)&lsb_bytes[0], 4);
    instrument_offset[i] = from_lsb4_lu(lsb_bytes);
  }
  for (int i=0; i<num_samples; i++)
  {
    file->read((char *)&lsb_bytes[0], 4);
    sample_header_offset[i] = from_lsb4_lu(lsb_bytes);
  }
  for (int i=0; i<num_patterns; i++)
  {
    file->read((char *)&lsb_bytes[0], 4);
    pattern_offset[i] = from_lsb4_lu(lsb_bytes);
  }

  {
    long expected_offset = 0xC0 + order_list_length + 4 * (num_instruments + num_samples + num_patterns);
    long actual_offset = file->tellg();

    if (expected_offset != actual_offset)
      throw "internal error (file offset is not what it should be)";
  }

  vector<it_instrument_description> insts;

  for (int i=0; i<num_instruments; i++)
  {
    file->seekg(instrument_offset[i]);

    it_instrument_description desc;
    if (cwt.compatible_with_major_version < 2)
      load_it_old_instrument(file, desc, i);
    else
      load_it_new_instrument(file, desc, i);
    insts.push_back(desc);
  }

  vector<sample *> samps;

  for (int i=0; i<num_samples; i++)
  {
    file->seekg(sample_header_offset[i]);
    sample *smp = load_it_sample(file, i, cwt);
    samps.push_back(smp);
  }

  int channels = flags.stereo() ? 2 : 1;

  if (flags.use_instruments()) // this conversion MUST be done before loading patterns
  {
    vector<sample *> instruments;

    for (int i=0; i<num_instruments; i++)
    {
      it_instrument_description &id = insts[i];
      
      sample_instrument *is = new sample_instrument(i);

      is->global_volume = id.global_volume;
      is->default_pan.set_channels(channels).from_linear_pan(id.default_pan, 0, 64);
      is->use_default_pan = id.use_default_pan;

      is->pitch_pan_center = id.pitch_pan_center;
      is->pitch_pan_separation = id.pitch_pan_separation;

      is->volume_variation_pctg = id.volume_variation_pctg;
      is->panning_variation = id.panning_variation;

      is->fade_out = id.fade_out;

      is->duplicate_note_check = id.duplicate_note_check;
      is->duplicate_check_action = id.duplicate_check_action;
      is->new_note_action = id.new_note_action;

      for (int i=0; i<120; i++)
      {
        is->tone_offset[i] = id.tone_offset[i];
        if (id.note_sample[i])
          is->note_sample[i] = samps[id.note_sample[i] - 1];
        else
          is->note_sample[i] = NULL;
      }

      load_it_convert_envelope(is->volume_envelope, id.volume_envelope);
      load_it_convert_envelope(is->panning_envelope, id.pan_envelope);
      load_it_convert_envelope(is->pitch_envelope, id.pitch_envelope);

      instruments.push_back(is);
    }

    samps = instruments;
  }

  vector<pattern> pats;
  bool has_note_events[MAX_MODULE_CHANNELS] = { false }; // entire struct to 0

  cerr << "Loading " << num_patterns << " patterns" << endl;
  cerr << '.' << string(num_patterns, '-') << '.' << endl << ' ';
  for (int i=0; i<num_patterns; i++)
  {
    file->seekg(pattern_offset[i]);

    pattern pat;
    load_it_pattern(file, pat, samps, has_note_events);
    pats.push_back(pat);
  }
  cerr << endl << endl;

  module_struct *ret = new module_struct();

  ret->name = songname;

  ret->stereo = flags.stereo();

  ret->patterns = pats;
  ret->samples = samps;

  for (int i=0; i<order_list_length; i++)
  {
    if (order_table[i] == 254) //  ++ (skip)
    {
      ret->pattern_list.push_back(NULL);
      continue;
    }
    if (order_table[i] == 255) //  -- (end of tune)
      break;
    ret->pattern_list.push_back(&ret->patterns[order_table[i]]);
  }

  memset(ret->base_pan, 0, sizeof(ret->base_pan));

  int count = 0;

  for (int i=0; i<64; i++)
  {
    if (has_note_events[i])
    {
      count++;
      ret->channel_enabled[i] = true;
      ret->channel_map[i] = i;
      if (ret->stereo)
      {
        ret->initial_panning[i].set_channels(2);

        ret->base_pan[i] = initial_channel_pan[i];
        ret->initial_panning[i].from_amiga_pan(ret->base_pan[i]);
      }
    }
    else
      ret->channel_enabled[i] = false;
  }
  ret->num_channels = count;

  ret->speed = settings.initial_speed;
  ret->tempo = settings.initial_tempo;

  ret->it_module = true;
  ret->it_module_new_effects = !flags.old_effects();
  ret->it_module_portamento_link = flags.portamento_memory_link();
  ret->it_module_linear_slides = flags.linear_slides();

  return ret;
}
