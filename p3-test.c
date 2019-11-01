#ifdef CS333_P3

// A starting point for writing your own p3 test program(s).
// Notes
// 1. The parent never gets to the wait() call, so killing any child will cause that
//    child to move to zombie indefinitely.
// 2. When running as a background process, the parent of the main process
//    will be init. This is how background processes work. Read sh.c for details.
// 3. Useful test: run in background then kill a child. Child will be in zombie. Now
//    kill parent. Should get "zombie!" equal to the number of zombies.
//    ps or ^p should now show init as the parent of all child processes.
//    init will then reap the zombies.
// 4. Can also be used to show that the RUNNABLE list is still round robin.


#include "types.h"
#include "user.h"
#include "param.h"
#include "pdx.h"

int
main(int argc, char *argv[])
{
  int rc, i = 0, childCount = 20;

  if(argc > 1)
    childCount = atoi(argv[1]);

  if(!childCount)
  {
    printf(2, "No children to create, so %s is exiting.\n", argv[0]);
    exit();
  }

  printf(1, "Starting %d child process that will run forever\n", childCount);

  do {
    rc = fork();
    if(rc < 0)
    {
      printf(2, "Fork Failed!\n");
      exit();
    }

    if(rc == 0) {
      while(1)
        ++i;
      exit();
    }
    --childCount;
  }while(childCount);

    printf(1, "All child processes created\n");
    while(1)
      ++i;

    exit();
  }
#endif
