#include <iostream>
#include <csignal>

using namespace std;

#include "break_handler.h"

#include "MultiPLAY.h"

namespace MultiPLAY
{
	namespace
	{
		void unix_break_handler(int)
		{
			cerr << "Caught Break...shutting down." << endl;
			start_shutdown();
		}
	}

	void register_break_handler()
	{
		signal(SIGINT, unix_break_handler);
	}
}
