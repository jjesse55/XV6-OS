#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#ifdef PDX_XV6
#include "pdx-kernel.h"
#endif // PDX_XV6
#ifdef CS333_P2
#include "uproc.h"
#endif

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      return -1;
    }
    sleep(&ticks, (struct spinlock *)0);
  }
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  xticks = ticks;
  return xticks;
}

#ifdef PDX_XV6
// shutdown QEMU
int
sys_halt(void)
{
  do_shutdown();  // never returns
  return 0;
}
#endif // PDX_XV6

#ifdef CS333_P1
//Add the date system call implementation.
int
sys_date(void)
{
  struct rtcdate *d;

  if(argptr(0, (void*)&d, sizeof(struct rtcdate)) < 0)
    return -1;

  cmostime(d);
  return 0;
}
#endif


//Implement the set sys calls for uid and gid as well as the
//getters for uid, gid, and ppid.
#ifdef CS333_P2
int
sys_getuid(void)
{
  return (int) get_uid();
}

int
sys_getgid(void)
{
  return (int) get_gid();
}

int
sys_getppid(void)
{
  return (int) get_ppid();
}

int
sys_setuid(void)
{
  int uid;
  if(argint(0, &uid) < 0)
    return -1;
  //The min and max values allowed for uid values.
  if(uid < 0 || uid > 32767)
    return -1;

  uint uint_uid = (uint) uid;
  return set_uid(uint_uid);
}

int
sys_setgid(void)
{
  int gid;
  if(argint(0, &gid) < 0)
    return -1;
  //The min and max values allowed for gid values.
  if(gid < 0 || gid > 32767)
    return -1;

  uint uint_gid = (uint) gid;
  return set_gid(uint_gid);
}


int
sys_getprocs(void)
{
  int max;
  struct uproc * table;
  if(argint(0, &max) < 0)
    return -1;
  if(argptr(1, (void*)&table, (sizeof(struct uproc)*max)) < 0)
    return -1;

  return get_procs(max, table);
}
#endif  //CS333_P2

#ifdef CS333_P4
int
sys_setpriority(void)
{
  int pid, prio;
  if(argint(0, &pid) < 0 || argint(1, &prio) < 0)
    return -1;

  //Test a valid pid value passes in against the min and max values.
  //Also, test to make sure the priority value passed in is valid as well.
  if(pid < 0 || pid > 32767 || prio < 0 || prio > MAXPRIO)
    return -1;

  return set_priority(pid, prio);
}

int
sys_getpriority(void)
{
  int pid;
  if(argint(0, &pid) < 0)
    return -1;
  
  //Test a valid pid value passes in against the min and max values.
  if(pid < 0 || pid > 32767)
    return -1;

  return get_priority(pid);
}
#endif
