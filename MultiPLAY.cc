#include <functional>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <map>

using namespace std;

#include <cctype>
#include <cmath>

#include "RAII.h"

using namespace RAII;

#include "one_sample.h"

#ifdef DIRECTX
#include "Output-DirectX.h"
#endif

#ifdef SDL
#include "Output-SDL.h"
#endif

volatile bool shutting_down = false, shutdown_complete = false;

void start_shutdown()
{
  shutting_down = true;
}

#ifdef WIN32
#include "win32_break_handler.h"
#else
#include "unix_break_handler.h"
#endif

const double mod_finetune[16] = {
  7895.0, 7941.0, 7985.0, 8046.0, 8107.0, 8169.0, 8232.0, 8280.0,
  8363.0, 8413.0, 8463.0, 8529.0, 8581.0, 8651.0, 8723.0, 8757.0 };
/* The above were provided by some tutorial, but I don't think they're right. */

// These were calculated according to one finetune step being 1/8 of a
// semitone. Thus, the first entry is one semitone lower than the 8th
// entry.
/*
const double mod_finetune[16] = {
  7893.646721,
  7950.844085,
  8008.455902,
  8066.485173,
  8124.934925,
  8183.808203,
  8243.108077,
  8302.837638,
  8363.000000,
  8423.598298,
  8484.635691,
  8546.115361,
  8608.040513,
  8670.414375,
  8733.240198,
  8796.521256 };/**/

namespace Waveform
{
  enum Type
  {
    Sine,     // ._.·'¯'·._.·'¯'·._.

    Square,   // _|¯|_|¯|_|¯|_|¯|_|¯

    Sawtooth, // /|/|/|/|/|/|/|/|/|/
    
    RampDown, // \|\|\|\|\|\|\|\|\|\

    Triangle, // /\/\/\/\/\/\/\/\/\/

    Sample,

    Random,   // crazy all over the place (changes at arbitrary intervals)
  };
}

const Waveform::Type default_waveform = Waveform::Triangle;

const double dropoff_min_length = 30.0 / 44100.0;
const double dropoff_max_length = 70.0 / 44100.0;
const double dropoff_proportion = 1.0 / (dropoff_min_length + dropoff_max_length);

//                           G# A#     C# D#    F# G#
//                             A   B  C  D  E  F  G
const int note_by_name[7] = {  9, 11, 0, 2, 4, 5, 7  };

#define LOG2 0.6931471805599453

double lg(double x)
{
  return log(x) * (1.0 / LOG2);
}

double p2(double x)
{
  return exp(LOG2 * x);
}

int output_channels;

int expect_int(istream *in)
{
  int built_up = 0;

  while (true)
  {
    int ch = in->get();

    if (ch < 0)
      return built_up;

    if ((ch >= '0') && (ch <= '9'))
      built_up = (built_up * 10) + (ch - '0');
    else
    {
      in->putback(ch);
      return built_up;
    }
  }
}

int accidental(istream *in)
{
  int ch = in->get();

  if (ch == '-')
    return -1;
  if ((ch == '+') || (ch == '#'))
    return +1;

  in->putback(ch);
  return 0;
}

double expect_duration(istream *in, int &note_length_denominator)
{
  double duration = 1.0;
  double duration_add = 0.5;

  int ch = in->get();

  if (isdigit(ch) && (&note_length_denominator != NULL))
  {
    in->putback(ch);
    note_length_denominator = expect_int(in);
  }

  while (ch == '.')
  {
    duration += duration_add;
    duration_add /= 2.0;

    ch = in->get();
  }

  if (ch >= 0)
    in->putback(ch);

  return duration;
}

int ticks_per_second;
double inter_note = p2(1.0 / 12.0);

int from_lsb2(unsigned char in[2])
{
  return in[0] | (static_cast<signed char>(in[1]) << 8);
}

unsigned int from_lsb2_u(unsigned char in[2])
{
  return in[0] | (in[1] << 8);
}

int from_lsb4(unsigned char in[4])
{
  return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
}

long from_lsb4_l(unsigned char in[4])
{
  return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
}

unsigned long from_lsb4_lu(unsigned char in[4])
{
  return in[0] | (in[1] << 8) | (in[2] << 16) | (in[3] << 24);
}

int from_msb2(unsigned char in[2])
{
  return in[1] | (static_cast<signed char>(in[0]) << 8);
}

unsigned int from_msb2_u(unsigned char in[2])
{
  return in[1] | (in[0] << 8);
}

int from_msb4(unsigned char in[4])
{
  return in[3] | (in[2] << 8) | (in[1] << 16) | (in[0] << 24);
}

namespace WAV
{
  struct raw_chunk
  {
    char chunkID[4];
    unsigned long chunkSize;

    char chunkData[1];
  };

  struct format_chunk
  {
    char chunkID[4];
    unsigned long chunkSize;

    short wFormatTag;
    unsigned short wChannels;
    unsigned long dwSamplesPerSec;
    unsigned long dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;
  };

  union chunk
  {
    raw_chunk raw;

    string chunkID()
    {
      return string((char *)raw.chunkID, 4);
    }

    format_chunk format;
  };
}

struct ieee_extended
{
  char data[10];

  double toDouble()
  {
    int sign = data[0] & 128;
    double mantissa = 0.0;
    double add = 1.0;

    int exponent = ((data[1]) | ((data[0] & 0x7F) << 8)) - 16383;

    for (int i=2; i<10; i++)
    {
      for (int j=128; j >= 1; j >>= 1)
      {
        if (data[i] & j)
          mantissa += add;
        add /= 2.0;
      }
    }

    if (sign)
      return mantissa * -p2(exponent);
    else
      return mantissa * p2(exponent);
  }
};

namespace AIFF
{
  struct raw_chunk
  {
    char chunkID[4];
    unsigned long chunkSize;

    char chunkData[1];
  };

  struct common_chunk
  {
    char chunkID[4];
    unsigned long chunkSize;

    short numChannels;
    unsigned long numSampleFrames;
    short sampleSize;
    ieee_extended sampleRate;
  };

  struct ssnd_chunk
  {
    char chunkID[4];
    unsigned long chunkSize;

    unsigned long offset;
    unsigned long blockSize;
    char waveformData[1];
  };

  union chunk
  {
    raw_chunk raw;

    string chunkID()
    {
      return string((char *)raw.chunkID, 4);
    }

    common_chunk common;
    ssnd_chunk ssnd;
  };
}

struct row;
struct channel;
struct sample;

struct sample_context
{
  sample *created_with;
  double samples_per_second;
  double default_volume;
  int num_samples;

  sample_context(sample *cw) : created_with(cw) { }
};

struct sample
{
  int index;

  int fade_out;

  int num_samples;
  double samples_per_second;

  bool use_vibrato;
  double vibrato_depth, vibrato_cycle_frequency; // frequency relative to samples with samples_per_second per second

  virtual one_sample get_sample(int sample, double offset, sample_context *c = NULL) = 0;
  virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **c = NULL, double effect_tick_length = 0.0, bool top_level = true, int *znote = NULL) = 0;
  virtual void occlude_note(channel *p = NULL, sample_context **c = NULL, sample *new_sample = NULL, row *r = NULL) = 0;
  virtual void exit_sustain_loop(sample_context *c = NULL) = 0; //..if it has one :-)
  virtual void kill_note(sample_context *c = NULL) = 0;
  virtual bool past_end(int sample, double offset, sample_context *c = NULL) = 0;

  sample(int idx)
    : use_vibrato(false), index(idx + 1)
  {
  }
};

inline double bilinear(double left, double right, double offset)
{
  return left * (1.0 - offset) + right * offset; // bilinear interpolation
}

namespace SustainLoopState
{
  enum Type
  {
    Off, Running, Finishing, Finished,
  };
}

struct sample_builtintype_context : sample_context
{
  int last_looped_sample;

  long sustain_loop_exit_difference;

  SustainLoopState::Type sustain_loop_state;

  sample_builtintype_context(sample *cw) : sample_context(cw) { }
};

template <class T>
struct sample_builtintype : public sample
{
  T *sample_data[MAX_CHANNELS];
  int sample_channels;

  double sample_scale;
  double default_volume;

  long loop_begin, loop_end;
  long sustain_loop_begin, sustain_loop_end;

  bool use_sustain_loop;

  virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **c = NULL, double effect_tick_length = 0, bool top_level = true, int *znote = NULL)
  {
    if (c == NULL)
      throw "need sample context";

    if (*c)
    {
      (*c)->created_with->occlude_note(p, c, this, r);

      if (*c)
        delete *c;
    }

    *c = new sample_builtintype_context(this);

    sample_builtintype_context &context = *(sample_builtintype_context *)*c;

    if (use_sustain_loop) // reset sustain loop
      context.sustain_loop_state = SustainLoopState::Running;
    else
      context.sustain_loop_state = SustainLoopState::Off;

    context.last_looped_sample = 0;
    context.sustain_loop_exit_difference = 0;
    context.samples_per_second = samples_per_second;
    context.default_volume = default_volume;
    context.num_samples = num_samples;

    if (p)
    {
      if (top_level)
      {
        p->volume_envelope = NULL;
        p->panning_envelope = NULL;
        p->pitch_envelope = NULL;
      }
      p->samples_this_note = 0;
    }
  }

  virtual void occlude_note(channel *p = NULL, sample_context **c = NULL, sample *new_sample = NULL, row *r = NULL)
  { // do nothing
  }

  virtual void exit_sustain_loop(sample_context *c)
  {
    sample_builtintype_context &context = *(sample_builtintype_context *)c;
    if (context.sustain_loop_state == SustainLoopState::Running)
      context.sustain_loop_state = SustainLoopState::Finishing;
  }

  virtual void kill_note(sample_context *c = NULL)
  {
    // no implementation required
  }

  virtual bool past_end(int sample, double offset, sample_context *c = NULL)
  {
    if (c == NULL)
      throw "need context for instrument";

    if (loop_end != 0xFFFFFFFF)
      return false;

    sample_builtintype_context &context = *(sample_builtintype_context *)c;
    switch (context.sustain_loop_state)
    {
      case SustainLoopState::Running:
      case SustainLoopState::Finishing: return false;
    }

    if (context.sustain_loop_state == SustainLoopState::Finished)
      sample -= context.sustain_loop_exit_difference;

    return (sample >= num_samples);
  }

  virtual one_sample get_sample(int sample, double offset, sample_context *c = NULL)
  {
    if (c == NULL)
      throw "need a sample context";

    sample_builtintype_context &context = *(sample_builtintype_context *)c;

    bool using_sustain_loop = false;

    if ((sample < 0)
     || ((sample >= num_samples)
      && (loop_end == 0xFFFFFFFF)
      && (!use_sustain_loop)))
      return one_sample(output_channels);

    switch (context.sustain_loop_state)
    {
      case SustainLoopState::Running:
      case SustainLoopState::Finishing:
        int new_sample;

        if (sample > sustain_loop_end)
          new_sample = ((sample - sustain_loop_begin) % (sustain_loop_end - sustain_loop_begin + 1)) + sustain_loop_begin;
        else
          new_sample = sample;

        using_sustain_loop = true;

        if (context.sustain_loop_state == SustainLoopState::Finishing)
        {
          if (new_sample < context.last_looped_sample)
          {
            new_sample += (sustain_loop_end - sustain_loop_begin + 1);
            context.sustain_loop_state = SustainLoopState::Finished;
            context.sustain_loop_exit_difference = sample - new_sample;
          }
          context.last_looped_sample = new_sample;
        }

        sample = new_sample;            

        if (context.sustain_loop_state != SustainLoopState::Finished)
          break;

        using_sustain_loop = false;
        sample += context.sustain_loop_exit_difference; // compensate for fall through 'finished'
      case SustainLoopState::Finished:
        sample -= context.sustain_loop_exit_difference; // intentionally fall through
      case SustainLoopState::Off:
        if ((sample > loop_end) && (loop_end != 0xFFFFFFFF))
          sample = ((sample - loop_begin) % (loop_end - loop_begin + 1)) + loop_begin;
        break;
    }

    double before, after;

    one_sample ret(sample_channels);

    ret.reset();

    if (using_sustain_loop)
      for (int i=0; i<sample_channels; i++)
      {
        before = sample_data[i][sample];
        if ((sample + 1 < num_samples) && (sample < sustain_loop_end))
          after = sample_data[i][sample + 1];
        else if (sample == loop_end)
          after = sample_data[i][sustain_loop_begin];
        else
          after = 0.0;

        ret.next_sample() = bilinear(before, after, offset);
      }
    else if (loop_end == 0xFFFFFFFF)
      for (int i=0; i<sample_channels; i++)
      {
        before = sample_data[i][sample];
        if (sample + 1 < num_samples)
          after = sample_data[i][sample + 1];
        else
          after = 0.0;

        ret.next_sample() = bilinear(before, after, offset);
      }
    else
      for (int i=0; i<sample_channels; i++)
      {
        before = sample_data[i][sample];
        if ((sample + 1 < num_samples) && (sample < loop_end))
          after = sample_data[i][sample + 1];
        else if (sample == loop_end)
          after = sample_data[i][loop_begin];
        else
          after = 0.0;

        ret.next_sample() = bilinear(before, after, offset);
      }

    return ret.scale(sample_scale).set_channels(output_channels);
  }

  sample_builtintype(int index, int sample_channels,
                     T **data = NULL, int num_samples = 0,
                     long loop_begin = 0, long loop_end = 0xFFFFFFFF,
                     long susloop_begin = 0, long susloop_end = 0xFFFFFFFF)
    : sample(index)
  {
    switch (sizeof(T))
    {
      case 1: sample_scale = 1.0 / 127.5;        break;
      case 2: sample_scale = 1.0 / 32767.5;      break;
      case 4: sample_scale = 1.0 / 2147483647.5; break;
      default: throw "could not deduce sample scale";
    }

    this->sample_channels = sample_channels;

    if (data)
      for (int i=0; i<sample_channels; i++)
        this->sample_data[i] = data[i];
    else
      for (int i=0; i<sample_channels; i++)
        this->sample_data[i] = NULL;

    this->num_samples = num_samples;

    this->loop_begin = loop_begin;
    this->loop_end = loop_end;

    this->sustain_loop_begin = susloop_begin;
    this->sustain_loop_end = susloop_end;

    use_sustain_loop = (susloop_end != 0xFFFFFFFF);
  }
};

enum sample_filetype
{
  sample_filetype_unknown,
  sample_filetype_WAV,
  sample_filetype_AIFF,
  sample_filetype_RAW,
};

