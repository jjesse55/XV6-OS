//Joseph Jesse
//Test program to test the setpriority system call.

#include "types.h"
#include "user.h"

int
main(int argc, char * argv[])
{
  if(argc != 3) {
    printf(2, "Wrong number of command line args... exiting\n");
    exit();
  }

  int pid = atoi(argv[1]);
  int prio = atoi(argv[2]);

  if(setpriority(pid, prio) < 0) {
    printf(2, "Error when attempting to set priority... exiting\n");
    exit();
  }
  
  exit();
}
