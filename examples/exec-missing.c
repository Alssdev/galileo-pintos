/* Tries to execute a nonexistent process.
   The exec system call must return -1. */

#include <syscall.h>
#include <stdio.h>

int
main (void) 
{
  printf ("exec(\"no-such-file\"): %d\n", exec ("no-such-file"));
}
