#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#ifdef CS333_P2
#include "uproc.h"
#endif

#ifdef CS333_P3
#define statecount NELEM(states)
#endif

static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif  //CS333_P3

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif  //CS333_P3
#ifdef CS333_P4
  struct ptrs ready[MAXPRIO + 1];
  uint PromoteAtTime;
#endif  //CS333_P4
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void stateListAdd(struct ptrs*, struct proc*);
static int stateListRemove(struct ptrs*, struct proc* p);
static void assertState(struct proc*, enum procstate, const char *, int);
#endif  //CS333_P3

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

#ifdef CS333_P3
  p = ptable.list[UNUSED].head;
  if (!p) {
    release(&ptable.lock);
    return 0;
  }
  if(stateListRemove(&ptable.list[p->state], p) < 0)
    panic("Process not found when removing from state list");
  assertState(p, UNUSED, __FUNCTION__, __LINE__);
  p->state = EMBRYO;
  stateListAdd(&ptable.list[p->state], p);

#else
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
#endif
  p->pid = nextpid++;

  //Set the amount of time that the process has been both scheduled
  //and running in the cpu to 0.
#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif  //CS333_P2

#ifdef CS333_P4
  p->prio = MAXPRIO;
#endif  //CS333_P4

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
#ifdef CS333_P3
    acquire(&ptable.lock);
    stateListRemove(&ptable.list[p->state], p);
    assertState(p, EMBRYO, __FUNCTION__, __LINE__);
    p->state = UNUSED;
    stateListAdd(&ptable.list[p->state], p);
    release(&ptable.lock);
#else
    p->state = UNUSED;
#endif  //CS333_P3
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //Set the start_ticks field for the proc p structure to the value of
  //the global variable ticks.
  p->start_ticks = ticks;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  //Initialize the lists before making the call to allocproc.
#ifdef CS333_P3
  acquire(&ptable.lock);
  initProcessLists();
  initFreeList();
  release(&ptable.lock);
#endif

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

#ifdef CS333_P2
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
#endif  //CS333_P2

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

#ifdef CS333_P3
  if(stateListRemove(&ptable.list[p->state], p) < 0)
    panic("Process not found when removing from state list");
  assertState(p, EMBRYO, __FUNCTION__, __LINE__);
  p->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[MAXPRIO], p);
#else
  stateListAdd(&ptable.list[p->state], p);
#endif
#else
  p->state = RUNNABLE;
#endif  //CS333_P3

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
#ifdef CS333_P3
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[np->state], np) < 0)
      panic("Process not found when removing from state list");
    assertState(np, EMBRYO, __FUNCTION__, __LINE__);
    np->state = UNUSED;
    stateListAdd(&ptable.list[np->state], np);
    release(&ptable.lock);
#else
    np->state = UNUSED;
#endif
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  //Add the copying process for the UID and GID for project 2.
#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif  //CS333_P2

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

#ifdef CS333_P3
  if(stateListRemove(&ptable.list[np->state], np) < 0)
    panic("Process not found when removing from state list");
  assertState(np, EMBRYO, __FUNCTION__, __LINE__);
  np->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[MAXPRIO], np);
#else
  stateListAdd(&ptable.list[np->state], np);
#endif
#else
  np->state = RUNNABLE;
#endif

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  if(stateListRemove(&ptable.list[curproc->state], curproc) < 0)
    panic("Process not found when removing from state list");
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);

  // Jump into the scheduler, never to return.
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
#ifdef PDX_XV6
  curproc->sz = 0;
#endif // PDX_XV6
  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P3
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
#ifdef CS333_P4
        p->prio = 0;
