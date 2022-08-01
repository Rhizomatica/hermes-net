// file become_daemon.c
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include "daemon.h"

// returns 0 on success, negative on error
int become_daemon()
{
  int fd;

  /* The first fork will change our pid
   * but the sid and pgid will be the
   * calling process.
   */
  switch(fork())                    // become background process
  {
    case -1: return -1;
    case 0: break;                  // child falls through
    default: _exit(EXIT_SUCCESS);   // parent terminates
  }

  /*
   * Run the process in a new session without a controlling
   * terminal. The process group ID will be the process ID
   * and thus, the process will be the process group leader.
   * After this call the process will be in a new session,
   * and it will be the progress group leader in a new
   * process group.
   */
  if(setsid() == -1)                // become leader of new session
    return -1;

  /*
   * We will fork again, also known as a
   * double fork. This second fork will orphan
   * our process because the parent will exit.
   * When the parent process exits the child
   * process will be adopted by the init process
   * with process ID 1.
   * The result of this second fork is a process
   * with the parent as the init process with an ID
   * of 1. The process will be in it's own session
   * and process group and will have no controlling
   * terminal. Furthermore, the process will not
   * be the process group leader and thus, cannot
   * have the controlling terminal if there was one.
   */
  switch(fork())
  {
    case -1: return -1;
    case 0: break;                  // child breaks out of case
    default: _exit(EXIT_SUCCESS);   // parent process will exit
  }

  umask(0);                       // clear file creation mode mask

  chdir("/");                     // change to root directory

  close(0);
  close(1);
  close(2);

  /* now time to go "dark"!
   * we'll close stdin
   * then we'll point stdout and stderr
   * to /dev/null
   */

  fd = open("/dev/null", O_RDWR);
  if(fd != STDIN_FILENO)
    return -1;
  if(dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
    return -2;
  if(dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
    return -3;

  return 0;
}