sample *sample_from_file(ifstream *file, sample_filetype filetype_hint = sample_filetype_unknown)
{
  char magic[5];

  magic[4] = 0;

  file->read(magic, 4);

  switch (filetype_hint)
  {
    case sample_filetype_WAV:
      if (string(magic) != "RIFF")
      {
        cerr << "Warning: WAV file does not have the expected RIFF header" << endl;
        memcpy(magic, "RIFF", 4);
      }
      break;
    case sample_filetype_AIFF:
      if (string(magic) != "FORM")
      {
        cerr << "Warning: AIFF file does not have the expected FORM header" << endl;
        memcpy(magic, "FORM", 4);
      }
      break;
  }

  if ((filetype_hint != sample_filetype_RAW) && (string(magic) == "RIFF")) // then it's a WAV format file
  {
    unsigned char lsb_bytes[4];

    file->read((char *)&lsb_bytes[0], 4);
    unsigned long file_length = from_lsb4_lu(lsb_bytes); // not including 8-byte RIFF header

    file->read((char *)magic, 4);
    if (string((char *)magic, 4) == "WAVE") // then it's a WAV audio file
    {
      WAV::chunk chunk;
      WAV::format_chunk format_chunk;
      bool have_format_chunk = false;

      while (true)
      {
        file->read(chunk.raw.chunkID, 4);
        file->read((char *)&lsb_bytes[0], 4);
        chunk.raw.chunkSize = from_lsb4(lsb_bytes);

        if (!file->good())
          throw "unexpected end-of-file";

        if (chunk.chunkID() == "fmt ")
        {
          if (chunk.raw.chunkSize > sizeof(chunk))
          {
            file->ignore(chunk.raw.chunkSize);
            continue;
          }

          file->read((char *)&lsb_bytes[0], 2);
          chunk.format.wFormatTag = from_lsb2(lsb_bytes);

          file->read((char *)&lsb_bytes[0], 2);
          chunk.format.wChannels = from_lsb2_u(lsb_bytes);

          file->read((char *)&lsb_bytes[0], 4);
          chunk.format.dwSamplesPerSec = from_lsb4_lu(lsb_bytes);

          file->read((char *)&lsb_bytes[0], 4);
          chunk.format.dwAvgBytesPerSec = from_lsb4_lu(lsb_bytes);

          file->read((char *)&lsb_bytes[0], 2);
          chunk.format.wBlockAlign = from_lsb2_u(lsb_bytes);

          file->read((char *)&lsb_bytes[0], 2);
          chunk.format.wBitsPerSample = from_lsb2_u(lsb_bytes);

          format_chunk = chunk.format;
          have_format_chunk = true;
        }

        if (!file->good())
          throw "unexpected end-of-file";

        if (chunk.chunkID() == "data")
        {
          if (!have_format_chunk)
            throw "did not find a valid fmt chunk before data";

          if (format_chunk.wFormatTag != 1)
            throw "unknown file encoding";

          if (format_chunk.wBitsPerSample <= 8)
          {
            sample_builtintype<signed char> *smp;

            smp = new sample_builtintype<signed char>(0, format_chunk.wChannels);

            smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
            smp->samples_per_second = format_chunk.dwSamplesPerSec;
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed char[smp->num_samples];

            int padding = format_chunk.wBlockAlign - format_chunk.wChannels;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<smp->sample_channels; j++)
              {
                signed char sample = 0;
                sample = (signed char)(int((unsigned char)file->get()) - 128);
                smp->sample_data[j][i] = sample;
              }
              if (padding)
                file->ignore(padding);
            }

            return smp;
          }

          if (format_chunk.wBitsPerSample <= 16)
          {
            sample_builtintype<signed short> *smp;

            smp = new sample_builtintype<signed short>(0, format_chunk.wChannels);

            smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
            smp->samples_per_second = format_chunk.dwSamplesPerSec;
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed short[smp->num_samples];

            int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 2;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<smp->sample_channels; j++)
              {
                file->read((char *)&lsb_bytes[0], 2);
                smp->sample_data[j][i] = from_lsb2(lsb_bytes);
              }
              if (padding)
                file->ignore(padding);
            }

            return smp;
          }

          if (format_chunk.wBitsPerSample <= 24)
          {
            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, format_chunk.wChannels);

            smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
            smp->samples_per_second = format_chunk.dwSamplesPerSec;
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 3;

            lsb_bytes[0] = 0;
            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<smp->sample_channels; j++)
              {
                file->read((char *)&lsb_bytes[1], 3);
                smp->sample_data[j][i] = from_lsb4_l(lsb_bytes);
              }
              if (padding)
                file->ignore(padding);
            }

            return smp;
          }

          if (format_chunk.wBitsPerSample <= 32)
          {
            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, format_chunk.wChannels);

            smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
            smp->samples_per_second = format_chunk.dwSamplesPerSec;
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 4;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<format_chunk.wChannels; j++)
              {
                file->read((char *)&lsb_bytes[0], 4);
                smp->sample_data[j][i] = from_lsb4_l(lsb_bytes);
              }
              if (padding)
                file->ignore(padding);
            }

            return smp;
          }

          if (format_chunk.wBitsPerSample > 32)
          {
            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, format_chunk.wChannels);

            smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
            smp->samples_per_second = format_chunk.dwSamplesPerSec;
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 4;
            int differential = (format_chunk.wBitsPerSample - 25) / 8;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<smp->sample_channels; j++)
              {
                file->ignore(differential);
                file->read((char *)&lsb_bytes[0], 4);
                smp->sample_data[j][i] = from_lsb4(lsb_bytes);
              }
              if (padding)
                file->ignore(padding);
            }

            return smp;
          }

          throw "format was not recognized";
        }
      }
      throw "no data chunk was found";
    }
    else
      throw "RIFF type is not WAVE";
  }
  else if ((filetype_hint != sample_filetype_RAW) && (string(magic, 4) == "FORM")) // then it's probably an AIFF format file
  {
    unsigned char msb_bytes[4];

    unsigned long file_length; // not including 8-byte RIFF header
    file->read((char *)&msb_bytes[0], 4);
    file_length = from_msb4(msb_bytes);

    file->read(magic, 4);
    if (string(magic, 4) == "AIFF") // then it's an AIFF audio file
    {
      AIFF::chunk chunk;
      AIFF::common_chunk common_chunk;
      bool have_common_chunk = false;

      while (true)
      {
        file->read(chunk.raw.chunkID, 4);
        file->read((char *)&msb_bytes[0], 4);
        chunk.raw.chunkSize = from_msb4(msb_bytes);

        if (!file->good())
          throw "unexpected end-of-file";

        if (chunk.chunkID() == "COMM")
        {
          if (chunk.raw.chunkSize > sizeof(chunk) - 8)
          {
            file->ignore(chunk.raw.chunkSize);
            continue;
          }

          file->read((char *)&msb_bytes[0], 2);
          chunk.common.numChannels = from_msb2(msb_bytes);

          file->read((char *)&msb_bytes[0], 4);
          chunk.common.numSampleFrames = from_msb4(msb_bytes);

          file->read((char *)&msb_bytes[0], 2);
          chunk.common.sampleSize = from_msb2(msb_bytes);

          file->read((char *)&chunk.common.sampleRate.data[0], 10);
          
          common_chunk = chunk.common;
          have_common_chunk = true;
        }

        if (!file->good())
          throw "unexpected end-of-file";

        if (chunk.chunkID() == "SSND")
        {
          if (!have_common_chunk)
            throw "did not find a valid COMM chunk before SSND";

          file->read((char *)&msb_bytes[0], 4);
          chunk.ssnd.offset = from_msb4(msb_bytes);

          file->read((char *)&msb_bytes[0], 4);
          chunk.ssnd.blockSize = from_msb4(msb_bytes);
          
          if (common_chunk.sampleSize <= 8)
          {
            sample_builtintype<signed char> *smp;

            smp = new sample_builtintype<signed char>(0, common_chunk.numChannels);

            smp->num_samples = common_chunk.numSampleFrames;
            smp->samples_per_second = common_chunk.sampleRate.toDouble();
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed char[smp->num_samples];

            if (chunk.ssnd.blockSize == 0)
            {
              chunk.ssnd.blockSize = 1;
              chunk.ssnd.offset = 0;
            }

            ArrayAllocator<char> block_allocator(chunk.ssnd.blockSize);
            char *block = block_allocator.getArray();

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<common_chunk.numChannels; j++)
              {
                file->read(block, chunk.ssnd.blockSize);
                smp->sample_data[j][i] = (signed char)block[chunk.ssnd.offset];
              }
            }

            return smp;
          }

          if (common_chunk.sampleSize <= 16)
          {
            sample_builtintype<signed short> *smp;

            smp = new sample_builtintype<signed short>(0, common_chunk.numChannels);

            smp->num_samples = common_chunk.numSampleFrames;
            smp->samples_per_second = common_chunk.sampleRate.toDouble();
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed short[smp->num_samples];

            if (chunk.ssnd.blockSize == 0)
            {
              chunk.ssnd.blockSize = 2;
              chunk.ssnd.offset = 0;
            }

            ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
            unsigned char *block = block_allocator.getArray();

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<common_chunk.numChannels; j++)
              {
                file->read((char *)block, chunk.ssnd.blockSize);
                smp->sample_data[j][i] = from_msb2(block + chunk.ssnd.offset);
              }
            }

            return smp;
          }

          if (common_chunk.sampleSize <= 24)
          {
            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, common_chunk.numChannels);

            smp->num_samples = common_chunk.numSampleFrames;
            smp->samples_per_second = common_chunk.sampleRate.toDouble();
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            if (chunk.ssnd.blockSize == 0)
            {
              chunk.ssnd.blockSize = 3;
              chunk.ssnd.offset = 0;
            }

            ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize + 1);
            unsigned char *block = block_allocator.getArray();;
            block[chunk.ssnd.blockSize] = 0;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<common_chunk.numChannels; j++)
              {
                file->read((char *)block, chunk.ssnd.blockSize);
                smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
              }
            }

            return smp;
          }

          if (common_chunk.sampleSize <= 32)
          {
            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, common_chunk.numChannels);

            smp->num_samples = common_chunk.numSampleFrames;
            smp->samples_per_second = common_chunk.sampleRate.toDouble();
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            if (chunk.ssnd.blockSize == 0)
            {
              chunk.ssnd.blockSize = 1;
              chunk.ssnd.offset = 0;
            }

            ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
            unsigned char *block = block_allocator.getArray();;

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<common_chunk.numChannels; j++)
              {
                file->read((char *)block, chunk.ssnd.blockSize);
                smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
              }
            }

            return smp;
          }

          if (common_chunk.sampleSize > 32)
          {
            cerr << "Warning: AIFF sample size is greater than 32 bits (non-standard)" << endl;

            sample_builtintype<signed long> *smp;

            smp = new sample_builtintype<signed long>(0, common_chunk.numChannels);

            smp->num_samples = common_chunk.numSampleFrames;
            smp->samples_per_second = common_chunk.sampleRate.toDouble();
            for (int i=0; i<smp->sample_channels; i++)
              smp->sample_data[i] = new signed long[smp->num_samples];

            if (chunk.ssnd.blockSize == 0)
            {
              chunk.ssnd.blockSize = (common_chunk.sampleSize + 7) / 8;
              chunk.ssnd.offset = chunk.ssnd.blockSize - 4;
            }

            ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
            unsigned char *block = block_allocator.getArray();

            for (int i=0; i<smp->num_samples; i++)
            {
              for (int j=0; j<common_chunk.numChannels; j++)
              {
                file->read((char *)block, chunk.ssnd.blockSize);
                smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
              }
            }

            return smp;
          }

          throw "format was not recognized";
        }
      }
    }
  }
  else // raw data
  {
    int diff_8bit = 0;
    int diff_16bit = 0;
    int diff_16bit_msb = 0;
    for (int i=0; i<500; i++)
    {
      diff_8bit += (magic[3] - magic[2]) + (magic[1] - magic[0]);
      diff_16bit += (magic[3] - magic[1]);
      diff_16bit_msb += (magic[2] - magic[0]);
      file->read(magic, 4);
      if (file->eof())
        break;
    }

    file->seekg(0, ios::end);
    long length = file->tellg();
    file->seekg(0, ios::beg);

    if ((diff_8bit < diff_16bit) && (diff_8bit < diff_16bit_msb)) // then try for 8-bit data :-)
    {
      unsigned char *data = new unsigned char[length];

      file->read((char *)data, length);

      // now find signed-ness
      int delta = data[1] - data[0];

      int signed_score = 0;
      int unsigned_score = 0;

      for (int i=2; i<length; i++)
      {
        if ((data[1] < 0x80) && (data[2] >= 0x80))
        {
          if (delta > 0)
            unsigned_score++;
          else
            signed_score++;
        }
        else if ((data[1] >= 0x80) && (data[2] < 0x80))
        {
          if (delta > 0)
            signed_score++;
          else
            unsigned_score++;
        }
      }

      signed char *data_sgn = (signed char *)data;

      if (unsigned_score > signed_score)
        for (int i=0; i<length; i++)
          data_sgn[i] = (signed char)(int(data[i]) - 128);

      sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(0, 1);

      smp->num_samples = length;
      smp->sample_data[0] = data_sgn;
      smp->samples_per_second = 8363; // typical default

      return smp;
    }
    else if ((diff_16bit < diff_8bit) && (diff_16bit < diff_16bit_msb))
    {
      if (length & 1)
        cerr << "Warning: sample data looks 16-bit, but byte length is not even" << endl;

      length >>= 1;

      unsigned short *data = new unsigned short[length];

      file->read((char *)data, length);

      // now find signed-ness
      int delta = data[1] - data[0];

      int signed_score = 0;
      int unsigned_score = 0;

      for (int i=2; i<length; i++)
      {
        if ((data[1] < 0x8000) && (data[2] >= 0x8000))
        {
          if (delta > 0)
            unsigned_score++;
          else
            signed_score++;
        }
        else if ((data[1] >= 0x8000) && (data[2] < 0x8000))
        {
          if (delta > 0)
            signed_score++;
          else
            unsigned_score++;
        }
      }

      signed short *data_sgn = (signed short *)data;

      if (unsigned_score > signed_score)
        for (int i=0; i<length; i++)
          data_sgn[i] = (signed short)(int(data[i]) - 32768);

      sample_builtintype<signed short > *smp = new sample_builtintype<signed short >(0, 1);

      smp->num_samples = length >> 1;
      smp->sample_data[0] = data_sgn;
      smp->samples_per_second = 8363; // typical default

      return smp;
    }
    else if ((diff_16bit_msb < diff_8bit) && (diff_16bit_msb < diff_16bit))
    {
      if (length & 1)
        cerr << "Warning: sample data looks 16-bit, but byte length is not even" << endl;

      length >>= 1;

      unsigned char msb_bytes[2];

      unsigned short *data = new unsigned short[length];

      for (int i=0; i<length; i++)
      {
        file->read((char *)&msb_bytes[0], 2);
        data[i] = from_msb2(msb_bytes);
      }

      // now find signed-ness
      int delta = data[1] - data[0];

      int signed_score = 0;
      int unsigned_score = 0;

      for (int i=2; i<length; i++)
      {
        if ((data[1] < 0x8000) && (data[2] >= 0x8000))
        {
          if (delta > 0)
            unsigned_score++;
          else
            signed_score++;
        }
        else if ((data[1] >= 0x8000) && (data[2] < 0x8000))
        {
          if (delta > 0)
            signed_score++;
          else
            unsigned_score++;
        }
      }

      signed short *data_sgn = (signed short *)data;

      if (unsigned_score > signed_score)
        for (int i=0; i<length; i++)
          data_sgn[i] = (signed short)(int(data[i]) - 32768);

      sample_builtintype<signed short > *smp = new sample_builtintype<signed short >(0, 1);

      smp->num_samples = length >> 1;
      smp->sample_data[0] = data_sgn;
      smp->samples_per_second = 8363; // typical default

      return smp;
    }
  }

  throw "format was not recognized";
}

