#include "schedos-app.h"
#include "x86sync.h"

/***************************
define this alternative sync mechanism for Ex. 8
comment out for Ex. 6
***************************/
#define alt_sync_mech


/*****************************************************************************
 * schedos-1
 *
 *   This tiny application prints red "1"s to the console.
 *   It yields the CPU to the kernel after each "1" using the sys_yield()
 *   system call.  This lets the kernel (schedos-kern.c) pick another
 *   application to run, if it wants.
 *
 *   The other schedos-* processes simply #include this file after defining
 *   PRINTCHAR appropriately.
 *
 *****************************************************************************/

#ifndef PRINTCHAR
#define PRINTCHAR	(uint16_t)('1' | 0x0C00)
#endif

void
start(void)
{
	int i;
	//sys_set_priority(PRINTCHAR);
	for (i = 0; i < RUNCOUNT; i++) {
		// Write characters to the console, yielding after each one.
	  
#ifdef alt_sync_mech
	  sys_print(PRINTCHAR);
#else
	  
	  while(atomic_swap(&lock, 1) != 0)
	    continue;
	  
	  *cursorpos++ = PRINTCHAR;

	  atomic_swap(&lock, 0);
#endif
		
	  sys_yield();
	}

	sys_exit(0);

	// Yield forever.
	while (1)
		sys_yield();
}
