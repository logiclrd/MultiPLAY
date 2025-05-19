#include "Load_Sample.h"

#include <cstring>
#include <fstream>

using namespace std;

#include "wave_file.h"

using namespace WaveFile;

#include "RAII.h"

using namespace RAII;

#include "conversion.h"
#include "sample.h"
#include "sample_builtintype.h"

namespace MultiPLAY
{
	sample *sample_from_file(ifstream *file, SampleFileType::Type filetype_hint/* = SampleFileType::Unknown*/)
	{
		char magic[5];

		magic[4] = 0;

		file->read(magic, 4);

		switch (filetype_hint)
		{
			case SampleFileType::RAW:
			case SampleFileType::Unknown:
				// Nothing to do.
				break;
			case SampleFileType::WAV:
				if (string(magic) != "RIFF")
				{
					cerr << "Warning: WAV file does not have the expected RIFF header" << endl;
					memcpy(magic, "RIFF", 4);
				}
				break;
			case SampleFileType::AIFF:
				if (string(magic) != "FORM")
				{
					cerr << "Warning: AIFF file does not have the expected FORM header" << endl;
					memcpy(magic, "FORM", 4);
				}
				break;
		}

		if ((filetype_hint != SampleFileType::RAW) && (string(magic) == "RIFF")) // then it's a WAV format file
		{
			unsigned char lsb_bytes[4];

			file->read((char *)&lsb_bytes[0], 4);
			//unsigned long file_length = from_lsb4_lu(lsb_bytes); // not including 8-byte RIFF header

			file->read((char *)magic, 4);
			if (string((char *)magic, 4) == "WAVE") // then it's a WAV audio file
			{
				WAV::chunk chunk = { { 0 } };
				WAV::format_chunk format_chunk = { 0 };
				bool have_format_chunk = false;

				while (true)
				{
					file->read(chunk.raw.chunkID, 4);
					file->read((char *)&lsb_bytes[0], 4);
					chunk.raw.chunkSize = unsigned(from_lsb4(lsb_bytes));

					if (!file->good())
						throw "unexpected end-of-file";

					if (chunk.chunkID() == "fmt ")
					{
						if (chunk.raw.chunkSize > sizeof(chunk))
						{
							file->ignore(streamsize(chunk.raw.chunkSize));
							continue;
						}

						file->read((char *)&lsb_bytes[0], 2);
						chunk.format.wFormatTag = from_lsb2(lsb_bytes);

						file->read((char *)&lsb_bytes[0], 2);
						chunk.format.wChannels = from_lsb2_u(lsb_bytes);

						file->read((char *)&lsb_bytes[0], 4);
						chunk.format.dwSamplesPerSec = from_lsb4_lu(lsb_bytes);

						file->read((char *)&lsb_bytes[0], 4);
						chunk.format.dwAvgBytesPerSec = from_lsb4_lu(lsb_bytes);

						file->read((char *)&lsb_bytes[0], 2);
						chunk.format.wBlockAlign = from_lsb2_u(lsb_bytes);

						file->read((char *)&lsb_bytes[0], 2);
						chunk.format.wBitsPerSample = from_lsb2_u(lsb_bytes);

						format_chunk = chunk.format;
						have_format_chunk = true;
					}

					if (!file->good())
						throw "unexpected end-of-file";

					if (chunk.chunkID() == "data")
					{
						if (!have_format_chunk)
							throw "did not find a valid fmt chunk before data";

						if (format_chunk.wFormatTag != 1)
							throw "unknown file encoding";

						if (format_chunk.wBitsPerSample <= 8)
						{
							sample_builtintype<signed char> *smp;

							smp = new sample_builtintype<signed char>(0, format_chunk.wChannels, 1.0);

							smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
							smp->samples_per_second = format_chunk.dwSamplesPerSec;
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed char[smp->num_samples];

							int padding = format_chunk.wBlockAlign - format_chunk.wChannels;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<smp->sample_channels; j++)
								{
									signed char sample = 0;
									sample = (signed char)(int((unsigned char)file->get()) - 128);
									smp->sample_data[j][i] = sample;
								}
								if (padding)
									file->ignore(padding);
							}

							return smp;
						}

						if (format_chunk.wBitsPerSample <= 16)
						{
							sample_builtintype<signed short> *smp;

							smp = new sample_builtintype<signed short>(0, format_chunk.wChannels, 1.0);

							smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
							smp->samples_per_second = format_chunk.dwSamplesPerSec;
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed short[smp->num_samples];

							int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 2;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<smp->sample_channels; j++)
								{
									file->read((char *)&lsb_bytes[0], 2);
									smp->sample_data[j][i] = from_lsb2(lsb_bytes);
								}
								if (padding)
									file->ignore(padding);
							}

							return smp;
						}

						if (format_chunk.wBitsPerSample <= 24)
						{
							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, format_chunk.wChannels, 1.0);

							smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
							smp->samples_per_second = format_chunk.dwSamplesPerSec;
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 3;

							lsb_bytes[0] = 0;
							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<smp->sample_channels; j++)
								{
									file->read((char *)&lsb_bytes[1], 3);
									smp->sample_data[j][i] = from_lsb4_l(lsb_bytes);
								}
								if (padding)
									file->ignore(padding);
							}

							return smp;
						}

						if (format_chunk.wBitsPerSample <= 32)
						{
							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, format_chunk.wChannels, 1.0);

							smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
							smp->samples_per_second = format_chunk.dwSamplesPerSec;
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 4;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<format_chunk.wChannels; j++)
								{
									file->read((char *)&lsb_bytes[0], 4);
									smp->sample_data[j][i] = from_lsb4_l(lsb_bytes);
								}
								if (padding)
									file->ignore(padding);
							}

							return smp;
						}

						if (format_chunk.wBitsPerSample > 32)
						{
							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, format_chunk.wChannels, 1.0);

							smp->num_samples = chunk.raw.chunkSize / format_chunk.wBlockAlign;
							smp->samples_per_second = format_chunk.dwSamplesPerSec;
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							int padding = format_chunk.wBlockAlign - format_chunk.wChannels * 4;
							int differential = (format_chunk.wBitsPerSample - 25) / 8;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<smp->sample_channels; j++)
								{
									file->ignore(differential);
									file->read((char *)&lsb_bytes[0], 4);
									smp->sample_data[j][i] = from_lsb4(lsb_bytes);
								}
								if (padding)
									file->ignore(padding);
							}

							return smp;
						}

						throw "format was not recognized";
					}
				}
				throw "no data chunk was found";
			}
			else
				throw "RIFF type is not WAVE";
		}
		else if ((filetype_hint != SampleFileType::RAW) && (string(magic, 4) == "FORM")) // then it's probably an AIFF format file
		{
			unsigned char msb_bytes[4];

			unsigned long file_length; // not including 8-byte RIFF header
			file->read((char *)&msb_bytes[0], 4);
			file_length = unsigned(from_msb4(msb_bytes));

			file->read(magic, 4);
			if (string(magic, 4) == "AIFF") // then it's an AIFF audio file
			{
				AIFF::chunk chunk = { 0 };
				AIFF::common_chunk common_chunk = { 0 };
				bool have_common_chunk = false;

				while (true)
				{
					file->read(chunk.raw.chunkID, 4);
					file->read((char *)&msb_bytes[0], 4);
					chunk.raw.chunkSize = unsigned(from_msb4(msb_bytes));

					if (!file->good())
						throw "unexpected end-of-file";

					if (chunk.chunkID() == "COMM")
					{
						if (chunk.raw.chunkSize > sizeof(chunk) - 8)
						{
							file->ignore(streamsize(chunk.raw.chunkSize));
							continue;
						}

						file->read((char *)&msb_bytes[0], 2);
						chunk.common.numChannels = from_msb2(msb_bytes);

						file->read((char *)&msb_bytes[0], 4);
						chunk.common.numSampleFrames = unsigned(from_msb4(msb_bytes));

						file->read((char *)&msb_bytes[0], 2);
						chunk.common.sampleSize = from_msb2(msb_bytes);

						file->read((char *)&chunk.common.sampleRate.data[0], 10);
						
						common_chunk = chunk.common;
						have_common_chunk = true;
					}

					if (!file->good())
						throw "unexpected end-of-file";

					if (chunk.chunkID() == "SSND")
					{
						if (!have_common_chunk)
							throw "did not find a valid COMM chunk before SSND";

						file->read((char *)&msb_bytes[0], 4);
						chunk.ssnd.offset = unsigned(from_msb4(msb_bytes));

						file->read((char *)&msb_bytes[0], 4);
						chunk.ssnd.blockSize = unsigned(from_msb4(msb_bytes));
						
						if (common_chunk.sampleSize <= 8)
						{
							sample_builtintype<signed char> *smp;

							smp = new sample_builtintype<signed char>(0, common_chunk.numChannels, 1.0);

							smp->num_samples = common_chunk.numSampleFrames;
							smp->samples_per_second = common_chunk.sampleRate.toDouble();
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed char[smp->num_samples];

							if (chunk.ssnd.blockSize == 0)
							{
								chunk.ssnd.blockSize = 1;
								chunk.ssnd.offset = 0;
							}

							ArrayAllocator<char> block_allocator(chunk.ssnd.blockSize);
							char *block = block_allocator.getArray();

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<common_chunk.numChannels; j++)
								{
									file->read(block, streamsize(chunk.ssnd.blockSize));
									smp->sample_data[j][i] = (signed char)block[chunk.ssnd.offset];
								}
							}

							return smp;
						}

						if (common_chunk.sampleSize <= 16)
						{
							sample_builtintype<signed short> *smp;

							smp = new sample_builtintype<signed short>(0, common_chunk.numChannels, 1.0);

							smp->num_samples = common_chunk.numSampleFrames;
							smp->samples_per_second = common_chunk.sampleRate.toDouble();
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed short[smp->num_samples];

							if (chunk.ssnd.blockSize == 0)
							{
								chunk.ssnd.blockSize = 2;
								chunk.ssnd.offset = 0;
							}

							ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
							unsigned char *block = block_allocator.getArray();

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<common_chunk.numChannels; j++)
								{
									file->read((char *)block, streamsize(chunk.ssnd.blockSize));
									smp->sample_data[j][i] = from_msb2(block + chunk.ssnd.offset);
								}
							}

							return smp;
						}

						if (common_chunk.sampleSize <= 24)
						{
							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, common_chunk.numChannels, 1.0);

							smp->num_samples = common_chunk.numSampleFrames;
							smp->samples_per_second = common_chunk.sampleRate.toDouble();
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							if (chunk.ssnd.blockSize == 0)
							{
								chunk.ssnd.blockSize = 3;
								chunk.ssnd.offset = 0;
							}

							ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize + 1);
							unsigned char *block = block_allocator.getArray();;
							block[chunk.ssnd.blockSize] = 0;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<common_chunk.numChannels; j++)
								{
									file->read((char *)block, unsigned(chunk.ssnd.blockSize));
									smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
								}
							}

							return smp;
						}

						if (common_chunk.sampleSize <= 32)
						{
							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, common_chunk.numChannels, 1.0);

							smp->num_samples = common_chunk.numSampleFrames;
							smp->samples_per_second = common_chunk.sampleRate.toDouble();
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							if (chunk.ssnd.blockSize == 0)
							{
								chunk.ssnd.blockSize = 1;
								chunk.ssnd.offset = 0;
							}

							ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
							unsigned char *block = block_allocator.getArray();;

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<common_chunk.numChannels; j++)
								{
									file->read((char *)block, unsigned(chunk.ssnd.blockSize));
									smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
								}
							}

							return smp;
						}

						if (common_chunk.sampleSize > 32)
						{
							cerr << "Warning: AIFF sample size is greater than 32 bits (non-standard)" << endl;

							sample_builtintype<signed long> *smp;

							smp = new sample_builtintype<signed long>(0, common_chunk.numChannels, 1.0);

							smp->num_samples = common_chunk.numSampleFrames;
							smp->samples_per_second = common_chunk.sampleRate.toDouble();
							for (int i=0; i<smp->sample_channels; i++)
								smp->sample_data[i] = new signed long[smp->num_samples];

							if (chunk.ssnd.blockSize == 0)
							{
								chunk.ssnd.blockSize = ((unsigned long)(common_chunk.sampleSize) + 7) / 8;
								chunk.ssnd.offset = chunk.ssnd.blockSize - 4;
							}

							ArrayAllocator<unsigned char> block_allocator(chunk.ssnd.blockSize);
							unsigned char *block = block_allocator.getArray();

							for (unsigned i=0; i<smp->num_samples; i++)
							{
								for (int j=0; j<common_chunk.numChannels; j++)
								{
									file->read((char *)block, streamsize(chunk.ssnd.blockSize));
									smp->sample_data[j][i] = from_msb4(block + chunk.ssnd.offset);
								}
							}

							return smp;
						}

						throw "format was not recognized";
					}
				}
			}
		}
		else // raw data
		{
			int diff_8bit = 0;
			int diff_16bit = 0;
			int diff_16bit_msb = 0;
			for (int i=0; i<500; i++)
			{
				diff_8bit += (magic[3] - magic[2]) + (magic[1] - magic[0]);
				diff_16bit += (magic[3] - magic[1]);
				diff_16bit_msb += (magic[2] - magic[0]);
				file->read(magic, 4);
				if (file->eof())
					break;
			}

			file->seekg(0, ios::end);
			long length = long(file->tellg());
			file->seekg(0, ios::beg);

			if ((diff_8bit < diff_16bit) && (diff_8bit < diff_16bit_msb)) // then try for 8-bit data :-)
			{
				unsigned char *data = new unsigned char[unsigned(length)];

				file->read((char *)data, length);

				// now find signed-ness
				int delta = data[1] - data[0];

				int signed_score = 0;
				int unsigned_score = 0;

				for (int i=2; i<length; i++)
				{
					if ((data[1] < 0x80) && (data[2] >= 0x80))
					{
						if (delta > 0)
							unsigned_score++;
						else
							signed_score++;
					}
					else if ((data[1] >= 0x80) && (data[2] < 0x80))
					{
						if (delta > 0)
							signed_score++;
						else
							unsigned_score++;
					}
				}

				signed char *data_sgn = (signed char *)data;

				if (unsigned_score > signed_score)
					for (int i=0; i<length; i++)
						data_sgn[i] = (signed char)(int(data[i]) - 128);

				sample_builtintype<signed char> *smp = new sample_builtintype<signed char>(0, 1, 1.0);

				smp->num_samples = length;
				smp->sample_data[0] = data_sgn;
				smp->samples_per_second = 8363; // typical default

				return smp;
			}
			else if ((diff_16bit < diff_8bit) && (diff_16bit < diff_16bit_msb))
			{
				if (length & 1)
					cerr << "Warning: sample data looks 16-bit, but byte length is not even" << endl;

				length >>= 1;

				unsigned short *data = new unsigned short[unsigned(length)];

				file->read((char *)data, length);

				// now find signed-ness
				int delta = data[1] - data[0];

				int signed_score = 0;
				int unsigned_score = 0;

				for (int i=2; i<length; i++)
				{
					if ((data[1] < 0x8000) && (data[2] >= 0x8000))
					{
						if (delta > 0)
							unsigned_score++;
						else
							signed_score++;
					}
					else if ((data[1] >= 0x8000) && (data[2] < 0x8000))
					{
						if (delta > 0)
							signed_score++;
						else
							unsigned_score++;
					}
				}

				signed short *data_sgn = (signed short *)data;

				if (unsigned_score > signed_score)
					for (int i=0; i<length; i++)
						data_sgn[i] = (signed short)(int(data[i]) - 32768);

				sample_builtintype<signed short > *smp = new sample_builtintype<signed short >(0, 1, 1.0);

				smp->num_samples = length >> 1;
				smp->sample_data[0] = data_sgn;
				smp->samples_per_second = 8363; // typical default

				return smp;
			}
			else if ((diff_16bit_msb < diff_8bit) && (diff_16bit_msb < diff_16bit))
			{
				if (length & 1)
					cerr << "Warning: sample data looks 16-bit, but byte length is not even" << endl;

				length >>= 1;

				unsigned char msb_bytes[2];

				unsigned short *data = new unsigned short[unsigned(length)];

				for (int i=0; i<length; i++)
				{
					file->read((char *)&msb_bytes[0], 2);
					data[i] = from_msb2_u(msb_bytes);
				}

				// now find signed-ness
				int delta = data[1] - data[0];

				int signed_score = 0;
				int unsigned_score = 0;

				for (int i=2; i<length; i++)
				{
					if ((data[1] < 0x8000) && (data[2] >= 0x8000))
					{
						if (delta > 0)
							unsigned_score++;
						else
							signed_score++;
					}
					else if ((data[1] >= 0x8000) && (data[2] < 0x8000))
					{
						if (delta > 0)
							signed_score++;
						else
							unsigned_score++;
					}
				}

				signed short *data_sgn = (signed short *)data;

				if (unsigned_score > signed_score)
					for (int i=0; i<length; i++)
						data_sgn[i] = (signed short)(int(data[i]) - 32768);

				sample_builtintype<signed short > *smp = new sample_builtintype<signed short >(0, 1, 1.0);

				smp->num_samples = length >> 1;
				smp->sample_data[0] = data_sgn;
				smp->samples_per_second = 8363; // typical default

				return smp;
			}
		}

		throw "format was not recognized";
	}
}