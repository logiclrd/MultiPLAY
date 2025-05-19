#ifndef WAVE_FILE_H
#define WAVE_FILE_H

#include <string>

#include "MultiPLAY.h"

namespace WaveFile
{
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

			std::string chunkID();

			format_chunk format;
		};
	}

	struct ieee_extended
	{
		char data[10];

		double toDouble();
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

			std::string chunkID();

			common_chunk common;
			ssnd_chunk ssnd;
		};
	}
}

#endif // WAVE_FILE_H
