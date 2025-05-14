#include <functional>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <locale>
#include <vector>
#include <string>
#include <map>

using namespace std;

#include <cctype>
#include <cmath>

#include "RAII.h"

using namespace RAII;

namespace MultiPLAY
{
  #include "one_sample.h"
}

#ifdef DIRECTX
#include "Output-DirectX.h"
#endif

#ifdef SDL
#include "Output-SDL.h"
#endif

namespace MultiPLAY
{
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
      Sine,     // ._.�'�'�._.�'�'�._.

      Square,   // _|�|_|�|_|�|_|�|_|�

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
    virtual ~sample_context() {}

    sample_context *clone()
    {
      sample_context *ret = create_new();

      copy_to(ret);

      return ret;
    }

    virtual sample_context *create_new()
    {
      return new sample_context(NULL);
    }

    virtual void copy_to(sample_context *other)
    {
      other->created_with = this->created_with;
      other->samples_per_second = this->samples_per_second;
      other->default_volume = this->default_volume;
      other->num_samples = this->num_samples;
    }
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
          break;
      }

      tick -= node[idx - 1].tick;

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

  #include "Channel.h"

  namespace LoopStyle
  {
    enum Type
    {
      Forward,
      PingPong,
    };
  }

  struct sample_builtintype_context : sample_context
  {
    int last_looped_sample;

    int sustain_loop_exit_sample;
    int sustain_loop_exit_difference;

    SustainLoopState::Type sustain_loop_state;

    sample_builtintype_context(sample *cw) : sample_context(cw) { }

    virtual sample_context *create_new()
    {
      return new sample_builtintype_context(NULL);
    }

    virtual void copy_to(sample_context *other)
    {
      sample_builtintype_context *target = dynamic_cast<sample_builtintype_context *>(other);

      if (target == NULL)
        throw "Copy sample context to wrong type";

      sample_context::copy_to(target);

      target->last_looped_sample = this->last_looped_sample;
      target->sustain_loop_exit_difference = this->sustain_loop_exit_difference;
      target->sustain_loop_exit_sample = this->sustain_loop_exit_sample;
      target->sustain_loop_state = this->sustain_loop_state;
    }
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

    LoopStyle::Type loop_style, sustain_loop_style;

    bool use_sustain_loop;

    virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **c = NULL, double effect_tick_length = 0, bool top_level = true, int *znote = NULL)
    {
      if (c == NULL)
        throw "need sample context";

      if (*c)
      {
        (*c)->created_with->occlude_note(p, c, this, r);
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
      context.sustain_loop_exit_sample = 0x7FFFFFFF;
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

      if ((sample < 0)
       || ((sample >= num_samples)
        && (loop_end == 0xFFFFFFFF)
        && (!use_sustain_loop)))
        return one_sample(output_channels);

      int subsequent_sample = sample + 1;

      switch (context.sustain_loop_state)
      {
        case SustainLoopState::Running:
        case SustainLoopState::Finishing:
          int new_sample;
          int new_subsequent_sample;
          int next_clean_exit_sample;

          if ((context.sustain_loop_state == SustainLoopState::Finishing)
           && (sample >= context.sustain_loop_exit_sample))
          {
            new_sample = sample - context.sustain_loop_exit_difference;
            new_subsequent_sample = new_sample + 1;
            context.sustain_loop_state = SustainLoopState::Finished;
          }
          else if (sample > sustain_loop_end)
          {
            int sustain_loop_length = sustain_loop_end - sustain_loop_begin;
            double overrun = (sample + offset) - sustain_loop_end;
            int direction = -1;

            next_clean_exit_sample = sustain_loop_end + sustain_loop_length;

            while (overrun > sustain_loop_length)
            {
              next_clean_exit_sample += sustain_loop_length;
              overrun -= sustain_loop_length;
              direction = -direction;
            }

            if (sustain_loop_style == LoopStyle::Forward)
              direction = 1;

            if (direction < 0)
            {
              next_clean_exit_sample += sustain_loop_length;
              new_sample = sustain_loop_end - overrun;
            }
            else
              new_sample = sustain_loop_begin + overrun;

            new_subsequent_sample = new_sample + direction;

            if (new_subsequent_sample < sustain_loop_begin)
              new_subsequent_sample = sustain_loop_begin + 1;
            if (new_subsequent_sample > sustain_loop_end)
              new_subsequent_sample = sustain_loop_begin + (sustain_loop_length - direction) % sustain_loop_length;

            if (context.sustain_loop_state < SustainLoopState::Finishing)
            {
              context.sustain_loop_exit_sample = next_clean_exit_sample;
              context.sustain_loop_exit_difference = next_clean_exit_sample - sustain_loop_end;
            }
          }
          else
          {
            new_sample = sample;
            new_subsequent_sample = subsequent_sample;
          }

          sample = new_sample;
          subsequent_sample = new_subsequent_sample;

          break;
        case SustainLoopState::Finished:
          sample -= context.sustain_loop_exit_difference; // intentionally fall through
        case SustainLoopState::Off:
          if ((sample > loop_end) && (loop_end != 0xFFFFFFFF))
          {
            int loop_length = loop_end - loop_begin;
            double overrun = (sample + offset) - loop_end;
            int direction = -1;

            while (overrun > loop_length)
            {
              overrun -= loop_length;
              direction = -direction;
            }

            if (loop_style == LoopStyle::Forward)
              direction = 1;

            if (direction < 0)
              new_sample = loop_end - overrun;
            else
              new_sample = loop_begin + overrun;

            new_subsequent_sample = new_sample + direction;

            if (new_subsequent_sample < loop_begin)
              new_subsequent_sample = loop_begin + 1;
            if (new_subsequent_sample > loop_end)
              new_subsequent_sample = loop_begin + (loop_length - direction) % loop_length;

            sample = new_sample;
            subsequent_sample = new_subsequent_sample;
          }

          break;
      }

      double before, after;

      one_sample ret(sample_channels);

      ret.reset();

      for (int i=0; i<sample_channels; i++)
      {
        before = sample_data[i][sample];
        if (subsequent_sample < num_samples)
          after = sample_data[i][subsequent_sample];
        else
          after = 0.0;

        ret.next_sample() = bilinear(before, after, offset);
      }

      return ret.scale(sample_scale).set_channels(output_channels);
    }

    sample_builtintype(int index, int sample_channels,
                      T **data = NULL, int num_samples = 0,
                      long loop_begin = 0, long loop_end = 0xFFFFFFFF,
                      long susloop_begin = 0, long susloop_end = 0xFFFFFFFF,
                      LoopStyle::Type loop_style = LoopStyle::Forward,
                      LoopStyle::Type sustain_loop_style = LoopStyle::Forward)
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

      this->loop_style = loop_style;
      this->sustain_loop_style = sustain_loop_style;

      use_sustain_loop = (susloop_end != 0xFFFFFFFF);
    }
  };

  #include "Load_Sample.h"

  double global_volume = 1.0;
  vector<sample *> samples;

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
  bool anticlick = false;

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

  #include "Channel_DYNAMIC.h"

  #include "Channel_PLAY.h"

  #include "Channel_MODULE.h"

  struct sample_instrument_context : sample_context
  {
    sample *cur_sample;
    sample_context *cur_sample_context;
    double cur_volume;
    SustainLoopState::Type sustain_loop_state;
    channel *owner_channel;
    double effect_tick_length;
    int inote;

    sample_instrument_context(sample *cw) : sample_context(cw) {}

    virtual ~sample_instrument_context()
    {
      sample_context::~sample_context();
      if (cur_sample_context != NULL)
        delete cur_sample_context;
    }

    virtual sample_context *create_new()
    {
      return new sample_instrument_context(NULL);
    }

    virtual void copy_to(sample_context *other)
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

      if ((*context != NULL) && (p != NULL))
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

        if (fade_per_tick < 0.1)
          fade_per_tick = 0.1;

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
        p->my_ancillary_channels.push_back(ancillary);
      }
    }

    virtual void begin_new_note(row *r = NULL, channel *p = NULL, sample_context **context = NULL, double effect_tick_length = 0, bool top_level = true, int *znote = NULL)
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
    transform(s.begin(), s.end(), s.begin(), std::function<int(int)>(::tolower));
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
    cerr << "usage: " << cmd_name << " [-play_overlap -play_no_overlap -play <PLAY files>] [-samples <sample files>]" << endl
        << "       " << indentws << " [-module <S3M filenames>] [-frame-based_portamento]" << endl
        << "       " << indentws << " [-anticlick] [-max_time <seconds>] [-max_ticks <ticks>]" << endl
        << "       " << indentws << " [-output <output_file>] [-amplify <factor>] [-compress]" << endl
        << "       " << indentws << " {-stereo | -mono} {-lsb | -msb | -system_byte_order}" << endl
        << "       " << indentws << " { {-8 | -16} [-unsigned] | {-32 | -64} | [-ulaw] | [-alaw] }" << endl
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

  #include "uLaw-aLaw.h"
}