#endif
        if(stateListRemove(&ptable.list[p->state], p) < 0)
          panic("Process not found when removing from state list");
        assertState(p, ZOMBIE, __FUNCTION__, __LINE__);
        p->state = UNUSED;
        stateListAdd(&ptable.list[p->state], p);
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
#ifdef CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    p = ptable.list[RUNNABLE].head;
    if(p) {

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      if(stateListRemove(&ptable.list[p->state], p) < 0)
        panic("Process not found when removing from state list");
      assertState(p, RUNNABLE, __FUNCTION__, __LINE__);
      p->state = RUNNING;
      stateListAdd(&ptable.list[p->state], p);
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif  //CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif  //CS333_P2
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

#ifdef CS333_P2
  p->cpu_ticks_total += ticks - p->cpu_ticks_in;
#endif  //CS333_P2

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  if(stateListRemove(&ptable.list[curproc->state], curproc) < 0)
    panic("Process not found when removing from state list");
  assertState(curproc, RUNNING, __FUNCTION__, __LINE__);
  curproc->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[curproc->prio], curproc);
#else
  stateListAdd(&ptable.list[curproc->state], curproc);
#endif
  sched();
  release(&ptable.lock);
}
#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;

  if(stateListRemove(&ptable.list[p->state], p) < 0)
    panic("Process not found when removing from state list");
  assertState(p, RUNNING, __FUNCTION__, __LINE__);
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.list[SLEEPING].head; p && p != ptable.list[SLEEPING].tail->next; p = p->next) {
    if(p->chan == chan) {
      if(stateListRemove(&ptable.list[p->state], p) < 0)
        panic("Process not found when removing from state list");
      assertState(p, SLEEPING, __FUNCTION__, __LINE__);
      p->state = RUNNABLE;
#ifdef CS333_P4
      stateListAdd(&ptable.ready[p->prio], p);
#else
      stateListAdd(&ptable.list[p->state], p);
#endif
    }

    if(p == ptable.list[SLEEPING].tail)
      break;
  }
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P3
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING) {
        if(stateListRemove(&ptable.list[p->state], p) < 0)
          panic("Process not found when removing from state list");
        assertState(p, SLEEPING, __FUNCTION__, __LINE__);
        p->state = RUNNABLE;
        stateListAdd(&ptable.list[p->state], p);
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#ifdef CS333_P1
static void
procdumpP1(struct proc * p, char * state)
{
  int num_ticks_per_second = 1000;
  //Calculate the time ellapsed in ticks (sec and millisec).
  int time_elapsed = ticks - p->start_ticks;
  int tick_seconds = time_elapsed/num_ticks_per_second;
  int tick_milliseconds = time_elapsed % num_ticks_per_second;

  //First output the seconds that has passed.
  cprintf("%d.", tick_seconds);

  //Add additional zeros to the time (if necessary)
  if(tick_milliseconds < 10) cprintf("00%d\t", tick_milliseconds);
  else if(tick_milliseconds < 100) cprintf("0%d\t", tick_milliseconds);
  else cprintf("%d\t", tick_milliseconds);
}
#endif  //CS333_P2

#ifdef CS333_P2
static void
procdumpP2(struct proc * p, char * state)
{
  cprintf("%d\t%d\t", p->uid, p->gid);
  if(!p->parent)
    cprintf("%d\t", p->pid);
  else
    cprintf("%d\t", p->parent->pid);

#ifdef CS333_P4
  cprintf("%d\t", p->prio);
#endif

  procdumpP1(p, state);

  //cprintf("%d\t", p->cpu_ticks_total);
  int num_ticks_per_second = 1000;

  //Calculate the time ellapsed in ticks (sec and millisec).
  int time_elapsed = p->cpu_ticks_total;
  int tick_seconds = time_elapsed/num_ticks_per_second;
  int tick_milliseconds = time_elapsed % num_ticks_per_second;

  //First output the seconds that has passed.
  cprintf("%d.", tick_seconds);

  //Add additional zeros to the time (if necessary)
  if(tick_milliseconds < 10) cprintf("00%d\t", tick_milliseconds);
  else if(tick_milliseconds < 100) cprintf("0%d\t", tick_milliseconds);
  else cprintf("%d\t", tick_milliseconds);
}
#endif  //CS333_P2

#ifdef CS333_P3
static void
procdumpP3(struct proc * p, char * state)
{
  procdumpP2(p, state);
}
#endif  //CS333_P3

void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\tPCs\n"
#elif defined(CS333_P3)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\tPCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\tCPU\tState\tSize\tPCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName\t\tElapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);  // not conditionally compiled as must work in all project states

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)  {
      continue;
    }
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#ifdef CS333_P1
    cprintf("%d\t%s\t", p->pid, p->name);
    if(strlen(p->name) < 9)
      cprintf("\t");
