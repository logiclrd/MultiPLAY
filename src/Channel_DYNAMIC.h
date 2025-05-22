#ifndef CHANNEL_DYNAMIC_H
#define CHANNEL_DYNAMIC_H

#include "Profile.h"

#include "channel.h"
#include "one_sample.h"
#include "sample.h"

namespace MultiPLAY
{
	struct channel_DYNAMIC : channel
	{
		channel *parent_channel;

		channel_DYNAMIC(channel *spawner, sample *sample, sample_context *context);
		~channel_DYNAMIC();

		static channel_DYNAMIC *assume_note(channel *spawner, bool spawner_note_cut = true);

		virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile);
		virtual void get_playback_position(PlaybackPosition &position);
	};
}

#endif // CHANNEL_DYNAMIC_H