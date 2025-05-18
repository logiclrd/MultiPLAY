// This is old, bitrotted code that isn't part of the main codebase.

struct channel_NIHONGOBANGOO;

vector<channel_NIHONGOBANGOO *> new_thread_instance_queue;

#ifdef WIN32
DWORD CALLBACK nhb_thread_start_trampoline(LPVOID lpParam);
#endif

struct channel_NIHONGOBANGOO : channel
{
	vector<int> remaining;
	static map<string,sample *> voice_samples;

	void break_down_number(int l)
	{
		if (l > 99999999)
			throw "Number is too large";
		if (l < 0)
			throw "Cannot handle negative numbers";

		if (l >= 10000)
		{
			break_down_number(l / 10000);
			if (remaining.back() == 1)
				remaining.erase(remaining.end() - 1);
			remaining.push_back(10000);
			l %= 10000;
		}

		if (l >= 1000)
		{
			int digit = l / 1000;

			if (digit > 1)
				remaining.push_back(digit);
			remaining.push_back(1000);

			l %= 1000;
		}

		if (l >= 100)
		{
			int digit = l / 100;

			if (digit > 1)
				remaining.push_back(digit);
			remaining.push_back(100);

			l %= 100;
		}

		if (l >= 10)
		{
			int digit = l / 10;

			if (digit > 1)
				remaining.push_back(digit);
			remaining.push_back(10);

			l %= 10;
		}

		if (l > 0)
			remaining.push_back(l);
	}

	string get_filename(int items)
	{
		stringstream ss;

		if (static_cast<unsigned int>(items) > remaining.size())
			return "";
		
		for (int i=0; i<items; i++)
		{
			if (i > 0)
				ss << '-';
			ss << remaining[i];
		}

		return ss.str();
	}

	int input_thread()
	{
		while (true)
		{
			int number;

			cout << "Enter a number to be synthesized in Japanese (-1 to exit): ";
			cin >> number;

			if (number < 0)
			{
				finished = true;
				break;
			}

			break_down_number(number);
		}

		return 0;
	}

	bool input_thread_running;

	virtual ChannelPlaybackState::Type advance_pattern(one_sample &sample, Profile &profile)
	{
		if (!input_thread_running)
		{
#ifdef WIN32
			DWORD thread_id;
			new_thread_instance_queue.push_back(this);
			CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)nhb_thread_start_trampoline, NULL, 0, &thread_id);
#else
			cerr << "UNIX input thread startup not implemented" << endl;
			finished = true;
			return ChannelPlaybackState::Finished;
#endif
			input_thread_running = true;
		}

		if (ticks_left)
			return ChannelPlaybackState::Ongoig;

		if (remaining.size() == 0)
		{
			sample.clear();
			return ChannelPlaybackState::Finished;
		}

		string filename;
		size_t length;

		for (length = 1; length <= remaining.size(); length++)
		{
			string try_filename = get_filename(length);
			if (voice_samples.find(try_filename) == voice_samples.end())
				break;
			filename.swap(try_filename);
		}

		length--;
		remaining.erase(remaining.begin(), remaining.begin() + length);

		current_sample = voice_samples[filename];
		current_sample->begin_new_note(NULL, this, &current_sample_context);

		offset_major = 0;
		offset = 0;
		current_waveform = Waveform::Sample;

		delta_offset_per_tick = ticks_per_second / current_sample->samples_per_second;
		
		ticks_left = current_sample_context->num_samples;
		dropoff_start = ticks_left;
		cutoff_ticks = ticks_left;

		return ChannelPlaybackState::Ongoing;
	}

	bool try_data_directory(string directory)
	{
#ifdef WIN32
		WIN32_FIND_DATA find_data;

		char current_directory[MAX_PATH * 2]; // HACK: Will fail if full path exceeds 510 bytes (but this is supposed to be impossible)
		GetCurrentDirectory(MAX_PATH * 2, current_directory);

		SetCurrentDirectory(directory.c_str());

		try
		{
			HANDLE hFindFile = FindFirstFile("*.nhb", &find_data);
			if ((!hFindFile) || (hFindFile == INVALID_HANDLE_VALUE))
			{
				SetCurrentDirectory(current_directory);
				return false;
			}

			try
			{
				do
				{
					string filename = find_data.cFileName;

					string extension = filename.substr(filename.size() - 4, 4);
					if (tolower(extension) == ".nhb")
					{
						ifstream file(filename.c_str(), ios::binary);
						sample *voice_sample = sample_from_file(&file, SampleFileType::RAW);
						voice_sample->samples_per_second = 44100;
						voice_samples[filename.substr(0, filename.size() - 4)] = voice_sample;
					}
				} while (FindNextFile(hFindFile, &find_data));
				hFindFile = NULL; // apparently, the handle is invalid after FindNextFile returns false
			}
			catch (...)
			{
				if ((hFindFile != NULL) && (hFindFile != INVALID_HANDLE_VALUE))
					CloseHandle(hFindFile);
				throw;
			}
			if ((hFindFile != NULL) && (hFindFile != INVALID_HANDLE_VALUE))
				CloseHandle(hFindFile);
		}
		catch (...)
		{
			SetCurrentDirectory(current_directory);
			throw;
		}
		SetCurrentDirectory(current_directory);
#else
		throw "Unix NHB sample set loader not yet implemented";
#endif

		return (voice_samples.size() > 0);
	}

	static bool initialized;

	channel_NIHONGOBANGOO()
		: channel(false)
	{
		if (!initialized)
		{
			if ((!try_data_directory(".\\"))
			 && (!try_data_directory("..\\"))
			 && (!try_data_directory("Japanese Number Synth Files\\"))
			 && (!try_data_directory("..\\Japanese Number Synth Files\\"))
			 && (!try_data_directory("NHB\\"))
			 && (!try_data_directory("..\\NHB\\")))
			{
				cerr << "Unable to locate Japanese number synthesizer sample files!" << endl
						 << "These files have the extension NHB. Please place them into" << endl
						 << "a subdirectory called \"Japanese Number Synth Files\" or" << endl
						 << "\"NHB\" and then try again." << endl;
				throw "Initialization failed";
			}

			initialized = true;
		}

		input_thread_running = false;
	}
};

map<string,sample *> channel_NIHONGOBANGOO::voice_samples;
bool channel_NIHONGOBANGOO::initialized = false;

#ifdef WIN32
DWORD CALLBACK nhb_thread_start_trampoline(LPVOID lpParam)
{
	if (new_thread_instance_queue.size() == 0)
		return -1;

	// WARNING: Possible race condition (not currently a risk; can only
	// occur when two instances of channel_NIHONGOBANGOO start up at
	// the same time, and only one instance is currently allowed).
	channel_NIHONGOBANGOO *instance = new_thread_instance_queue[0];
	new_thread_instance_queue.erase(new_thread_instance_queue.begin());

	return instance->input_thread();
}
#endif