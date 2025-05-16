#ifndef CHANNEL_PLAY_H
#define CHANNEL_PLAY_H

#include <iostream>

#include "channel.h"
#include "progress.h"

namespace MultiPLAY
{
	struct channel_PLAY : public channel
	{
		std::istream *in;
		long in_offset, in_length;
		double actual_intensity;
		bool overlap_notes;

		channel_PLAY(std::istream *input, bool looping, bool overlap = false);
		virtual ~channel_PLAY();

		virtual void get_playback_position(PlaybackPosition &position);
		virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile);
	private:
		bool looped;

		int pull_char();
		void push_char(int ch);
		int expect_int();
		int accidental();
		double expect_duration(int *note_length_denominator = NULL);
	};
}

#endif // CHANNEL_PLAY_H