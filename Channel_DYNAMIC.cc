#include "Channel_DYNAMIC.h"

#include <algorithm>
#include <iterator>
#include <sstream>

using namespace std;

#include "Profile.h"

#include "channel.h"
#include "one_sample.h"
#include "sample.h"

namespace MultiPLAY
{
	namespace
	{
		int next_dynamic_channel_id = 0;
	}

	// struct channel_DYNAMIC
  channel_DYNAMIC::channel_DYNAMIC(channel &spawner, sample *sample, sample_context *context, double fade_per_tick)
    : channel(spawner.panning, false, true),
      parent_channel(spawner)
  {
    stringstream ss;

    ss << "dynamic_" << (next_dynamic_channel_id++);

    this->identity = ss.str();

    this->current_waveform = Waveform::Sample;
    this->current_sample = sample;
    this->current_sample_context = context->clone();
    this->note_frequency = spawner.note_frequency;
    this->delta_offset_per_tick = spawner.note_frequency / ticks_per_second;
    this->intensity = spawner.intensity;
    this->fade_per_tick = fade_per_tick;
    this->have_fade_per_tick = (fade_per_tick > 0);
    this->offset_major = spawner.offset_major;
    this->offset = spawner.offset;
    this->samples_this_note = spawner.samples_this_note;
    this->envelope_offset = spawner.envelope_offset;
    this->finish_with_fade = true;
  }

  /*virtual*/ ChannelPlaybackState::Type channel_DYNAMIC::advance_pattern(one_sample &/*sample*/, Profile &/*profile*/)
  {
    ticks_left = 1; // always have ticks left
    rest_ticks = 0;
    dropoff_start = 0;
    finished = finished
            || (fading && (fade_value <= 0.0))
            || (current_sample == NULL)
            || current_sample->past_end(offset_major, offset, current_sample_context);

    return ChannelPlaybackState::Ongoing;
  }

	/*virtual*/ void channel_DYNAMIC::get_playback_position(PlaybackPosition &position)
	{
		position.Order = 0;
		position.OrderCount = 0;
		position.Pattern = 0;
		position.PatternCount = 0;
		position.Row = 0;
		position.RowCount = 0;
		position.Offset = offset_major;
		position.OffsetCount = ticks_total;
		position.FormatString = "{Offset}/{OffsetCount}";
	}

  channel_DYNAMIC::~channel_DYNAMIC()
  {
    auto parent_position = find(parent_channel.my_ancillary_channels.begin(), parent_channel.my_ancillary_channels.end(), this);
    if (parent_position != parent_channel.my_ancillary_channels.end())
      parent_channel.my_ancillary_channels.erase(parent_position);
  }
}
