#include "Channel_PLAY.h"

#include <iostream>
#include <sstream>

using namespace std;

#include "MultiPLAY.h"

#include "channel.h"

#include "Channel_DYNAMIC.h"

namespace MultiPLAY
{
	namespace
	{
		int next_play_channel_id = 0;

		//                           G# A#     C# D#    F# G#
		//                             A   B  C  D  E  F  G
		const int note_by_name[7] = {  9, 11, 0, 2, 4, 5, 7  };
	}

	// struct channel_PLAY
	channel_PLAY::channel_PLAY(istream *input, bool looping, bool overlap/* = false*/)
		: channel(looping, true),
			in(input)
	{
		stringstream ss;

		ss << "play_" << (next_play_channel_id++);

		this->identity = ss.str();

		if (in->get() < 0)
			finished = true;

		in->seekg(0, ios::end);
		in_length = (long)in->tellg();

		in->seekg(0, ios::beg);
		in_offset = 0;

		overlap_notes = overlap;

		looped = false;
	}

	/*virtual*/ channel_PLAY::~channel_PLAY()
	{
	}

	/*virtual*/ void channel_PLAY::get_playback_position(PlaybackPosition &position)
	{
		position.Order = 0;
		position.OrderCount = 0;
		position.Pattern = 0;
		position.PatternCount = 0;
		position.Row = 0;
		position.RowCount = 0;
		position.Offset = in_offset;
		position.OffsetCount = in_length;
		position.FormatString = "{Offset}/{OffsetCount}";
	}

	int channel_PLAY::pull_char()
	{
		looped = false;

		int ch = in->get();

		if (ch < 0)
		{
			if (looping)
			{
				in->seekg(0, ios::beg);
				in_offset = 0;

				looped = true;

				ch = in->get();
			}
			else
				finished = true;
		}

		in_offset++;

		return ch;
	}

	void channel_PLAY::push_char(int ch)
	{
		in->putback(char(ch));
		in_offset--;
	}

	int channel_PLAY::expect_int()
	{
		int built_up = 0;

		while (true)
		{
			int ch = pull_char();

			if (finished)
				return built_up;

			if ((ch >= '0') && (ch <= '9'))
				built_up = (built_up * 10) + (ch - '0');
			else
			{
				push_char(ch);
				return built_up;
			}
		}
	}

	int channel_PLAY::accidental()
	{
		int ch = pull_char();

		if (ch == '-')
			return -1;
		if ((ch == '+') || (ch == '#'))
			return +1;

		if (ch > 0)
			push_char(ch);

		return 0;
	}

	double channel_PLAY::expect_duration(int *note_length_denominator/* = NULL*/)
	{
		double duration = 1.0;
		double duration_add = 0.5;

		int ch = pull_char();

		if (isdigit(ch) && (note_length_denominator != NULL))
		{
			push_char(ch);
			*note_length_denominator = expect_int();
		}

		while (ch == '.')
		{
			duration += duration_add;
			duration_add /= 2.0;

			ch = pull_char();
		}

		if (ch >= 0)
			push_char(ch);

		return duration;
	}

	/*virtual*/ ChannelPlaybackState::Type channel_PLAY::advance_pattern(one_sample &sample, Profile &/*profile*/)
	{
		while (!ticks_left)
		{
			int ch = pull_char();

			if (finished)
			{
				sample = 0.0;
				return ChannelPlaybackState::Finished;
			}

			while (ch == '\'')
			{
				while (true)
				{
					ch = pull_char();

					if (finished)
					{
						sample = 0.0;
						return ChannelPlaybackState::Finished;
					}

					if (ch == '\n')
						break;
				}

				ch = pull_char();

				if (finished)
				{
					sample = 0.0;
					return ChannelPlaybackState::Finished;
				}
			}

			if (isspace(ch))
				continue;

			int znote, rests;
			double duration_scale = 1.0;

			switch (ch)
			{
				case 'o':
				case 'O':
					octave = expect_int();
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
				{
					znote = note_by_name[tolower(ch) - 'a'] + 12*octave + accidental() + 1;
					this_note_length_denominator = note_length_denominator;
					duration_scale = expect_duration(&this_note_length_denominator);
					if (note_length_denominator && (this_note_length_denominator == 0))
					{
						cerr << "Warning: invalid note length denominator " << this_note_length_denominator << endl;
						this_note_length_denominator = note_length_denominator;
					}

					bool use_sample = (current_waveform == Waveform::Sample) && (current_sample != NULL);

					if (!use_sample)
						recalc(znote, duration_scale);
					else
					{
						current_sample->begin_new_note(NULL, this, &current_sample_context, 0.02 * ticks_per_second, true, &znote);

						recalc(znote, duration_scale);

						if (overlap_notes)
							ancillary_channels.push_back(channel_DYNAMIC::assume_note(this));
					}
					break;
				}
				case 'n':
				case 'N':
					znote = expect_int();
					duration_scale = expect_duration();
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
					note_length_denominator = expect_int();
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
					ch = pull_char();

					if (finished)
					{
						sample = 0.0;
						return ChannelPlaybackState::Finished;
					}

					if (looped)
						break;

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
					rests = expect_int();
					duration_scale = expect_duration();
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
					tempo = expect_int();
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
					ch = pull_char();

					if (finished)
					{
						sample = 0.0;
						return ChannelPlaybackState::Finished;
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
						{
							push_char(ch);

							unsigned sample_number = unsigned(expect_int());

							if (sample_number >= samples.size())
								cerr << "Warning: Sample number " << sample_number << " out of range" << endl;
							else
							{
								current_sample = samples[sample_number];
								if (current_sample == NULL)
									current_sample_context = NULL;
								current_waveform = Waveform::Sample;
							}
							
							break;
						}
						default:
							cerr << "Warning: Syntax error in input: W" << char(toupper(ch)) << endl;
					}
					break;
			}
		}

		return ChannelPlaybackState::Ongoing;
	}
}
