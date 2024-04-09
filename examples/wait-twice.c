/* Wait for a subprocess to finish, twice.
   The first call must wait in the usual way and return the exit code.
   The second wait call must return -1 immediately. */

#include <syscall.h>
#include <stdio.h>
int
main (void) 
{
  pid_t child = exec ("child-simple");
  printf ("wait(exec()) = %d\n", wait (child));
  printf ("wait(exec()) = %d\n", wait (child));
}