struct instrument_envelope_node
{
  int tick;
  double value;

  instrument_envelope_node(int t, double v)
    : tick(t), value(v)
  {
  }
};

struct sustain_loop_position
{
  bool still_running;
  double exit_diff;
};

struct instrument_envelope
{
  bool enabled, looping, sustain_loop;

  int loop_begin_tick, loop_end_tick, sustain_loop_begin_tick, sustain_loop_end_tick;
  double loop_length, sustain_loop_length; // to reduce conversions

  vector<instrument_envelope_node> node;

  instrument_envelope()
  {
    enabled = false;
  }

  void begin_sustain_loop(sustain_loop_position &susloop)
  {
    loop_length = loop_end_tick - loop_begin_tick; // have to calc this somewhere
    sustain_loop_length = sustain_loop_end_tick - sustain_loop_begin_tick;

    susloop.still_running = (sustain_loop == true); // for clarity
    susloop.exit_diff = 0.0;
  }

  void exit_sustain_loop(double tick, sustain_loop_position &susloop)
  {
    if (susloop.still_running)
    {
      if (tick > sustain_loop_end_tick)
      {
        double new_tick;

        if (sustain_loop_length)
        {
          double repetitions = floor((tick - sustain_loop_begin_tick) / sustain_loop_length);
          new_tick = tick - repetitions * sustain_loop_length;
        }
        else
          new_tick = sustain_loop_begin_tick;

        susloop.exit_diff = tick - new_tick;
      }
      else
        susloop.exit_diff = 0.0;

      susloop.still_running = false;
    }
  }

  double get_value_at(double tick, sustain_loop_position &susloop)
  {
    if (susloop.still_running)
    {
      if (tick > sustain_loop_end_tick)
      {
        if (sustain_loop_length)
        {
          double repetitions = floor((tick - sustain_loop_begin_tick) / sustain_loop_length);
          tick -= repetitions * sustain_loop_length;
        }
        else
          tick = sustain_loop_begin_tick;
      }
    }
    else
    {
      tick -= susloop.exit_diff;

      if (looping && (tick > loop_end_tick))
      {
        if (loop_length)
        {
          double repetitions = floor((tick - loop_begin_tick) / loop_length);
          tick -= repetitions * loop_length;
        }
        else
          tick = loop_begin_tick;
      }
    }

    int idx = 1;
    int l = node.size() - 1;

    while (tick > node[idx].tick)
    {
      idx++;

      if (idx >= l)
        return node[l].value;
    }

    tick -= node[idx].tick;

    double t = tick / (node[idx].tick - node[idx - 1].tick);

    return bilinear(node[idx - 1].value, node[idx].value, t);
  }
  
  bool past_end(double tick, sustain_loop_position &susloop)
  {
    if (susloop.still_running)
      return false;
    if (looping)
      return false;

    if (sustain_loop)
      tick -= susloop.exit_diff;

    return (tick > node.back().tick);
  }
};

struct playback_envelope
{
  instrument_envelope &env;
  double sample_ticks_per_envelope_tick, envelope_ticks_per_sample_tick;
  sustain_loop_position susloop;
  playback_envelope *wrap;
  bool scale, looping;

  playback_envelope(const playback_envelope &other)
    : wrap(other.wrap ? new playback_envelope(*other.wrap) : NULL),
      sample_ticks_per_envelope_tick(other.sample_ticks_per_envelope_tick),
      envelope_ticks_per_sample_tick(other.envelope_ticks_per_sample_tick),
      susloop(other.susloop), scale(other.scale), env(other.env),
      looping(other.looping)
  {
  }

  playback_envelope(instrument_envelope &e, double ticks)
    : wrap(NULL), env(e), sample_ticks_per_envelope_tick(ticks),
      envelope_ticks_per_sample_tick(1.0 / ticks),
      looping(e.looping)
  {
  }

  playback_envelope(playback_envelope *w, instrument_envelope &e, double ticks, bool scale_envelope)
    : wrap(w), env(e), sample_ticks_per_envelope_tick(ticks),
      envelope_ticks_per_sample_tick(1.0 / ticks), scale(scale_envelope),
      looping(e.looping || w->looping)
  {
  }

  ~playback_envelope()
  {
    if (wrap)
      delete wrap;
  }

  void begin_note(bool recurse = true)
  {
    env.begin_sustain_loop(susloop);
    if (wrap && recurse)
      wrap->begin_note();
  }

  void note_off(double sample_offset)
  {
    double tick = sample_offset * envelope_ticks_per_sample_tick;
    env.exit_sustain_loop(tick, susloop);
    if (wrap)
      wrap->note_off(sample_offset);
  }

  void note_off(long sample, double offset)
  {
    note_off(sample + offset);
  }

  double get_value_at(double sample_offset)
  {
    double tick = sample_offset * envelope_ticks_per_sample_tick;

    if (!wrap)
      return env.get_value_at(tick, susloop);

    if (scale)
      return env.get_value_at(tick, susloop) * wrap->get_value_at(sample_offset);
    else
      return env.get_value_at(tick, susloop) + wrap->get_value_at(sample_offset);
  }

  double get_value_at(long sample, double offset)
  {
    return get_value_at(sample + offset);
  }

  bool past_end(double sample_offset)
  {
    double tick = sample_offset * envelope_ticks_per_sample_tick;

    if (env.past_end(tick, susloop))
      return true;

    if (wrap)
      return wrap->past_end(sample_offset);

    return false;
  }

  bool past_end(long sample, double offset)
  {
    return past_end(sample + offset);
  }
};

double global_volume = 1.0;
vector<sample *> samples;

struct channel
{
  bool finished, looping;
  double offset, delta_offset_per_tick;
  double note_frequency;
  long offset_major;
  int ticks_left, dropoff_start, rest_ticks, cutoff_ticks;
  int current_znote;
  bool play_full_sample;
  double intensity, channel_volume;
  Waveform::Type current_waveform;
  sample *current_sample;
  sample_context *current_sample_context;
  pan_value panning;
  one_sample return_sample;
  long samples_this_note;
  playback_envelope *volume_envelope, *panning_envelope, *pitch_envelope;

  vector<channel *> my_ancillary_channels;

  bool fading, have_fade_per_tick;
  double fade_per_tick, fade_value;

  int tempo;
  int octave;
  int note_length_denominator, this_note_length_denominator;
  double rest_ticks_proportion;

