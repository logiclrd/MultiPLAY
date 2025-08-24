#include <clocale>
#include <csignal>
#include <cstring>
#include <sstream>
#include <cctype>
#include <cmath>

using namespace std;

#include "RAII.h"

using namespace RAII;

#include "uLaw-aLaw.h"

using namespace Telephony;

#include "Profile.h"

#include "bit_memory_stream.h"
#include "bit_value.h"
#include "break_handler.h"
#include "channel.h"
#include "conversion.h"
#include "effect.h"
#include "envelope.h"
#include "one_sample.h"
#include "math.h"
#include "module.h"
#include "mod_finetune.h"
#include "notes.h"
#include "pattern.h"
#include "progress.h"
#include "sample.h"
#include "sample_instrument.h"

#ifdef DIRECTX
#include "Output-DirectX.h"
#endif

#ifdef SDL
#include "Output-SDL.h"
#endif

#include "MultiPLAY.h"

#include "Load_Sample.h"

#include "Load_MOD.h"
#include "Load_MTM.h"
#include "Load_S3M.h"
#include "Load_IT.h"
#include "Load_XM.h"
#include "Load_UMX.h"

#include "Channel_DYNAMIC.h"
#include "Channel_PLAY.h"
#include "Channel_MODULE.h"

#include "OpenFile.h"

#include "charset/UTF8.h"

using namespace CharSet;

namespace MultiPLAY
{
	volatile bool shutting_down = false, shutdown_complete = false;

	extern void start_shutdown()
	{
		shutting_down = true;
	}

	int output_channels;
	int ticks_per_second;

	long long current_absolute_tick_number;

	double global_volume = 1.0;

	bool smooth_portamento_effect = true, trace_mod = false;
	bool anticlick = false;

	vector<sample *> samples;
	vector<channel *> channels;
	vector<channel *> ancillary_channels;
}

namespace MultiPLAY
{
	namespace
	{
		wstring get_extension_from_filename(const wstring &filename)
		{
			size_t offset = filename.find_last_of(L'.');
			wstring extension(filename.substr(offset + 1));

			make_lowercase(extension);

			return extension;
		}
	}

	module_struct *load_module(const wstring &filename)
	{
		ifstream input = File::OpenRead(filename);
		module_struct *module;

		if (!input.is_open())
			throw L"Failed to open file";

		wstring extension = get_extension_from_filename(filename);

		try
		{
			if (extension == L"mod")
				module = load_mod(&input);
			else if (extension == L"mud")
				module = load_mod(&input, true);
			else if (extension == L"mtm")
				module = load_mtm(&input);
			else if (extension == L"xm")
				module = load_xm(&input);
			else if (extension == L"s3m")
				module = load_s3m(&input);
			else if (extension == L"it")
				module = load_it(&input);
			else if (extension == L"shit")
				module = load_it(&input, true);
			else if (extension == L"umx")
				module = load_umx(&input);
			else
				throw L"unrecognized file extension";
		}
		catch (const wchar_t *e)
		{
			input.close();
			throw e;
		}

		input.close();

		if (module != nullptr)
		{
			module->filename = filename;

			// Cap the pattern order, in case the loader hasn't already done it.
			module->pattern_list_length = unsigned(module->pattern_list.size());
			module->pattern_list.push_back(&pattern::end_marker);
		}

		return module;
	}

	namespace direct_output
	{
		enum type
		{
			none,
	#ifdef DIRECTX
			directx,
	#endif
	#ifdef SDL
			sdl,
	#endif
		};
	};

