#include <signal.h>

void unix_break_handler(int signal)
{
  cerr << "Caught Break...shutting down." << endl;
  start_shutdown();
}

void register_break_handler()
{
  signal(SIGINT, unix_break_handler);
}