  void recalc(int znote, double duration_scale, bool calculate_length = true, bool reset_sample_offset = true, bool zero_means_no_note = true)
  {
    double seconds;

    if (calculate_length)
    {
      if (this_note_length_denominator != 0)
        seconds = (4.0 / this_note_length_denominator) * 60.0 / tempo;
    }

    current_znote = znote;

    if ((znote != 0) || (!zero_means_no_note))
    {
      if ((current_waveform == Waveform::Sample) && (current_sample != NULL))
        note_frequency = current_sample_context->samples_per_second * 2;
      else
        note_frequency = 987.766603;
      
      note_frequency *= pow(inter_note, znote - 49);

      if (reset_sample_offset)
      {
        offset = 0;
        offset_major = 0;
      }

      delta_offset_per_tick = note_frequency / ticks_per_second;
    }

    if (calculate_length)
    {
      if (this_note_length_denominator == 0)
        seconds = current_sample->num_samples / current_sample->samples_per_second;

      ticks_left = (int)(seconds * ticks_per_second * duration_scale);
      if (play_full_sample)
      {
        double note_seconds = seconds;
        if (this_note_length_denominator != 0)
          seconds = (4.0 / this_note_length_denominator) * 60.0 / tempo;

        cutoff_ticks = (int)((seconds - note_seconds) * ticks_per_second * duration_scale);
        rest_ticks = cutoff_ticks + (int)((ticks_left - cutoff_ticks) * rest_ticks_proportion);
      }
      else if ((znote != 0) || (!zero_means_no_note))
        rest_ticks = (int)(ticks_left * rest_ticks_proportion);
      else
        rest_ticks = ticks_left;

      double dropoff_time = seconds * dropoff_proportion;
      if (dropoff_time < dropoff_min_length)
        dropoff_time = dropoff_min_length;
      else if (dropoff_time > dropoff_max_length)
        dropoff_time = dropoff_max_length;

      int dropoff_ticks = (int)(dropoff_time * ticks_per_second * duration_scale);

      dropoff_start = rest_ticks + dropoff_ticks;
    }
  }

  virtual bool advance_pattern(one_sample &sample)
  {
    throw "no implementation for advance_pattern";
  }

  virtual void note_off(bool calc_fade_per_tick = true, bool all_notes_off = true)
  {
    if (current_sample != NULL)
      current_sample->exit_sustain_loop(current_sample_context);

    if (all_notes_off)
    {
      if (volume_envelope != NULL)
        volume_envelope->note_off(offset_major, offset);
      if (panning_envelope != NULL)
        panning_envelope->note_off(offset_major, offset);
      if (pitch_envelope != NULL)
        pitch_envelope->note_off(offset_major, offset);
    }

    if ((volume_envelope == NULL) || volume_envelope->looping)
    {
      fading = true;
      fade_value = 1.0;
    }
    if (calc_fade_per_tick)
    {
      double fade_ticks = dropoff_proportion * ticks_left;
      if (fade_ticks < dropoff_min_length)
        fade_ticks = dropoff_min_length;
      if (fade_ticks > dropoff_max_length)
        fade_ticks = dropoff_max_length;
      fade_per_tick = 1.0 / fade_ticks;
    }
  }

  one_sample &calculate_next_tick()
  {
    return_sample.clear(output_channels);

    if (finished)
      return return_sample;

    if (advance_pattern(return_sample))
    {
      return_sample = panning * (channel_volume * return_sample);
      return return_sample;
    }

    if (ticks_left)
    {
      ticks_left--;
      if (ticks_left < rest_ticks)
      {
        return_sample.clear();
        return return_sample;
      }

      switch (current_waveform)
      {
        case Waveform::Sine:
          return_sample = panning * (channel_volume * sin(6.283185 * offset));
          break;
        case Waveform::Square:
          return_sample = panning * (channel_volume * ((offset > 0.5) * 2 - 1));
          break;
        case Waveform::Sawtooth:
          if (offset < 0.5)
            return_sample = panning * (channel_volume * (2.0 * offset));
          else
            return_sample = panning * (channel_volume * (2.0 * (offset - 1.5)));
          break;
        case Waveform::RampDown:
          if (offset < 0.5)
            return_sample = panning * (channel_volume * (-2.0 * offset));
          else
            return_sample = panning * (channel_volume * ((1.0 - offset) * 2.0));
          break;
        case Waveform::Triangle:
          if (offset < 0.25)
            return_sample = panning * (channel_volume * (4.0 * offset));
          else if (offset < 0.75)
            return_sample = panning * (channel_volume * (4.0 * (0.5 - offset)));
          else                                            
            return_sample = panning * (channel_volume * (4.0 * (offset - 1.0)));
          break;
        case Waveform::Sample:
          if (current_sample)
            return_sample = panning * (channel_volume * current_sample->get_sample(offset_major, offset, current_sample_context));
          else
            return_sample.clear(output_channels);
      }

      double sample_offset;
      if (volume_envelope || panning_envelope || pitch_envelope)
        sample_offset = samples_this_note;

      if (fading)
      {
        if (fade_value > 0.0)
          return_sample *= fade_value;
        else
          return_sample.clear();
        fade_value -= fade_per_tick;
      }
      else if (volume_envelope != NULL)
      {
        if (volume_envelope->past_end(sample_offset))
        {
          fading = true;
          fade_value = 1.0;

          if (!have_fade_per_tick)
          {
            note_off(true, false);
            have_fade_per_tick = true;
          }
        }
        return_sample *= volume_envelope->get_value_at(sample_offset);
      }

      if (panning_envelope != NULL)
        return_sample *= pan_value(panning_envelope->get_value_at(sample_offset));

      if (pitch_envelope != NULL)
      {
        double frequency = delta_offset_per_tick * ticks_per_second;
        double exponent = lg(frequency);

        exponent += pitch_envelope->get_value_at(sample_offset) * (16.0 / 12.0);
        if ((current_waveform == Waveform::Sample) && (current_sample->use_vibrato))
          exponent += current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

        frequency = p2(exponent);
        offset += (frequency / ticks_per_second);
      }
      else
      {
        if ((current_waveform == Waveform::Sample) && (current_sample && current_sample->use_vibrato))
        {
          double frequency = delta_offset_per_tick * ticks_per_second;
          double exponent = lg(frequency);
          
          exponent += current_sample->vibrato_depth * sin(6.283185 * samples_this_note * current_sample->vibrato_cycle_frequency);

          frequency = p2(exponent);
          offset += (frequency / ticks_per_second);
        }
        else
          offset += delta_offset_per_tick;
      }

      samples_this_note++;

      if ((offset > 1.0) || (offset < 0.0))
      {
        if (offset > 2.0)
        {
          double offset_floor = floor(offset);

          offset_major += long(offset_floor);
          offset -= offset_floor;
        }
        else
        {
          offset_major++;
          offset -= 1.0;
        }
      }

      if (ticks_left < dropoff_start)
      {
        double volume = double(ticks_left - rest_ticks) / (dropoff_start - rest_ticks);
        return_sample *= volume;
      }

      if (ticks_left == cutoff_ticks)
        ticks_left = 0;

      return_sample *= intensity;
    }

    return return_sample;
  }

  channel(bool looping)
    : return_sample(output_channels),
      panning(output_channels),
      looping(looping)
  {
    finished = false;
    ticks_left = 0;
    octave = 4;
    tempo = 120;
    note_length_denominator = 4;
    rest_ticks_proportion = 1.0 / 8.0;
    play_full_sample = false;
    intensity = 5000.0 / 32767.0;
    channel_volume = 1.0;
    current_waveform = default_waveform;
    current_sample = NULL;
    current_sample_context = NULL;
    volume_envelope = NULL;
    panning_envelope = NULL;
    pitch_envelope = NULL;
    fading = false;
    have_fade_per_tick = false;
  }

  channel(pan_value &default_panning, bool looping)
    : return_sample(output_channels),
      panning(default_panning),
      looping(looping)
  {
    finished = false;
    ticks_left = 0;
    octave = 4;
    tempo = 120;
    note_length_denominator = 4;
    rest_ticks_proportion = 1.0 / 8.0;
    play_full_sample = false;
    intensity = 5000.0 / 32767.0;
    channel_volume = 1.0;
    current_waveform = default_waveform;
    current_sample = NULL;
    current_sample_context = NULL;
    volume_envelope = NULL;
    panning_envelope = NULL;
    pitch_envelope = NULL;
    fading = false;
    have_fade_per_tick = false;
  }

  virtual ~channel()
  {
    if (volume_envelope)
      delete volume_envelope;
    if (panning_envelope)
      delete panning_envelope;
    if (pitch_envelope)
      delete pitch_envelope;
    if (current_sample_context)
      delete current_sample_context;
  }
};

struct channel_PLAY : public channel
{
  istream *in;

  channel_PLAY(istream *input, bool looping)
    : channel(looping),
      in(input)
  {
    if (in->get() < 0)
      finished = true;
    
    in->seekg(0, ios::beg);
  }

  virtual ~channel_PLAY()
  {
  }
  
