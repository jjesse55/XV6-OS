//User program which calls sys_time to test see how long a program runs.

//Joseph Jesse

#ifdef CS333_P2

#include "types.h"
#include "user.h"


static void
report_time(char * program, int start, int end)
{
  //in milliseconds.
  int num_ticks_per_second = 1000;
  //Calculate the time ellapsed in ticks (sec and millisec).
  int time_elapsed = end - start;
  int tick_seconds = time_elapsed/num_ticks_per_second;
  int tick_milliseconds = time_elapsed % num_ticks_per_second;

  printf(1, "%s ran in ", program);
  //First output the seconds that has passed.
  printf(1, "%d.", tick_seconds);

  //Add additional zeros to the time (if necessary)
  if(tick_milliseconds < 10) printf(1, "00%dseconds\n", tick_milliseconds);
  else if(tick_milliseconds < 100) printf(1, "0%d seconds\n", tick_milliseconds);
  else printf(1, "%d seconds\n", tick_milliseconds);
}


int
main(int argc, char ** argv)
{
  //Just for nice formatting.
  printf(1, "\n");
  int pid = fork();
  if(pid < 0)
  {
    printf(2, "__ERROR__\nFork failed.\n\n");
    exit();
  }

  //increment to the actual program to be timed.
  ++argv;
  //Start the time right before making the call to exec.
  int start = uptime();

  if(pid == 0) {
    exec(argv[(argc - argc)], argv);
    printf(2, "__ERROR__\nExec failed to execute, no program named %s\n\n", argv[0]);
    exit();
  }
  else
    wait();

  //End the time as soon as the process ends.
  int end = uptime();
  report_time((argv[(argc - argc)]), start, end);
  exit();
}
#endif  //CS333_P2