	void show_usage(const wchar_t *cmd_name)
	{
		wstring indentws(wcslen(cmd_name), wchar_t(32));

		//       12345678901234567890123456789012345678901234567890123456789012345678901234567890
		wcerr << L"usage: " << cmd_name << L" [-play_overlap -play_no_overlap -play <PLAY files>] [-samples <sample files>]" << endl
		      << L"       " << indentws << L" [-module_start_pattern <pattern order number>] [-module <S3M filenames>]" << endl
		      << L"       " << indentws << L" [-frame-based_portamento] [-anticlick] [-max_time <seconds>]" << endl
		      << L"       " << indentws << L" [-max_ticks <ticks>] [-output <output_file>] [-output_per_pattern_row]" << endl
		      << L"       " << indentws << L" [-amplify <factor>] [-compress] {-stereo | -mono} " << endl
		      << L"       " << indentws << L" {-lsb | -msb | -system_byte_order}" << endl
		      << L"       " << indentws << L" { {-8 | -16} [-unsigned] | {-32 | -64} | [-ulaw] | [-alaw] }" << endl
		      << L"       " << indentws << L" [-sample_rate <samples_per_sec>] [-looping]" << endl
		      << L"       " << indentws
	#ifdef DIRECTX
		                                << L" [-directx]"
	#endif
	#ifdef SDL
		                                                  << L" [-sdl]"
	#endif
		                                                                << endl
				<< endl
				<< L"If a module start pattern number is specified, it applies to any -module" << endl
				<< L"specified after." << endl
				<< endl
				<< L"8- and 16-bit sample sizes are integers, -32 and -64 are floating-point (IEEE" << endl
				<< L"standard). The default output filename is 'output.wav'. The default byte order" << endl
				<< L"is system; -lsb and -msb force Intel and Motorola endianness, respectively." << endl
				<< L"The byte order setting applies only to integer output (8- and 16-bit samples)." << endl;
	}

	namespace
	{
		void expect_filenames(int &index, const wchar_t *argv[], vector<wstring> &collection)
		{
			while (argv[index])
			{
				if (argv[index][0] == L'-')
					break;

				if (argv[index][0] == L'"')
				{
					wstringstream filename;

					wstring arg(&argv[index][1]);

					size_t offset = arg.find(L'"');

					if (offset != wstring::npos)
						filename << arg.substr(0, arg.size() - 1);
					else
					{
						filename << arg;

						while (true)
						{
							index++;

							filename << L' ';

							arg = argv[index];

							offset = arg.find(L'"');

							if (offset == wstring::npos)
								filename << arg;
							else
							{
								filename << arg.substr(0, offset);
								break;
							}
						}
					}
					collection.push_back(filename.str());
				}
				else
					collection.push_back(wstring(argv[index++]));
			}
			index--;
		}

		ofstream output;
		int output_file_number = 0;
		bool output_per_pattern_row = false;
		wstring output_extension;
		wave_header wav_file_header;

		void start_new_output_file(const wchar_t *name)
		{
			if (output_per_pattern_row)
			{
				if (output.is_open())
				{
					wstringstream ss;

					ss << L"dump_" << setw(4) << setfill(L'0') << output_file_number++ << '_' << name << "." << output_extension;

					wav_file_header.finalize();

					output.close();
					output = File::OpenWrite(ss.str(), ios::binary | ios::trunc);

					wav_file_header.begin(&output);
				}
			}
		}

		bool is_empty(const wstring &str)
		{
			return trim(str) == L"";
		}

		void output_module_summary(const module_struct *module)
		{
			wstring header_line = wstring(L"\"") + module->name + L"\" (" + module->filename + L")";

			wcout << header_line << endl;
			for (wstring::size_type j=0, l=header_line.length(); j < l; j++)
				wcout.put(L'=');
			wcout << endl;

			if (module->samples.size() > 0)
			{
				auto &info_text = module->information_text;

				auto last_specified_sample_name_index = info_text.size() - 1;

				while ((last_specified_sample_name_index > 0)
						&& is_empty(info_text[last_specified_sample_name_index]))
					last_specified_sample_name_index--;

				int num_consecutive_empty = 0;

				for (vector<wstring>::size_type j=0; j <= last_specified_sample_name_index; j++)
				{
					const wstring &line = info_text[j];

					if (is_empty(line))
						num_consecutive_empty++;
					else
						num_consecutive_empty = 0;

					if ((num_consecutive_empty < 3) || (num_consecutive_empty == 10))
						wcout << line << endl;
				}
			}

			wcout << endl;
		}
	}

	extern void notify_new_pattern_row_started(const wchar_t *name)
	{
		start_new_output_file(name);
	}
}

using namespace MultiPLAY;

