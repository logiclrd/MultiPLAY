#ifndef LOAD_SAMPLE_H
#define LOAD_SAMPLE_H

#include "conversion.h"
#include "sample.h"

namespace MultiPLAY
{
	namespace SampleFileType
	{
		enum Type
		{
			Unknown,
			WAV,
			AIFF,
			RAW,
		};
	}

	sample *sample_from_file(ifstream *file, SampleFileType::Type filetype_hint = SampleFileType::Unknown);
}

#endif // LOAD_SAMPLE_H
