struct channel_DYNAMIC : channel
{
  channel &parent_channel;

  virtual bool advance_pattern(one_sample &sample)
  {
    ticks_left = 1; // always have ticks left
    rest_ticks = 0;
    dropoff_start = 0;
    finished = finished
            || (fading && (fade_value <= 0.0))
            || (current_sample == NULL)
            || current_sample->past_end(offset_major, offset, current_sample_context);
    return false;
  }

  channel_DYNAMIC(channel &spawner, sample *sample, sample_context *context, double fade_per_tick)
    : channel(spawner.panning, false),
      parent_channel(spawner)
  {
    this->current_waveform = Waveform::Sample;
    this->current_sample = sample;
    this->current_sample_context = context;
    this->note_frequency = spawner.note_frequency;
    this->delta_offset_per_tick = spawner.note_frequency / ticks_per_second;
    this->intensity = spawner.intensity;
    this->fade_per_tick = fade_per_tick;
    this->have_fade_per_tick = true;
    this->offset_major = spawner.offset_major;
    this->offset = spawner.offset;
    this->samples_this_note = spawner.samples_this_note;
  }

  ~channel_DYNAMIC()
  {
    iter_t parent_position = find(parent_channel.my_ancillary_channels.begin(), parent_channel.my_ancillary_channels.end(), this);
    if (parent_position != parent_channel.my_ancillary_channels.end())
      parent_channel.my_ancillary_channels.erase(parent_position);
  }
};
