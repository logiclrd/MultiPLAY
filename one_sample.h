#define MAX_CHANNELS 2

struct pan_value
{
  double sample_scale[MAX_CHANNELS];
  int num_scales;

  pan_value(int channels=1)
  {
    num_scales = channels;
    for (int i=0; i<channels; i++)
      sample_scale[i] = 1.0;
  }

  pan_value(double position)
  {
    num_scales = 2;

    if (position > 0)
      sample_scale[0] = exp(-position);
    else
      sample_scale[0] = 1.0;

    if (position < 0)
      sample_scale[1] = exp(position);
    else
      sample_scale[1] = 1.0;

    for (int i=2; i<MAX_CHANNELS; i++)
      sample_scale[i] = 0.0;
  }

  inline void normalize()
  {
    double max_weight = -1.0;

    for (int i=0; i<num_scales; i++)
      if (sample_scale[i] > max_weight)
        max_weight = sample_scale[i];
    for (int i=0; i<num_scales; i++)
      sample_scale[i] /= max_weight;
  }

  template <class T>
  inline void from_linear_pan(T pan_value, T lower_limit, T upper_limit)
  {
    if (num_scales == 1)
      sample_scale[0] = 1.0;
    else
    {
      double middle     = (upper_limit + lower_limit) / 2.0;
      double half_range = (upper_limit - lower_limit) / 2.0;

      if (pan_value > middle)
        sample_scale[0] = exp((middle - pan_value) / half_range);
      else
        sample_scale[0] = 1.0;

      if (pan_value < middle)
        sample_scale[1] = exp((pan_value - middle) / half_range);
      else
        sample_scale[1] = 1.0;

      for (int i=2; i<num_scales; i++)
        sample_scale[i] = 0.0;
    }
  }

  template <class T>
  inline T to_linear_pan(T lower_limit, T upper_limit)
  {
    pan_value tmp = *this;

    tmp.normalize();

    double value;

    if (tmp.sample_scale[0] < 1.0)
      value = -log(tmp.sample_scale[0]);
    else
      value = log(tmp.sample_scale[1]);

    double middle     = (upper_limit + lower_limit) / 2.0;
    double half_range = (upper_limit - lower_limit) / 2.0;

    return (T)(middle + half_range * value);
  }

  inline void from_s3m_pan(int pan_value)
  { // pan_value 0 == left, pan_value 15 == right
    if (num_scales == 1)
      sample_scale[0] = 1.0;
    else
    {
      if (pan_value > 7)
        sample_scale[0] = exp((8 - pan_value) / 8.0);
      else
        sample_scale[0] = 1.0;
      if (pan_value < 8)
        sample_scale[1] = exp((pan_value - 8) / 8.0);
      else
        sample_scale[1] = 1.0;
      for (int i=2; i<num_scales; i++)
        sample_scale[i] = 0.0;
    }
  }

  inline void from_amiga_pan(int pan_value)
  {
    if (pan_value > 0x80)
    {
      if (pan_value == 0xA4)
      {
        sample_scale[0] = 1.0;
        if (num_scales > 0)
        {
          sample_scale[1] = -1.0;
          for (int i=2; i<num_scales; i++)
            sample_scale[i] = 0.0;
        }
      }
      else
        cerr << "Amiga pan of value "
          << setfill('0') << setw(2) << hex << uppercase << pan_value << nouppercase << dec
          << " is not defined" << endl;
    }
    else if (num_scales == 0)
      sample_scale[1] = 1.0;
    else
    {
      if (pan_value <= 0x40)
      {
        sample_scale[0] = 1.0;
        sample_scale[1] = exp((pan_value - 64.0) / 64.0);
      }
      else if (pan_value > 0x40)
      {
        sample_scale[0] = exp((64.0 - pan_value) / 64.0);
        sample_scale[1] = 1.0;
      }
    }
  }

  inline void from_mod_pan(int pan_value)
  {
    if (pan_value < 0x80)
    {
      sample_scale[0] = 1.0;
      sample_scale[1] = exp((pan_value - 127.5) / 127.5);
    }
    else if (pan_value > 0x7F)
    {
      sample_scale[0] = exp((127.5 - pan_value) / 127.5);
      sample_scale[1] = 1.0;
    }
  }

  inline void cross(double crosstalk_proportion)
  {
    double total_weight = 0.0;

    for (int i=0; i<num_scales; i++)
      total_weight += sample_scale[i];

    for (int i=0; i<num_scales; i++)
      sample_scale[i] += (total_weight - sample_scale[i]) * crosstalk_proportion;
  }