using namespace MultiPLAY;

int main(int argc, char *argv[])
{
  double max_time = -1.0;
  long max_ticks = -1;
  string output_filename("output.raw");
  bool output_file = false;
  int bits = 16;
  bool unsigned_samples = false, ulaw = false, alaw = false;
  direct_output::type direct_output_type = direct_output::none;
  bool stereo_output = true;
  bool looping = false;
  vector<string> play_filenames, play_sample_filenames, module_filenames;
  vector<bool> play_overlap_notes;
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

  bool next_play_overlap_notes = false;

  for (int i=1; i<argc; i++)
  {
    string arg(argv[i]);

    if ((arg == "-?") || (arg == "-h") || (arg == "-help") || (arg == "--help"))
    {
      show_usage(argv[0]);
      return 0;
    }
    else if (arg == "-play_overlap")
      next_play_overlap_notes = true;
    else if (arg == "-play_no_overlap")
      next_play_overlap_notes = false;
    else if (arg == "-play")
    {
      i++;
      if (i >= argc)
        cerr << argv[0] << ": missing argument for parameter -play" << endl;
      else
      {
        expect_filenames(i, argc, argv, play_filenames);

        while (play_overlap_notes.size() < play_filenames.size())
          play_overlap_notes.push_back(next_play_overlap_notes);
      }
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
    else if (arg == "-anticlick")
      anticlick = true;
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
      {
        output_file = true;
        output_filename = argv[i];
      }
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
    else if (arg == "-ulaw")
      ulaw = true, alaw = false;
    else if (arg == "-alaw")
      alaw = true, ulaw = false;
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

  try
  {
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
        channels.push_back(new channel_PLAY(file, looping, play_overlap_notes[i]));
    }

    for (int i=0; i < int(play_sample_filenames.size()); i++)
    {
      ifstream file(play_sample_filenames[i].c_str(), ios::binary);
      if (!file.is_open())
        cerr << argv[0] << ": unable to open sample file: " << play_sample_filenames[i] << endl;
      else
      {
        MultiPLAY::sample *this_sample;

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

    if (ulaw || alaw)
      if (direct_output_type == direct_output::none)
      {
        bits = 16;
        unsigned_samples = false;
        init_ulaw_alaw();
      }
      else
        cerr << argv[0] << ": " << (ulaw ? "u" : "A") << "-Law can't be used with direct output systems (they expect PCM)" << endl;

    if ((bits > 16) && unsigned_samples)
    {
      cerr << argv[0] << ": cannot use unsigned samples with floating-point output (32 and 64 bit precision)" << endl;
      return 1;
    }

    int status;

    ofstream output;

  retry_output:
    switch (direct_output_type)
    {
      case direct_output::none:
        if (!output_file)
        {
  #if defined(SDL) && defined(SDL_DEFAULT)
          direct_output_type = direct_output::sdl;
          goto retry_output;
  #elif defined(DIRECTX) && defined(DIRECTX_DEFAULT)
          direct_output_type == direct_output::directx;
          goto retry_output;
  #else
          cerr << argv[0] << ": no output specified, use -output to write a raw PCM or aLaw/uLaw file" << endl;
          return 1;
  #endif
        }

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

    if (ulaw)
      cerr << "  mu-Law sample encoding" << endl;

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
                  else if (ulaw)
                    output.put(ulaw_encode_sample(sample_short));
                  else if (alaw)
                    output.put(alaw_encode_sample(sample_short));
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
  }
  catch (const char *message)
  {
    cerr << endl;
    cerr << "CRASH: " << message << endl;
  }

  shutdown_complete = true;

  return 0;
}
