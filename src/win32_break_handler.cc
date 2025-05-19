#include <iostream>

using namespace std;

#include <windows.h>

#include "MultiPLAY.h"

namespace MultiPLAY
{
	BOOL __stdcall win32_break_handler(DWORD ctrl_code)
	{
		cerr << "Caught Break...shutting down." << endl;
		start_shutdown();
		if (ctrl_code == CTRL_CLOSE_EVENT)
			while (!shutdown_complete)
				Sleep(200);
		return TRUE;
	}

	void register_break_handler()
	{
		SetConsoleCtrlHandler(win32_break_handler, TRUE);
	}
}