#endif  //CS333_P1

#if defined(CS333_P3)
    procdumpP3(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif

#ifdef CS333_P1
    cprintf("%s\t%d\t", state, p->sz);
#endif

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        if(!i)
          cprintf("%p", pc[i]);
        else
          cprintf(" %p", pc[i]);
    }

    cprintf("\n");
  }
#ifdef CS333_P1
  cprintf("$ ");  // simulate shell prompt
#endif // CS333_P1
}


//Helper functions for the getter and setter function calls.
//Helps with getting/setting uid, gid, and ppid values.
#ifdef CS333_P2
uint
get_uid(void)
{
  uint uid;
  acquire(&ptable.lock);
  uid = myproc()->uid;
  release(&ptable.lock);

  return uid;
}

uint
get_gid(void)
{
  uint gid;
  acquire(&ptable.lock);
  gid =  myproc()->gid;
  release(&ptable.lock);

  return gid;
}

uint
get_ppid(void)
{
  uint ppid;
  struct proc * parent;

  acquire(&ptable.lock);
  parent = myproc()->parent;

  //Because in this case we want the pid of the same process
  //to be displayed.
  if(!parent)
    parent = myproc();
  ppid = parent->pid;
  release(&ptable.lock);

  return ppid;
}

int
set_uid(uint uid)
{
  acquire(&ptable.lock);
  myproc()->uid = uid;
  release(&ptable.lock);

  return 0;
}

int
set_gid(uint gid)
{
  acquire(&ptable.lock);
  myproc()->gid = gid;
  release(&ptable.lock);

  return 0;
}


int
get_procs(int max, struct uproc *table)
{
  char *state;
  int count = 0;

  acquire(&ptable.lock);
  for(struct proc *p = ptable.proc; count < max && p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED || p->state == EMBRYO)  {
      continue;
    }
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

    //copy over all the data
    table[count].pid = p->pid;
    table[count].uid = p->uid;
    table[count].gid = p->gid;
    if(!p->parent)
      table[count].ppid = p->pid;
    else
      table[count].ppid = p->parent->pid;
    table[count].elapsed_ticks = ticks - p->start_ticks;
    table[count].CPU_total_ticks= p->cpu_ticks_total;
    strncpy(table[count].state, state, (strlen(state) + 1));
    table[count].size = p->sz;
    strncpy(table[count].name, p->name, (strlen(state) + 1));

    ++count;
  }

  release(&ptable.lock);
  return count;
}
#endif  //CS333_P2


#ifdef CS333_P3
static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we’ve removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found, hit eject.
  if(current == NULL){
    return -1;
  }

  // Process found. Set the appropriate next pointer.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn’t point into the list.
  p->next = NULL;
  return 0;
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}

static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}

static void
assertState(struct proc *p, enum procstate state, const char * func, int line)
{
  if (p->state == state)
    return;
  cprintf("Error: proc state is %s and should be %s.\nCalled from %s line %d\n",
      states[p->state], states[state], func, line);
  panic("Error: Process state incorrect in assertState()");
}

void
proc_free(void)
{
  int count = 0;
  acquire(&ptable.lock);

  struct proc * p = ptable.list[UNUSED].head;

  while(p != NULL) {
    ++count;
    p = p->next;
  }

  release(&ptable.lock);

  cprintf("Free List Size: %d Processes\n$ ", count);
}