  inline pan_value &set_channels(int new_num_channels)
  {
    if (new_num_channels == 1)
    {
      if (num_scales == 1)
        return *this;

      for (int i=1; i<num_scales; i++)
        sample_scale[0] += sample_scale[i];

      sample_scale[0] /= num_scales;
      num_scales = 1;
    }
    else
    {
      if (num_scales == new_num_channels)
        return *this;

      if (num_scales != 1)
        set_channels(1);

      for (int i=1; i<new_num_channels; i++)
        sample_scale[i] = sample_scale[0];

      num_scales = new_num_channels;
    }

    return *this;
  }
};

struct one_sample
{
  double sample[MAX_CHANNELS];
  int num_samples, cur_sample;

  inline one_sample(int channels)
  {
    if (channels > MAX_CHANNELS)
      throw "Too many channels in one_sample initialization";

    num_samples = channels;
    while (channels--)
      sample[channels] = 0.0;
  }

  inline double &next_sample()
  {
    return sample[cur_sample++];
  }

  inline bool has_next()
  {
    return (cur_sample < num_samples);
  }

  inline void pan(pan_value &panning, double sample_base)
  {
    for (int i=0; i<num_samples; i++)
      sample[i] = sample_base * panning.sample_scale[i];
  }

  inline void clear()
  {
    for (int i=0; i<num_samples; i++)
      sample[i] = 0.0;
  }

  inline void clear(int num_channels)
  {
    num_samples = num_channels;
    for (int i=0; i<num_samples; i++)
      sample[i] = 0.0;
  }

  inline void reset()
  {
    cur_sample = 0;
  }

  inline one_sample &set_channels(int new_num_channels)
  {
    if (new_num_channels == 1)
    {
      if (num_samples == 1)
        return *this;

      for (int i=1; i<num_samples; i++)
        sample[0] += sample[i];

      sample[0] /= num_samples;
      num_samples = 1;
    }
    else
    {
      if (num_samples == new_num_channels)
        return *this;

      if (num_samples != 1)
        set_channels(1);

      for (int i=1; i<new_num_channels; i++)
        sample[i] = sample[0];

      num_samples = new_num_channels;
    }

    return *this;
  }
  
  inline one_sample &scale(double scale_value)
  {
    return *this *= scale_value;;
  }

  inline one_sample &operator +=(const one_sample &other)
  {
    if (num_samples != other.num_samples)
      throw "Trying to add a sample with an incorrect number of channels";

    for (int i=0; i<num_samples; i++)
      sample[i] += other.sample[i];

    return *this;
  }

  inline one_sample &operator *=(double scale)
  {
    for (int i=0; i<num_samples; i++)
      sample[i] *= scale;

    return *this;
  }

  inline one_sample &operator *=(const pan_value &other)
  {
    if (num_samples != other.num_scales)
      throw "Trying to pan a sample with an incorrect number of channels";

    for (int i=0; i<num_samples; i++)
      sample[i] *= other.sample_scale[i];

    return *this;
  }

  inline one_sample operator *(double scale) const
  {
    one_sample ret(num_samples);

    for (int i=0; i<num_samples; i++)
      ret.sample[i] = sample[i] * scale;

    return ret;
  }

  inline one_sample &operator =(double sample_value)
  {
    for (int i=0; i<num_samples; i++)
      sample[i] = sample_value;

    return *this;
  }
};

inline one_sample operator *(double scale, const one_sample &other)
{
  one_sample ret(other.num_samples);

  for (int i=0; i<other.num_samples; i++)
    ret.sample[i] = other.sample[i] * scale;

  return ret;
}

inline one_sample operator *(double sample_value, const pan_value &pan)
{
  one_sample ret(pan.num_scales);

  for (int i=0; i<pan.num_scales; i++)
    ret.sample[i] = pan.sample_scale[i] * sample_value;
  
  return ret;
}

inline one_sample operator *(const pan_value &pan, double sample_value)
{
  one_sample ret(pan.num_scales);

  for (int i=0; i<pan.num_scales; i++)
    ret.sample[i] = pan.sample_scale[i] * sample_value;

  return ret;
}

inline one_sample operator *(const pan_value &pan, const one_sample &sample)
{
  one_sample ret(sample);

  if (ret.num_samples != pan.num_scales)
    ret.set_channels(pan.num_scales);

  for (int i=0; i<pan.num_scales; i++)
    ret.sample[i] = pan.sample_scale[i] * ret.sample[i];
  
  return ret;
}