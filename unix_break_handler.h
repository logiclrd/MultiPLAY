#include <signal.h>

void unix_break_handler()
{
  cerr << "Caught Break...shutting down." << endl;
  start_shutdown();
}

void register_break_handler()
{
  sigset(SIGINT, unix_break_handler);
}