  virtual bool advance_pattern(one_sample &sample)
  {
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
            current_sample->begin_new_note(NULL, this, &current_sample_context, 0.02 * ticks_per_second, true, &znote);
          recalc(znote, duration_scale);
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

enum effect_type
{
  effect_type_mod,
  effect_type_s3m,
  effect_type_it,
};

struct effect_info_type
{
  struct // anonymous structs like this are not valid ISO C++, but both VC++ and g++ support them
  {
    unsigned int low_nybble  : 4;
    unsigned int high_nybble : 4;
  };
  unsigned char data;

  operator unsigned char()
  {
    return data;
  }

  effect_info_type &operator =(unsigned char data)
  {
    this->data = data;
    low_nybble = data & 0xF;
    high_nybble = (data >> 4) & 0xF;
    return *this;
  }

  effect_info_type()
  {
    data = 0;
    low_nybble = 0;
    high_nybble = 0;
  }
};

struct effect_struct
{
  bool present;
  char command;
  effect_info_type info;
  effect_type type;

  effect_struct &operator =(int value)
  {
    if (value < 0)
      present = false;
    else
    {
      present = true;
      command = (value >> 16) & 0xFF;
      info = (value & 0xFF);
      type = effect_type_s3m;
    }
    return *this;
  }

  effect_struct(effect_type type, char command, unsigned char info)
  {
    init(type, command, info);
  }

  void init(effect_type type, char command, unsigned char info, row *r = NULL);

  effect_struct()
    : present(false)
  {
  }

  bool keepNote()
  {
    if (!present)
      return false;

    switch (type)
    {
      case effect_type_s3m:
      case effect_type_it:
        switch (command)
        {
          case 'G':
          case 'L':
            return true;
          default:
            return false;
        }
      default:
        throw "internal error";
    }
  }
  
  bool keepVolume()
  {
    if (!present)
      return false;

    switch (type)
    {
      case effect_type_s3m:
      case effect_type_it:
        switch (command)
        {
          case 'D':
          case 'K':
          case 'L':
            return true;
          default:
            return false;
        }
      default:
        throw "internal error";
    }
  }

  bool isnt(char effect, signed char high_nybble = -1, signed char low_nybble = -1)
  {
    if (command != effect)
      return true;
    if ((high_nybble != -1) && (info.high_nybble != (char)high_nybble))
      return true;
    if ((low_nybble != -1) && (info.low_nybble != (char)high_nybble))
      return true;
    return false;
  }

  bool is(char effect, signed char high_nybble = -1, signed char low_nybble = -1)
  {
    return !isnt(effect, high_nybble, low_nybble);
  }
};

struct row
{
  int znote, snote;
  int volume;
  sample *instrument;
  effect_struct effect, secondary_effect;

  row()
  {
    snote = -1;
    volume = -1;
    instrument = NULL;
  }
};

void effect_struct::init(effect_type type, char command, unsigned char info, row *r)
{
  switch (type)
  {
    case effect_type_mod:
      // This is largely possible due to the correspondence between MOD and
      // S3M effect parameters. Only the 'invert loop' option required
      // special coding.
      switch (command)
      {
        case 0x0: // arpeggio
          if (info)
            command = 'J';
          else
          {
            present = false;
            return; // no further processing required
          }
          break;
        case 0x1: // portamento up
          command = 'F';
          if (info >= 0xE0)                          
            info = 0xDF;
          break;
        case 0x2: // portamento down
          command = 'E';
          if (info >= 0xE0)
            info = 0xDF;
          break;
        case 0x3: // tone portamento
          command = 'G';
          break;
        case 0x4: // vibrato
          command = 'H';
          break;
        case 0x5: // 300 & Axy
          command = 'L';
          break;
        case 0x6: // 400 & Axy
          command = 'K';
          break;
        case 0x7: // tremolo
          command = 'R';
          break;
        case 0x8: // fine panning (0-255; converted here to 0-128 for Xxx)
          command = 'X';
          info = info * 128 / 255;
          break;
        case 0x9: // set sample offset
          command = 'O';
          break;
        case 0xA: // volume slide
          command = 'D';
          break;
        case 0xB: // pattern/row jump
          command = 'B';
          break;
        case 0xC: // set volume
          if (r)
            r->volume = info;
          else
            cerr << "Warning: had to discard a MOD set volume effect" << endl;

          present = false; // no further processing required
          return;
        case 0xD: // pattern break
          command = 'C';
          break;
        case 0xE:
          switch (info >> 4)
          {
            case 0x0: // set filter
              present = false; // no further processing required
              return;
            case 0x1: // fine slide up
              if (info & 0xF)
              {
                command = 'F';
                info |= 0xF0;
                if ((info & 0xF) == 0xF)
                  info = 0xFE;
              }
              else
              {
                present = false; // no further processing required
                return;
              }
              break;
            case 0x2: // fine slide down
              if (info & 0xF)
              {
                command = 'E';
                info |= 0xF;
                if ((info & 0xF) == 0xF)
                  info = 0xFE;
              }
              else
              {
                present = false; // no further processing required
                return;
              }
              break;
            case 0x3: // glissando control
              command = 'S';
              info = (info & 0xF) | 0x10;
              break;
            case 0x4: // set vibrato waveform
              command = 'S';
              info = (info & 0xF) | 0x30;
              break;
            case 0x5: // set finetune
              command = 'S';
              info = ((info & 0xF) ^ 8) | 0x20;
              break;
            case 0x6: // loop pattern
              command = 'S';
              info = (info & 0xF) | 0xB0;
              break;
            case 0x7: // set tremolo waveform
              command = 'S';
              info = (info & 0xF) | 0x40;
              break;
            case 0x8: // rough panning
              command = 'S';
              break;
            case 0x9: // retrigger sample
              if (info & 0xF)
              {
                command = 'Q';
                info = info & 0xF;
              }
              else
              {
                present = false; // no further processing required
                return;
              }
              break;
            case 0xA: // fine volume slide up
              command = 'D';
              if ((info & 0xF) == 0xF)
                info = 0xE;
              info = ((info & 0xF) << 4) | 0xF;
              break;
            case 0xB: // fine volume slide down
              command = 'D';
              if ((info & 0xF) == 0xF)
                info = 0xE;
              info = (info & 0xF) | 0xF0;
              break;
            case 0xC: // note cut
              command = 'S';
              break;
            case 0xD: // note delay
              command = 'S';
              break;
            case 0xE: // pattern delay
              command = 'S';
              break;
            case 0xF: // invert loop
              break; // do not change
          }
          break;
        case 0xF: // speed change
          if (info >= 0x20)
            command = 'T';
          else
            command = 'A';
          break;
      }
      type = effect_type_s3m; // translated :-)
      this->command = command;
      this->info = info;
      break;
    case effect_type_s3m:
    case effect_type_it:
      if ((command < 1) || (command > 26))
      {
        present = false; // no further processing required
        return;
      }
      this->command = command + 64;
      this->info = info;
      break;
    default:
      throw "unimplemented effect type";
  }
  this->type = type;
  present = true;
}

struct pattern
{
  vector<vector<row> > row_list;
};

struct pattern_loop_type
{
  int row, repetitions;
  bool need_repetitions;

  pattern_loop_type()
  {
    row = 0;
    repetitions = 0;
    need_repetitions = true;
  }
};

#define MAX_MODULE_CHANNELS 64

struct module_sample_info
{
  char vibrato_depth, vibrato_speed;
};

struct module_struct
{
  string filename, name;
  vector<pattern> patterns;
  vector<pattern *> pattern_list;
  vector<sample *> samples;
  vector<module_sample_info> sample_info;
  bool channel_enabled[MAX_MODULE_CHANNELS];
  int channel_map[MAX_MODULE_CHANNELS];
  pattern_loop_type pattern_loop[MAX_MODULE_CHANNELS];
  pan_value initial_panning[MAX_MODULE_CHANNELS];
  int base_pan[MAX_MODULE_CHANNELS];
  int ticks_per_module_row, previous_ticks_per_module_row;
  double ticks_per_frame;
  int speed, tempo;
  int current_pattern, current_row;
  int num_channels;
  int auto_loop_target;
  bool stereo, use_instruments;
  bool it_module, it_module_new_effects, it_module_portamento_link, it_module_linear_slides;
  bool finished;

  void speed_change()
  {
    previous_ticks_per_module_row = ticks_per_module_row;
    ticks_per_module_row = speed * 60 * ticks_per_second / (24 * tempo);
    ticks_per_frame = double(ticks_per_module_row) / speed;
/*
    ticks_per_module_row = speed * ticks_per_quarter_note / 24
    ticks_per_quarter_note = seconds_per_quarter_note * ticks_per_second
    seconds_per_quarter_note = seconds_per_minute / quarter_notes_per_minute /* */
  }

  void initialize()
  {
    speed_change();
    current_pattern = 0;
    current_row = -1;
    finished = false;
  }

  module_struct()
  {
    use_instruments = false;
    it_module = false;
    it_module_new_effects = false;
    it_module_portamento_link = false;
    it_module_linear_slides = false;
    auto_loop_target = -1;
  }
};

int znote_from_snote(int snote)
{
  // middle C is z=39
  // snote middle C is high nybble = 4, low nybble = 0 -> 64
  return (12 * (snote >> 4)) + (snote & 15) - 11;
}

bool smooth_portamento_effect = true, trace_mod = false;

int snote_from_period(int period)
{
  double frequency = 3579264.0 / period;
  int qnote = int(floor(12 * lg(frequency)));

  return (qnote % 12) | (((qnote / 12) - 9) << 4);
}

int req_digits(int max)
{
  if (max < 10)
    return 1;
  if (max < 100)
    return 2;
  return 3;
}

vector<channel *> channels, ancillary_channels;
typedef vector<channel *>::iterator iter_t;

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
      intensity = original_intensity * row.volume / 64.0;
      volume = row.volume;
    }

