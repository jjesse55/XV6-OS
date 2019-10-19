//User program for the ps command that displays info for all active
//processes within the system.

//Joseph Jesse

#ifdef CS333_P2

#include "types.h"
#include "user.h"
#include "uproc.h"

#define MAX 64

static int
call_getprocs(uint max, struct uproc ** table)
{
  (*table) = (struct uproc *) malloc(sizeof(struct uproc)*max);
  if(!(*table))
  {
    printf(2, "Error: malloc() call failed. %s at line %d\n", __FUNCTION__, __LINE__);
    return -1;
  }
  return getprocs(max, (*table));
}


static void
output_ticks(int ticks)
{
      int num_ticks_per_second = 1000;
      //Calculate the time ellapsed in ticks (sec and millisec).
      int time_elapsed = ticks;
      int tick_seconds = time_elapsed/num_ticks_per_second;
      int tick_milliseconds = time_elapsed % num_ticks_per_second;

      //First output the seconds that has passed.
      printf(1, "%d.", tick_seconds);

      //Add additional zeros to the time (if necessary)
      if(tick_milliseconds < 10) printf(1, "00%d\t", tick_milliseconds);
      else if(tick_milliseconds < 100) printf(1, "0%d\t", tick_milliseconds);
      else printf(1, "%d\t", tick_milliseconds);
}

static void
display_procs(int num_copied, struct uproc * table)
{
  if(num_copied < 0)
    printf(2, "__ERROR__\n");
  else {
    printf(1, "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\n");
    for(int i = 0; i < num_copied; ++i)
    {
      printf(1, "%d\t%s\t", table[i].pid, table[i].name);
      if(strlen(table[i].name) < 9)
        printf(1, "\t");
      printf(1, "%d\t%d\t", table[i].uid, table[i].gid);
      printf(1, "%d\t", table[i].ppid);
      
      output_ticks(table[i].elapsed_ticks);
      output_ticks(table[i].CPU_total_ticks);

      //printf(1, "%d\t", table[i].CPU_total_ticks);
      printf(1, "%s\t%d\t\n", table[i].state, table[i].size);
    }
  }
}


int
main(void)
{
  int num_copied;
  uint max = MAX;
  struct uproc * table;

  num_copied = call_getprocs(max, &table);
  display_procs(num_copied, table);
  if(num_copied != -1)
    free(table);

  exit();
}
#endif  //CS333_P2