int wmain(int argc, const wchar_t *argv[])
{
	double max_time = -1.0;
	long max_ticks = -1;
	wstring output_filename(L"output.wav");
	bool output_file = false;
	int bits = 16;
	bool unsigned_samples = false, ulaw = false, alaw = false;
	direct_output::type direct_output_type = direct_output::none;
	bool stereo_output = true;
	bool looping = false;
	unsigned int module_start_pattern;
	vector<wstring> play_filenames, play_sample_filenames, module_filenames;
	vector<unsigned int> module_start_patterns;
	vector<bool> play_overlap_notes;
	vector<module_struct *> modules;
	bool lsb_output = false, msb_output = false;
	bool amplify = false;
	double amplify_by = 1.0;
	bool compress = false;

	ticks_per_second = 44100;

	if (argc < 2)
	{
		show_usage(argv[0]);
		return 0;
	}

	register_break_handler();

	bool next_play_overlap_notes = false;

	module_start_pattern = 0;

	for (int i=1; i<argc; i++)
	{
		wstring arg(argv[i]);

		if ((arg == L"-?") || (arg == L"-h") || (arg == L"-help") || (arg == L"--help"))
		{
			show_usage(argv[0]);
			return 0;
		}
		else if (arg == L"-play_overlap")
			next_play_overlap_notes = true;
		else if (arg == L"-play_no_overlap")
			next_play_overlap_notes = false;
		else if (arg == L"-play")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -play" << endl;
			else
			{
				expect_filenames(i, argv, play_filenames);

				while (play_overlap_notes.size() < play_filenames.size())
					play_overlap_notes.push_back(next_play_overlap_notes);
			}
		}
		else if ((arg == L"-sample") || (arg == L"-samples"))
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter " << arg << endl;
			else
				expect_filenames(i, argv, play_sample_filenames);
		}
		else if (arg == L"-module_start_pattern")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter " << arg << endl;
			else
				module_start_pattern = wcstoul(argv[i], nullptr, 10);
		}
		else if ((arg == L"-module") || (arg == L"-modules"))
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter " << arg << endl;
			else
			{
				expect_filenames(i, argv, module_filenames);

				while (module_start_patterns.size() < module_filenames.size())
					module_start_patterns.push_back(module_start_pattern);
			}
		}
		else if (arg == L"-frame-based_portamento")
			smooth_portamento_effect = false;
		else if (arg == L"-anticlick")
			anticlick = true;
		else if (arg == L"-trace")
			trace_mod = true;
		else if (arg == L"-max_seconds")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -max_seconds" << endl;
			else
				max_time = wcstod(argv[i], NULL);
		}
		else if (arg == L"-max_ticks")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -max_ticks" << endl;
			else
				max_ticks = wcstol(argv[i], NULL, 10);
		}
		else if (arg == L"-output")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -output" << endl;
			else
			{
				output_file = true;
				output_filename = argv[i];
			}
		}
		else if (arg == L"-output_per_pattern_row")
			output_per_pattern_row = true;
		else if (arg == L"-looping")
			looping = true;
		else if (arg == L"-msb")
			msb_output = true, lsb_output = false;
		else if (arg == L"-lsb")
		{
			msb_output = false;
			lsb_output = true;
		}
		else if (arg == L"-system_byte_order")
		{
			msb_output = false;
			lsb_output = false;
		}
		else if (arg == L"-stereo")
			stereo_output = true;
		else if (arg == L"-mono")
			stereo_output = false;
		else if (arg == L"-8")
			bits = 8;
		else if (arg == L"-16")
			bits = 16;
		else if (arg == L"-32")
			bits = 32;
		else if (arg == L"-64")
			bits = 64;
		else if (arg == L"-ulaw")
			ulaw = true, alaw = false;
		else if (arg == L"-alaw")
			alaw = true, ulaw = false;
		else if (arg == L"-unsigned")
			unsigned_samples = true;
		else if (arg == L"-sample_rate")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -samplerate" << endl;
			else
				ticks_per_second = abs(wcstol(argv[i], NULL, 10));
		}
		else if (arg == L"-amplify")
		{
			i++;
			if (i >= argc)
				wcerr << argv[0] << L": missing argument for parameter -amplify" << endl;
			else
				amplify = true,
				amplify_by = wcstod(argv[i], NULL);
		}
		else if (arg == L"-compress")
			compress = true;
#ifdef DIRECTX
		else if (arg == L"-directx")
			direct_output_type = direct_output::directx;
#endif
#ifdef SDL
		else if (arg == L"-sdl")
			direct_output_type = direct_output::sdl;
