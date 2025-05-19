#ifndef OUTPUT_SDL_H
#define OUTPUT_SDL_H

#ifdef SDL

#include "one_sample.h"

namespace MultiPLAY
{
	extern int init_sdl(long samples_per_sec, int channels, int bits);
	extern void emit_sample_to_sdl_buffer(one_sample &sample);
	extern void shutdown_sdl();
}

#endif // SDL

#endif // OUTPUT_SDL_H