    if (row.snote != -1)
    {
      if (row.snote == -2)
      {
        current_sample = NULL;
        current_sample_context = NULL;
      }
      else if (row.snote == -3)
        note_off();
      else
      {
        if ((row.instrument != NULL) || module->it_module)
        {
          if (!row.effect.keepNote())
          {
            if (row.instrument)
              current_sample = row.instrument;
            if (current_sample != NULL)
            {
              int translated_znote = row.znote;
              current_sample->begin_new_note(&row, this, &current_sample_context, module->ticks_per_frame, true, &translated_znote);
              recalc(translated_znote, 1.0, false);
            }
            else
              current_sample_context = NULL;
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

namespace NewNoteAction
{
  enum Type
  {
    Cut          = 0,
    ContinueNote = 1,
    NoteOff      = 2,
    NoteFade     = 3,
  };
}

namespace DuplicateCheck
{
  enum Type
  {
    Off        = 0,
    Note       = 1,
    Sample     = 2,
    Instrument = 3,
  };
}

namespace DuplicateCheckAction
{
  enum Type
  {
    Cut      = 0,
    NoteOff  = 1,
    NoteFade = 2,
  };
}

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

struct sample_instrument_context : sample_context
{
  sample *cur_sample;
  sample_context *cur_sample_context;
  double cur_volume;
  int ticks_retrieved; // makes the big assumption that samples are read once linearly per invocation
  SustainLoopState::Type sustain_loop_state;
  channel *owner_channel;
  double effect_tick_length;
  int inote;

  sample_instrument_context(sample *cw) : sample_context(cw) {}
};

#include "DSP.h"

struct sample_instrument : sample
{
  double global_volume;

  pan_value default_pan;
  bool use_default_pan;

  DSP dsp;
  bool use_dsp;

  int pitch_pan_separation, pitch_pan_center;

  double volume_variation_pctg, panning_variation;

  DuplicateCheck::Type duplicate_note_check;
  DuplicateCheckAction::Type duplicate_check_action;
  NewNoteAction::Type new_note_action;

  sample *note_sample[120];
  int tone_offset[120];

  instrument_envelope volume_envelope, panning_envelope, pitch_envelope;

  sample_instrument(int idx)
    : sample(idx)
  {
    use_dsp = false;
  }
  
  virtual void occlude_note(channel *p = NULL, sample_context **context = NULL, sample *new_sample = NULL, row *r = NULL)
  {
    NewNoteAction::Type effective_nna = new_note_action;

    if (r->effect.present && (r->effect.is('S', 7)))
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

    if ((effective_nna != NewNoteAction::Cut) && (*context != NULL) && (p != NULL))
    {
      bool is_duplicate = false;
      int new_inote = (r->snote >> 4) * 12 + (r->snote & 15);

      if (this == new_sample)
      {
        switch (duplicate_note_check)
        {
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

      if (p->volume_envelope)
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
        case NewNoteAction::NoteOff:
          ancillary->note_off();
          break;
      }

      ancillary_channels.push_back(ancillary);
      p->my_ancillary_channels.push_back(ancillary);

      *context = NULL; // protect the context from deletion
    }
  }

  virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **context = NULL, double effect_tick_length = 0, bool top_level = true, int *znote = NULL)
  {
    if (context == NULL)
      throw "need context for instrument";

    if (*context)
    {
      (*context)->created_with->occlude_note(p, context, this, r);
      if (*context)
        delete *context;
      *context = NULL;
    }

    if (r->snote >= 0)
    {
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

      if (r->effect.is('S', 7))
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
      p->samples_this_note = 0;
  }

  virtual void kill_note(sample_context *c = NULL)
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

  virtual void exit_sustain_loop(sample_context *c = NULL)
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

  virtual bool past_end(int sample, double offset, sample_context *c = NULL)
  {
    if (c == NULL)
      throw "need context for instrument";

    sample_instrument_context &context = *(sample_instrument_context *)c;

    if (context.cur_sample != NULL)
      return context.cur_sample->past_end(sample, offset, c);
    else
      return true;
  }

  virtual one_sample get_sample(int sample, double offset, sample_context *c = NULL)
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
};

#include "Load_MOD.h"
#include "Load_MTM.h"
#include "Load_S3M.h"
#include "Load_IT.h"

string &tolower(string &s)
{
  transform(s.begin(), s.end(), s.begin(), ptr_fun<int, int>(std::tolower));
  return s;
}

module_struct *load_module(const string &filename)
{
  ifstream input(filename.c_str(), ios::binary);
  module_struct *module;

  size_t offset = filename.find_last_of('.');
  string extension(filename.substr(offset + 1));

  tolower(extension);

  try
  {
    if (extension == "mod")
      module = load_mod(&input);
    else if (extension == "mud")
      module = load_mod(&input, true);
    else if (extension == "mtm")
      module = load_mtm(&input);
    else if (extension == "s3m")
      module = load_s3m(&input);
    else if (extension == "it")
      module = load_it(&input);
    else if (extension == "shit")
      module = load_it(&input, true);
    else
      throw "unrecognized file extension";
  }
  catch (const char *e)
  {
    input.close();
    throw e;
  }

  module->filename = filename;

  input.close();

  return module;
}

namespace direct_output
{
  enum type
  {
    none,
#ifdef DIRECTX
    directx,
#endif
#ifdef SDL
    sdl,
#endif
  };
};

void show_usage(char *cmd_name)
{
  string indentws(strlen(cmd_name), char(32));
  //       12345678901234567890123456789012345678901234567890123456789012345678901234567890
  cerr << "usage: " << cmd_name << " [-play <PLAY files>] [-samples <sample files>]" << endl
       << "       " << indentws << " [-module <S3M filenames>] [-frame-based_portamento]" << endl
       << "       " << indentws << " [-max_time <seconds>] [-max_ticks <ticks>] [-compress]" << endl
       << "       " << indentws << " [-output <output_file>] [-amplify <factor>]" << endl
       << "       " << indentws << " {-stereo | -mono} {-lsb | -msb | -system_byte_order}" << endl
       << "       " << indentws << " { {-8 | -16} [-unsigned] | {-32 | -64} }" << endl
       << "       " << indentws << " [-sample_rate <samples_per_sec>] [-looping]" << endl
       << "       " << indentws
#ifdef DIRECTX
                                << " [-directx]"
#endif
#ifdef SDL
                                            << " [-sdl]"
#endif
                                                    << endl
       << endl
       << "8- and 16-bit sample sizes are integers, -32 and -64 are floating-point (IEEE" << endl
       << "standard). The default output filename is 'output.raw'. The default byte order" << endl
       << "is system; -lsb and -msb force Intel and Motorola endianness, respectively." << endl
       << "The byte order setting applies only to integer output (8- and 16-bit samples)." << endl;
}

void expect_filenames(int &index, int count, char *argv[], vector<string> &collection)
{
  while (argv[index])
  {
    if (argv[index][0] == '-')
      break;

    if (argv[index][0] == '"')
    {
      stringstream filename;

      string arg(&argv[index][1]);

      size_t offset = arg.find('"');

      if (offset != string::npos)
        filename << arg.substr(0, arg.size() - 1);
      else
      {
        filename << arg;

        while (true)
        {
          index++;

          filename << ' ';

          arg = argv[index];

          offset = arg.find('"');

          if (offset == string::npos)
            filename << arg;
          else
          {
            filename << arg.substr(0, offset);
            break;
          }
        }
      }
      collection.push_back(filename.str());
    }
    else
      collection.push_back(string(argv[index++]));
  }
  index--;
}

string trim(string in)
{
  int start = in.find_first_not_of(" \t\n");
  int end = in.find_last_not_of(" \t\n");
  return in.substr(start, end - start + 1);
}

int main(int argc, char *argv[])
{
  double max_time = -1.0;
  long max_ticks = -1;
  string output_filename("output.raw");
  int bits = 16;
  bool unsigned_samples = false;
  direct_output::type direct_output_type = direct_output::none;
  bool stereo_output = true;
  bool looping = false;
  vector<string> play_filenames, play_sample_filenames, module_filenames;
  vector<module_struct *> modules;
  bool lsb_output = false, msb_output = false;
  bool amplify = false;
  double amplify_by = 1.0;
  bool compress = false;

  ticks_per_second = 44100;

  if (argc < 2)
  {
    show_usage(argv[0]);
    return 0;
  }

  register_break_handler();

  for (int i=1; i<argc; i++)
  {
    string arg(argv[i]);

    if ((arg == "-?") || (arg == "-h") || (arg == "-help") || (arg == "--help"))
    {
      show_usage(argv[0]);
      return 0;
    }
    else if (arg == "-play")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -play" << endl;
      else
        expect_filenames(i, argc, argv, play_filenames);
    }
    else if ((arg == "-sample") || (arg == "-samples"))
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter " << arg << endl;
      else
        expect_filenames(i, argc, argv, play_sample_filenames);
    }
    else if ((arg == "-module") || (arg == "-modules"))
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter " << arg << endl;
      else
        expect_filenames(i, argc, argv, module_filenames);
    }
    else if (arg == "-frame-based_portamento")
      smooth_portamento_effect = false;
    else if (arg == "-trace")
      trace_mod = true;
    else if (arg == "-max_seconds")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -max_seconds" << endl;
      else
        max_time = atof(argv[i]);
    }
    else if (arg == "-max_ticks")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -max_ticks" << endl;
      else
        max_ticks = atol(argv[i]);
    }
    else if (arg == "-output")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -output" << endl;
      else
        output_filename = argv[i];
    }
    else if (arg == "-looping")
      looping = true;
    else if (arg == "-msb")
      msb_output = true, lsb_output = false;
    else if (arg == "-lsb")
      msb_output = false, lsb_output = true;
    else if (arg == "-system_byte_order")
      msb_output - false, lsb_output = false;
    else if (arg == "-stereo")
      stereo_output = true;
    else if (arg == "-mono")
      stereo_output = false;
    else if (arg == "-8")
      bits = 8;
    else if (arg == "-16")
      bits = 16;
    else if (arg == "-32")
      bits = 32;
    else if (arg == "-64")
      bits = 64;
    else if (arg == "-unsigned")
      unsigned_samples = true;
    else if (arg == "-sample_rate")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -samplerate" << endl;
      else
        ticks_per_second = abs(atoi(argv[i]));
    }
    else if (arg == "-amplify")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -amplify" << endl;
      else
        amplify = true,
        amplify_by = atof(argv[i]);
    }
    else if (arg == "-compress")
      compress = true;
