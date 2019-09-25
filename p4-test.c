#ifdef CS333_P4

/* ============================================================================= *
 * Hey CS333 Students!                                                           *
 * -------------------                                                           *
 *                                                                               *
 * Do not modify anything in this file without the explicit permission of the    *
 * course staff.                                                                 *
 * ============================================================================= */




/* =============================================================================
   What does this program do?
   --------------------------

   This program will create 61 child processes. Each child process will do some
   amount of meaningless work before it eventually stops.

   *NOTE*
   You may need to adjust your budget and promotion timers in order to
   effectively test that those features are working.



   Instructions for use:
   ---------------------
   1) Execute this program at the console.
   2) Once you are prompted, test your program using the console commands.
   3) Verify that the results are what you expect.

   Do not change anything about this program without first asking course staff.
 ============================================================================== */
#include "types.h"
#include "user.h"
#include "param.h"
#include "pdx.h"

// We've given these somewhat silly names so they don't conflict with your own
// variable names.

// Number of processes to create at once.
#define P4T_TO_CREATE 5
// The maximum number of processes to create.
#define P4T_MAX 60
// How many seconds any process should live for.
#define P4T_SECONDS 60
// How long should we sleep for after creating a process?
#define P4T_PARENT_SLEEP_SECONDS 1
// How long should a child designated to sleep actually sleep?
#define P4T_CHILD_SLEEP_SECONDS 3 * P4T_SECONDS * TPS
// We use a counter to control how much pointless work we try to accomplish.
#define COUNTER_START -10000000
#define COUNTER_END    10000000

// Create a process and assign a task.
//
// Processes will live for P4T_SECONDS.
//
// Most processes will perform meaningless work that should burn up their budget
// and move the process through the different priority queues.
//
// Every seventh process will sleep for P4T_CHILD_SLEEP_SECONDS (whatever that
// might be). This can be used to demonstrate that sleeping processes are not
// being scheduled in the system.
int
createProcess(void)
{
  // What time is it right now?
  int start = uptime();
  // When should this process stop running?
  int end = start + (P4T_SECONDS * TPS);
  // A counter to track the pointless CPU cycles we're using
  int counter = COUNTER_START;
  // Store the return value of fork() so we know if this is the child or the
  // parent process.
  int rc = fork();

  if (rc == 0) {
    // we're in the child process, now create an infinite loop
    while (1) {
      // This process will only run for 30 seconds and then exit
      if (uptime() > end) {
        exit();
      }

      int pid = 0;
      pid = getpid();

      // Every seventh process will take a nap and then exit.
      // The remaining processes will do some work and attempt to fall down
      // the priority queues.
      if (pid % 7 == 0) {
        // I mean it, sleep for a really long time
        sleep(P4T_CHILD_SLEEP_SECONDS);
        exit();
      } else {
        // These processes only perform a finite amount of work because
        // they need to check (look at the top of the while loop) if it is
        // time to exit.
        while (counter < COUNTER_END) {
          ++counter;
        }
      }

      counter = COUNTER_START;
    }
  } else if (rc < 0) {
    // You shouldn't see this. If you do, it's because you have a number of
    // processes running in the background. Like, you've run `p4-test&` followed
    // by `p4-test&`
    printf(2, "Fork failed! Done creating\n");
  }

  return rc;
}

int
main(int argc, char* argv[])
{
  // Track the count of processes that we've created
  int pc = 0;
  int i;

  // Spin up several processes every second
  // This spreads the processes out so that it should be possible to see
  // multiple queues in use at the same time.
  //
  // If, for some reason, you are not seeing that behavior, adjust the budget
  // or promotion timer accordingly.
  while (pc < P4T_MAX) {
    for (i = 0; i < P4T_TO_CREATE; ++i) {
      if (createProcess() == -1) {
        break;
      }
      ++pc;
    }

    printf(1, "Created %d processes. Sleeping for %d seconds.\n",
           pc % P4T_TO_CREATE ? pc % P4T_TO_CREATE : P4T_TO_CREATE,
           P4T_PARENT_SLEEP_SECONDS);
    sleep(P4T_PARENT_SLEEP_SECONDS * TPS);
  }

  printf(1, "Created %d processes.\n", pc);

  // Loop for approximately one minute while prompting for action.
  for (i = 0; i < 6; ++i) {
    // Prompt for action!
    printf(1, "Now verify that your system is working by pressing C-p and then C-r.\n");
    // Now sleep for 10 seconds so the user can actually do something
    sleep(10 * TPS);
  }

  // Just make sure that all child processes have exited so we don't leave
  // orphaned processes hanging around. If there are any child processes,
  // print a helpful message and then sleep for 1 second.
  while (wait() != -1) {
    printf(1, "Waiting on all child processes to exit...\n");
    sleep(TPS);
  }

  printf(1, "Done!\n");

  exit();
}
#endif