#endif
		else
			wcerr << argv[0] << L": unrecognized command-line parameter: " << arg << endl;
	}

	if (play_filenames.size() + module_filenames.size() == 0)
	{
		wcerr << argv[0] << L": nothing to play!" << endl << endl;
		show_usage(argv[0]);
		return 0;
	}

	output_channels = stereo_output ? 2 : 1;

	output_extension = get_extension_from_filename(output_filename);

	if (output_extension == L"wav")
	{
		if (bits == 8)
			unsigned_samples = true;
		else if (bits == 16)
		{
			if (unsigned_samples)
			{
				wcerr << L"ignoring -unsigned because 16-bit WAV files always use signed samples" << endl;
				unsigned_samples = false;
			}
		}

		wav_file_header.enable();
		wav_file_header.set_bits_per_sample(bits);
		wav_file_header.set_num_channels(output_channels);
		wav_file_header.set_samples_per_second(ticks_per_second);
	}

	try
	{
		for (vector<wstring>::size_type i=0; i < module_filenames.size(); i++)
		{
			module_struct *module;

			try
			{
				module = load_module(module_filenames[i]);
			}
			catch (const wchar_t *message)
			{
				wcerr << argv[0] << L": unable to load module file " << module_filenames[i] << ": " << message << endl;
				continue;
			}

			if (module)
			{
				module->initialize();

				module->current_pattern = module_start_patterns[i];

				if (module->current_pattern >= module->pattern_list.size())
					module->finished = true;

				output_module_summary(module);

				auto channel_group = new vector<channel_MODULE *>();

				for (unsigned j=0; j<MAX_MODULE_CHANNELS; j++)
				{
					auto channel = new channel_MODULE(channel_group, j, module, 64, looping, module->channel_enabled[j]);

					channels.push_back(channel);
					channel_group->push_back(channel);
				}

				modules.push_back(module);
			}
		}

		for (vector<wstring>::size_type i=0; i < play_filenames.size(); i++)
		{
			ifstream *file = File::OpenReadPtr(play_filenames[i]);
			if (!file->is_open())
			{
				delete file;
				wcerr << argv[0] << L": unable to open input file: " << play_filenames[i] << endl;
			}
			else
				channels.push_back(new channel_PLAY(file, looping, play_overlap_notes[i]));
		}

		for (vector<wstring>::size_type i=0; i < play_sample_filenames.size(); i++)
		{
			ifstream file = File::OpenRead(play_sample_filenames[i]);
			if (!file.is_open())
				wcerr << argv[0] << L": unable to open sample file: " << play_sample_filenames[i] << endl;
			else
			{
				MultiPLAY::sample *this_sample;

				try
				{
					this_sample = sample_from_file(&file);
				}
				catch (const wchar_t *error)
				{
					wcerr << argv[0] << L": unable to load sample file: " << play_sample_filenames[i] << " (" << error << ")" << endl;
					continue;
				}
				samples.push_back(this_sample);
			}
		}

		if (channels.size() == 0)
			return 1;

		if (ulaw || alaw)
		{
			if (direct_output_type == direct_output::none)
			{
				bits = 16;
				unsigned_samples = false;
				init_ulaw_alaw();
			}
			else
				wcerr << argv[0] << L": " << (ulaw ? L"u" : L"A") << L"-Law can't be used with direct output systems (they expect PCM)" << endl;
		}

		if ((bits > 16) && unsigned_samples)
		{
			wcerr << argv[0] << L": cannot use unsigned samples with floating-point output (32 and 64 bit precision)" << endl;
			return 1;
		}

		int status;

#if (defined(SDL) && defined(SDL_DEFAULT)) || (defined(DIRECTX) && defined(DIRECTX_DEFAULT))
	retry_output:
#endif
		switch (direct_output_type)
		{
			case direct_output::none:
			{
				if (!output_file)
				{
	#if defined(SDL) && defined(SDL_DEFAULT)
					direct_output_type = direct_output::sdl;
					goto retry_output;
	#elif defined(DIRECTX) && defined(DIRECTX_DEFAULT)
					direct_output_type == direct_output::directx;
					goto retry_output;
	#else
					wcerr << argv[0] << L": no output specified, use -output to write a raw PCM or aLaw/uLaw file" << endl;
					return 1;
	#endif
				}

				output = File::OpenWrite(output_filename.c_str());

				if (!output.is_open())
				{
					wcerr << argv[0] << L": unable to open output file: " << output_filename << endl;
					return 1;
				}

				wav_file_header.begin(&output);

				status = 0; // squelch warning if no direct output modes are enabled

				break;
			}
	#ifdef DIRECTX
			case direct_output::directx:
			{
				status = init_directx(ticks_per_second, output_channels, bits);

				if (status)
					return status;

				break;
			}
	#endif
	#ifdef SDL
			case direct_output::sdl:
			{
				status = init_sdl(ticks_per_second, output_channels, bits);

				if (status)
					return status;

				break;
			}
	#endif
		}

		if (max_time >= 0.0)
			max_ticks = long(max_time * ticks_per_second);

		for (auto i = modules.begin(), l = modules.end(); i != l; ++i)
		{
			module_struct *module = *i;

			wcerr << "Rendering " << module->filename << ":" << endl
				<< "  \"" << trim(module->name) << "\"" << endl
				<< "  " << module->num_channels << " channels" << endl;
		}

		switch (direct_output_type)
		{
			case direct_output::none:
				wcerr << "Output to: " << output_filename << endl;
				break;
	#ifdef DIRECTX
			case direct_output::directx:
				wcerr << "Direct output: DirectX" << endl;
				break;
	#endif
	#ifdef SDL
			case direct_output::sdl:
				wcerr << "Direct output: SDL" << endl;
				break;
	#endif
		}

		wcerr << "  " << output_channels << " output channels" << endl
			<< "  " << ticks_per_second << " samples per second" << endl
			<< "  " << bits << " bits per sample";

		if (bits > 16)
			wcerr << " (floating-point)";

		if (unsigned_samples)
			wcerr << " (unsigned)";

		wcerr << endl;

		if (ulaw)
			wcerr << "  mu-Law sample encoding" << endl;

		if (looping)
			wcerr << "  looping" << endl;

		if (max_ticks > 0)
			wcerr << "  time limit " << max_ticks << " samples (" << (double(max_ticks) / ticks_per_second) << " seconds)" << endl;

		current_absolute_tick_number = 0;

		PlaybackPosition playback_position, last_rendered_playback_position, last_rendered_playback_position_fields;
		PlaybackTime playback_time, last_rendered_playback_time;

		while (true)
		{
			Profile profile;

			profile.push_back("initialize sample");

			one_sample sample(output_channels);
			int count = 0;

			for (auto i = channels.begin(), l = channels.end(); i != l; ++i)
			{
				profile.push_back("process a channel");

				if ((*i)->finished)
					continue;

				sample += (*i)->calculate_next_tick();
				count++;
			}

			for (int i = int(ancillary_channels.size() - 1); i >= 0; --i)
			{
				profile.push_back("process an ancillary channel");

				channel *chan = ancillary_channels[unsigned(i)];

				if (chan->finished)
				{
					auto dynamic_chan = dynamic_cast<channel_DYNAMIC *>(chan);

					if (dynamic_chan != nullptr)
						dynamic_chan->parent_channel->remove_ancillary_channel(chan);

					delete chan;
					ancillary_channels.erase(ancillary_channels.begin() + i);
					continue;
				}

				sample += chan->calculate_next_tick();
				count++;
			}

			if (count == 0)
				break;

			profile.push_back("apply global volume");

			sample *= global_volume;

			if (amplify)
				sample *= amplify_by;

			if (compress)
			{
				profile.push_back("apply compression");

				bool overdriven = false;
				sample.reset();

				while (sample.has_next())
				{
					double this_sample = sample.next_sample();

					if ((this_sample < -1.0) || (this_sample > 1.0))
					{
						amplify = true;
						amplify_by /= fabs(this_sample);
						overdriven = true;
					}
				}

				if (overdriven)
					wcerr << "Warning: overdriven output was detected. Output has been compressed." << endl;
			}

			profile.push_back("send to direct output");

			switch (direct_output_type)
			{
				case direct_output::none:
					sample.reset();
					for (int i=0; i<sample.num_samples; i++)
						switch (bits)
						{
							case 8:
							{
								signed char sample_char = (signed char)(127 * sample.next_sample());

								if (unsigned_samples)
								{
									unsigned char sample_uchar = (unsigned char)(int(sample_char) + 128); // bias sample
									output.put(char(sample_uchar));
								}
								else
									output.put(sample_char);

								break;
							}
							case 16:
							{
								short sample_short = (short)(32767 * sample.next_sample());

								if (unsigned_samples)
								{
									unsigned short sample_ushort = (unsigned short)(int(sample_short) + 32768); // bias sample
									if (msb_output)
									{
										output.put((sample_ushort >> 8) & 0xFF);
										output.put(sample_ushort & 0xFF);
									}
									else if (lsb_output)
									{
										output.put(sample_ushort & 0xFF);
										output.put((sample_ushort >> 8) & 0xFF);
									}
									else
										output.write((char *)&sample_ushort, 2);
								}
								else if (ulaw)
									output.put(char(ulaw_encode_sample(sample_short)));
								else if (alaw)
									output.put(char(alaw_encode_sample(sample_short)));
								else
								{
									unsigned short *pu16 = (unsigned short *)&sample_short;

									if (msb_output)
									{
										output.put(((*pu16) >> 8) & 0xFF);
										output.put((*pu16) & 0xFF);
									}
									else if (lsb_output)
									{
										output.put((*pu16) & 0xFF);
										output.put(((*pu16) >> 8) & 0xFF);
									}
									else
										output.write((char *)&sample_short, 2);
								}

								break;
							}
							case 32:
							{
								float sample_float = (float)sample.next_sample();

								output.write((char *)&sample_float, 4);

								break;
							}
							case 64:
								output.write((char *)&sample.next_sample(), 8);
								break;
						}
					break;
	#ifdef DIRECTX
				case direct_output::directx:
					emit_sample_to_directx_buffer(sample);
					break;
	#endif
	#ifdef SDL
				case direct_output::sdl:
					emit_sample_to_sdl_buffer(sample);
					break;
	#endif
			}

			profile.push_back("check for new playback_position");

			channels.front()->get_playback_position(playback_position);

			bool new_playback_position = playback_position.is_changed(last_rendered_playback_position, last_rendered_playback_position_fields);

			profile.push_back("check for new playback_time");

			playback_time.update(current_absolute_tick_number, ticks_per_second);

			bool new_playback_time = playback_time.is_changed(last_rendered_playback_time);

			if (new_playback_position || new_playback_time)
			{
				if (!trace_mod)
				{
					last_rendered_playback_position_fields.clear();

					profile.push_back("render status line");

					// Wall time
					wcerr << L'[' << setfill(L'0')
						<< setw(2) << playback_time.Hour << setw(0) << L':'
						<< setw(2) << playback_time.Minute << setw(0) << L':'
						<< setw(2) << playback_time.Second << setw(0)
						<< L"] ";

					// Position per the channel
					for (wstring::size_type i=0; playback_position.FormatString[i] != '\0'; i++)
					{
						if (playback_position.FormatString[i] != '{')
							wcerr << playback_position.FormatString[i];
						else
						{
							auto field_name_start = ++i;

							while (playback_position.FormatString[i] != L'}')
								i++;

							auto field_name_length = i - field_name_start;

							wstring field_name(&playback_position.FormatString[field_name_start], field_name_length);

							wstring::size_type width_start = field_name.find(L':');

							int width = 0;

							if (width_start != wstring::npos)
							{
								width = wcstol(&field_name[width_start + 1], NULL, 10);
								field_name.erase(width_start);
							}

							int value = playback_position.get_field(field_name);

							if (width)
								wcerr << setw(width);
							wcerr << value;
							if (width)
								wcerr << setw(0);

							last_rendered_playback_position_fields.set_field(field_name, 1);
						}
					}

#define BACKSPACES L"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"

					wcerr << L" -- dynamic channels: " << ancillary_channels.size()
						<< L"       " << BACKSPACES;
				}

				last_rendered_playback_time = playback_time;
				last_rendered_playback_position = playback_position;
			}

			profile.push_back("count the tick");

			current_absolute_tick_number++;

			if (max_ticks > 0)
			{
				max_ticks--;
				if (max_ticks <= 0)
					break;
			}

			if (shutting_down)
				break;
		}

		shutting_down = true;

		switch (direct_output_type)
		{
			case direct_output::none:
				wav_file_header.finalize();
				output.close();
				break;
	#ifdef DIRECTX
			case direct_output::directx:
				shutdown_directx();
				break;
	#endif
	#ifdef SDL
			case direct_output::sdl:
				shutdown_sdl();
				break;
	#endif
		}

		wcerr << endl;
	}
	catch (const wchar_t *message)
	{
		wcerr << endl;
		wcerr << L"CRASH: " << message << endl;
	}

	shutdown_complete = true;

	return 0;
}

#ifndef WIN32
int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "en_US.UTF-8");

	wcout.imbue(locale("en_US.UTF-8"));

	vector<wstring> args;
	vector<const wchar_t *> wargv(argc + 1);

	args.reserve(argc);

	for (int i=0; i < argc; i++)
		args.push_back(utf8_to_unicode(argv[i]));

	for (int i=0; i < argc; i++)
		wargv[i] = args[i].c_str();

	return wmain(argc, &wargv[0]);
}
#endif