#ifdef DIRECTX
    else if (arg == "-directx")
      direct_output_type = direct_output::directx;
#endif
#ifdef SDL
    else if (arg == "-sdl")
      direct_output_type = direct_output::sdl;
#endif
    else
      cerr << argv[0] << ": unrecognized command-line parameter: " << arg << endl;
  }

  if (play_filenames.size() + module_filenames.size() == 0)
  {
    cerr << argv[0] << ": nothing to play!" << endl << endl;
    show_usage(argv[0]);
    return 0;
  }

  output_channels = stereo_output ? 2 : 1;

  for (int i=0; i < int(module_filenames.size()); i++)
  {
    module_struct *module;
    
    try
    {
      module = load_module(module_filenames[i]);
    }
    catch (const char *message)
    {
      cerr << argv[0] << ": unable to load module file " << module_filenames[i] << ": " << message << endl;
      continue;
    }

    if (module)
    {
      for (int i=0; i<MAX_MODULE_CHANNELS; i++)
        if (module->channel_enabled[i])
          channels.push_back(new channel_MODULE(i, module, 64, looping));

      modules.push_back(module);
    }
  }

  for (int i=0; i < int(play_filenames.size()); i++)
  {
    ifstream *file = new ifstream(play_filenames[i].c_str(), ios::binary);
    if (!file->is_open())
    {
      delete file;
      cerr << argv[0] << ": unable to open input file: " << play_filenames[i] << endl;
    }
    else
      channels.push_back(new channel_PLAY(file, looping));
  }

  for (int i=0; i < int(play_sample_filenames.size()); i++)
  {
    ifstream file(play_sample_filenames[i].c_str(), ios::binary);
    if (!file.is_open())
      cerr << argv[0] << ": unable to open sample file: " << play_sample_filenames[i] << endl;
    else
    {
      sample *this_sample;

      try
      {
        this_sample = sample_from_file(&file);
      }
      catch (const char *error)
      {
        cerr << argv[0] << ": unable to load sample file: " << play_sample_filenames[i] << " (" << error << ")" << endl;
        continue;
      }
      samples.push_back(this_sample);
    }
  }

  if ((bits > 16) && unsigned_samples)
  {
    cerr << argv[0] << ": cannot use unsigned samples with floating-point output (32 and 64 bit precision)" << endl;
    return 1;
  }

  output_channels = stereo_output ? 2 : 1; // TODO: fix up cmdline parsing

  int status;

  ofstream output;
  
  switch (direct_output_type)
  {
    case direct_output::none:
      output.open(output_filename.c_str(), ios::binary);

      if (!output.is_open())
      {
        cerr << argv[0] << ": unable to open output file: " << output_filename << endl;
        return 1;
      }
      status = 0; // squelch warning if no direct output modes are enabled
      break;
#ifdef DIRECTX
    case direct_output::directx:
      status = init_directx(ticks_per_second, output_channels, bits);

      if (status)
        return status;
      break;
#endif
#ifdef SDL
    case direct_output::sdl:
      status = init_sdl(ticks_per_second, output_channels, bits);

      if (status)
        return status;
      break;
#endif
  }

  for (int i=0; i < int(modules.size()); i++)
    modules[i]->initialize();

  if (max_time >= 0.0)
    max_ticks = long(max_time * ticks_per_second);

  for (int i=0; i < int(modules.size()); i++)
  {
    module_struct *module = modules[i];

    cerr << "Rendering " << module->filename << ":" << endl
      << "  \"" << trim(module->name) << "\"" << endl
      << "  " << module->num_channels << " channels" << endl;
  }

  switch (direct_output_type)
  {
    case direct_output::none:
      cerr << "Output to: " << output_filename << endl;
      break;
#ifdef DIRECTX
    case direct_output::directx:
      cerr << "Direct output: DirectX" << endl;
      break;
#endif
#ifdef SDL
    case direct_output::sdl:
      cerr << "Direct output: SDL" << endl;
      break;
#endif
  }

  cerr << "  " << output_channels << " output channels" << endl
    << "  " << ticks_per_second << " samples per second" << endl
    << "  " << bits << " bits per sample";

  if (bits > 16)
    cerr << " (floating-point)";

  if (unsigned_samples)
    cerr << " (unsigned)";

  cerr << endl;

  if (looping)
    cerr << "  looping" << endl;

  if (max_ticks > 0)
    cerr << "  time limit " << max_ticks << " samples (" << (double(max_ticks) / ticks_per_second) << " seconds)" << endl;

  while (true)
  {
    one_sample sample(output_channels);
    int count = 0;

    for (iter_t i = channels.begin(), l = channels.end();
         i != l;
         ++i)
    {
      if ((*i)->finished)
        continue;

      sample += (*i)->calculate_next_tick();
      count++;
    }

    for (int i = ancillary_channels.size() - 1; i >= 0; i--)
    {
      channel &chan = *ancillary_channels[i];

      if (chan.finished)
      {
        delete &chan;
        ancillary_channels.erase(ancillary_channels.begin() + i);
        continue;
      }

      sample += chan.calculate_next_tick();
      count++;
    }

    if (count == 0)
      break;

    sample *= global_volume;

    if (amplify)
      sample *= amplify_by;

    if (compress)
    {
      bool overdriven = false;
      sample.reset();

      while (sample.has_next())
      {
        double this_sample = sample.next_sample();

        if ((this_sample < -1.0) || (this_sample > 1.0))
        {
          amplify = true;
          amplify_by /= fabs(this_sample);
          overdriven = true;
        }
      }

      if (overdriven)
        cerr << "Warning: overdriven output was detected. Output has been compressed." << endl;
    }

    switch (direct_output_type)
    {
      case direct_output::none:
        sample.reset();
        for (int i=0; i<sample.num_samples; i++)
          switch (bits)
          {
            case 8:
              {
                signed char sample_char = (signed char)(127 * sample.next_sample());

                if (unsigned_samples)
                {
                  unsigned char sample_uchar = (unsigned char)(int(sample_char) + 128); // bias sample
                  output.put(sample_uchar);
                }
                else
                  output.put(sample_char);
              }
              break;
            case 16:
              {
                short sample_short = (short)(32767 * sample.next_sample());

                if (unsigned_samples)
                {
                  unsigned short sample_ushort = short(int(sample_short) + 32768); // bias sample
                  if (msb_output)
                  {
                    output.put((sample_ushort >> 8) & 0xFF);
                    output.put(sample_ushort & 0xFF);
                  }
                  else if (lsb_output)
                  {
                    output.put(sample_ushort & 0xFF);
                    output.put((sample_ushort >> 8) & 0xFF);
                  }
                  else
                    output.write((char *)&sample_ushort, 2);
                }
                else
                {
                  unsigned short *pu16 = (unsigned short *)&sample_short;

                  if (msb_output)
                  {
                    output.put(((*pu16) >> 8) & 0xFF);
                    output.put((*pu16) & 0xFF);
                  }
                  else if (lsb_output)
                  {
                    output.put((*pu16) & 0xFF);
                    output.put(((*pu16) >> 8) & 0xFF);
                  }
                  else
                    output.write((char *)&sample_short, 2);
                }
              }
              break;
            case 32:
              {
                float sample_float = (float)sample.next_sample();

                output.write((char *)&sample_float, 4);
              }
              break;
            case 64:
              output.write((char *)&sample.next_sample(), 8);
              break;
          }
        break;
#ifdef DIRECTX
      case direct_output::directx:
        emit_sample_to_directx_buffer(sample);
        break;
#endif
#ifdef SDL
      case direct_output::sdl:
        emit_sample_to_sdl_buffer(sample);
        break;
#endif
    }

    if (max_ticks > 0)
    {
      max_ticks--;
      if (max_ticks <= 0)
        break;
    }

    if (shutting_down)
      break;
  }

  shutting_down = true;

  switch (direct_output_type)
  {
    case direct_output::none:
      output.close();
      break;
#ifdef DIRECTX
    case direct_output::directx:
      shutdown_directx();
      break;
#endif
#ifdef SDL
    case direct_output::sdl:
      shutdown_sdl();
      break;
#endif
  }

  cerr << endl;

  shutdown_complete = true;

  return 0;
}
