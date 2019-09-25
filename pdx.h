/*
 * This file contains types and definitions for Portland State University.
 * The contents are intended to be visible in both user and kernel space.
 */

#ifndef PDX_INCLUDE
#define PDX_INCLUDE

#define TRUE 1
#define FALSE 0
#define RETURN_SUCCESS 0
#define RETURN_FAILURE -1

#define NUL 0
#ifndef NULL
#define NULL NUL
#endif  // NULL

#define TPS 1000   // ticks-per-second
#define SCHED_INTERVAL (TPS/100)  // see trap.c

#define NPROC  64  // maximum number of processes -- normally in param.h

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#endif  // PDX_INCLUDE
