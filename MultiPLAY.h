#ifndef MULTIPLAY_H
#define MULTIPLAY_H

#include <vector>

namespace MultiPLAY
{
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

	extern int output_channels;
	extern int ticks_per_second;

	extern double global_volume;

	extern bool smooth_portamento_effect, trace_mod;
	extern bool anticlick;

	struct sample;
	struct channel;

	extern std::vector<sample *> samples;
	extern std::vector<channel *> channels;
	extern std::vector<channel *> ancillary_channels;

	extern void start_shutdown();

	extern bool shutdown_complete;
}

#endif // MULTIPLAY_H