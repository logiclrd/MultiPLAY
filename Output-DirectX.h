#ifndef OUTPUT_DIRECTX_H
#define OUTPUT_DIRECTX_H

#ifdef DIRECTX

#include "one_sample.h"

namespace MultiPLAY
{
	extern int init_directx(long samples_per_sec, int channels, int bits);
	extern void emit_sample_to_directx_buffer(one_sample &sample);
	extern void shutdown_directx();
}

#endif // DIRECTX

#endif // OUTPUT_DIRECTX_H