void
proc_ready(void)
{
  acquire(&ptable.lock);

  struct proc * p = ptable.list[RUNNABLE].head;

  cprintf("Ready list proccesses:\n");

  while(p && p != ptable.list[RUNNABLE].tail->next) {
    cprintf("%d ", p->pid);

    if(p == ptable.list[RUNNABLE].tail) {
      break;
    }
    else
      cprintf("-> ");

    p = p->next;
  }

  release(&ptable.lock);

  cprintf("\n$ ");
}

void
proc_sleep(void)
{
  acquire(&ptable.lock);

  struct proc * p = ptable.list[SLEEPING].head;

  cprintf("Sleeping list proccesses:\n");

  while(p) {
    cprintf("%d ", p->pid);

    if(p == ptable.list[SLEEPING].tail) {
      break;
    }
    else
      cprintf("-> ");

    p = p->next;
  }

  release(&ptable.lock);

  cprintf("\n$ ");
}

void
proc_zombie(void)
{
  acquire(&ptable.lock);

  struct proc * p = ptable.list[ZOMBIE].head;

  cprintf("Zombie list proccesses:\n");

  while(p) {
    cprintf("(%d, %d)", p->pid, p->parent->pid);

    if(p == ptable.list[ZOMBIE].tail) {
      break;
    }
    else
      cprintf(" -> ");

    p = p->next;
  }

  release(&ptable.lock);

  cprintf("\n$ ");
}
#endif

#ifdef CS333_P4
static void
retrieveFromList(struct proc * head, int pid)
{
  while(head)
  {
    if(head->pid == pid)
      break;
    head = head->next;
  }

  return head;
}

//Helper function for the getpriority system call.
int
get_priority(int pid)
{
  if(pid < 0 || pid > 32767)
    return -1;

  struct proc * p = NULL;

  acquire(&ptable.lock);

  //Check first to see if the process is in one of the ready lists.
  for(int i = MAXPRIO; i >= 0; --i)
  {
    p = retrieveFromList(ptable.ready[i].head, pid);
    if(p)
      break
  }

  if(!p)
  {
    //Check to see if it is in the running list.
    for(int i = EMBRYO; i <= ZOMBIE; ++i)
    {
      //Already checked the ready/runnable lists.
      if(i == RUNNABLE)
        continue;
      p = retrieveFromList(ptable.list[i].head, pid);
      if(p)
        break;
    }
  }

  //After going through all the lists, there was not an active process
  //found with the pid passed in.
  if(!p)
  {
    release(&ptable.lock);
    return -1;
  }
      
  int toReturn = p->prio;
  release(&ptable.lock);
  return toReturn;
}

//Helper function for the setpriority system call.
int
set_priority(int pid, int prio)
{
  //Test a valid pid value passes in against the min and max values.
  //Also, test to make sure the priority value passed in is valid as well.
  if(pid < 0 || pid > 32767 || prio < 0 || prio > MAXPRIO)
    return -1;
  
  struct proc * p = NULL;
  
  acquire(&ptable.lock);

  //Check first to see if the process is in one of the ready lists.
  for(int i = MAXPRIO; i >= 0; --i)
  {
    p = retrieveFromList(ptable.ready[i].head, pid);
    if(p)
    {
      p->prio = prio;
      release(&ptable.lock);
      return 0;
    }
  }

  //Check to see if it is in the running list.
  for(int i = EMBRYO; i <= ZOMBIE; ++i)
  {
    //Already checked the ready/runnable lists.
    if(i == RUNNABLE)
      continue;
    p = retrieveFromList(ptable.list[i].head, pid);
    if(p)
      break;
  }
  
  //After going through all the lists, there was not an active process
  //found with the pid passed in.
  if(!p)
  {
    release(&ptable.lock);
    return -1;
  }
      
  p->prio = prio;
  release(&ptable.lock);
  return 0;
}
#endif
