#ifndef PROFILE_H
#define PROFILE_H

#include <time.h>

struct ProfileEntry
{
	const char *point;
	long long timestamp;

#define NS_PER_S 1000000000LL

	ProfileEntry(const char *p)
	{
		struct timespec ts;

		clock_gettime(CLOCK_REALTIME, &ts);

		point = p;
		timestamp = ts.tv_sec * NS_PER_S + ts.tv_nsec;
	}
};

struct Profile
{
#ifdef PROFILE
	vector<ProfileEntry> entries;
#endif

	void push_back(const char *point)
	{
#ifdef PROFILE
		push_back((ProfileEntry)point);
#endif
	}

	void push_back(const ProfileEntry &entry)
	{
#ifdef PROFILE
		entries.push_back(entry);
#endif
	}

	void dump()
	{
#ifdef PROFILE
		ofstream out("profiles.txt", ios::app);

		out << "---------" << endl;

		for (int i=0; i < entries.size(); i++)
		{
			int whole_milliseconds = entries[i].timestamp / (NS_PER_S / 1000);

			int remainder = entries[i].timestamp - whole_milliseconds * (NS_PER_S / 1000);

			out << setw(4) << whole_milliseconds << setw(0) << "." << remainder << " : " << entries[i].point << endl;
		}
#endif
	}

	~Profile()
	{
#ifdef PROFILE
		if (entries.size() >= 2)
		{
			int duration_ns = entries.back().timestamp - entries.front().timestamp;

			if (duration_ns > NS_PER_S / 44100)
				dump();
		}
#endif
	}
};

#endif // PROFILE_H