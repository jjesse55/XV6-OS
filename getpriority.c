//Joseph Jesse
//Test program to test the getpriority system call.

#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
  int pid = getpid();

  printf(1, "My priority (for PID %d) is: %d\n", pid, getpriority(pid));

  printf(1, "The priority values for init (PID 1) and sh (PID 2) are %d and %d respectively.\n", getpriority(1), getpriority(2));

  printf(1, "Attempting to get the priority of processes with PIDs of -1 and 20 respectively\n");

  if(getpriority(-1) < 0)
    printf(2, "Error, no active PID found...\n");

  if(getpriority(20) < 0)
    printf(2, "Error, no active PID found...\n");

  sleep(5 * TPS);

  exit();
}
