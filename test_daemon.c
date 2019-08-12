#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>



// https://www.tutorialspoint.com/what-is-interprocess-communication
// https://www.tutorialspoint.com/inter_process_communication/inter_process_communication_signals
// http://man7.org/linux/man-pages/man2/sigaction.2.html
// https://www.tutorialspoint.com/what-is-interprocess-communication
// http://man7.org/linux/man-pages/man2/mmap.2.html
// https://stackoverflow.com/questions/5656530/how-to-use-shared-memory-with-linux-in-c
// https://www.geeksforgeeks.org/ipc-shared-memory/
// https://www.poftut.com/mmap-tutorial-with-examples-in-c-and-cpp-programming-languages/

void sigusr1();

char *ptr;
pid_t *pp;
int *shmp;


void create_daemon()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGUSR1, sigusr1);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
    setlogmask(LOG_UPTO(LOG_DEBUG));
    /* Open the log file */
    openlog ("test_daemon", LOG_CONS|LOG_PID|LOG_NDELAY, LOG_LOCAL1);

    pid = getpid();
    shmp[1] = 33333;
    shmp[2] = pid;
    pause();
    while(1)
      pause();
}


void *create_sm(size_t size)
{
  int prot = PROT_READ|PROT_WRITE;
  int visible = MAP_ANONYMOUS|MAP_SHARED;
  return mmap(NULL, size, prot, visible, -1, 0);
}

void sigusr1()
{
  syslog(LOG_INFO, "exec sigusr1 handler: log info error for test_daemon: %s", ptr);
}

int main()
{
  ptr = create_sm(1024);
  pp = (pid_t*)ptr+100;
  strcpy(ptr, "HELLO SM!");
  int shmid = shmget(0x1234, 8, 0644|IPC_CREAT);
  shmp = shmat(shmid, NULL, 0);
  shmp[1] = 55555;
  pid_t pid;
  pid = fork();
  if(pid==0)
  {
    printf("child[%d]\n", getpid());
    create_daemon();
    pp = (pid_t*)ptr+100;
    memcpy(pp, "44", sizeof(pid_t));
    shmp[1] = 33333;
    exit(2);
//    syslog(LOG_INFO, "child log info error for test_daemon, %s, %d %d", ptr, pid, *pp);
//    signal(SIGUSR1, sigusr1);
  }

//  printf("parent sending SIGUSR1 to child %d\n", pid);
  pid_t p1 = wait(NULL);

  while(shmp[1] != 33333);
  printf("shmp: %d\n", shmp[1]);
  printf("pr child: %d\n", p1);
  printf("parent get pid of daemon %d\n", *pp);
  for(int i=0; i<1024; i++)
  {
    printf("%02x ", pp[i]);
  }
  strcpy(ptr, "new HELLO msg");
  kill(shmp[2], SIGUSR1);
  pid_t pd = shmp[2];
  wait(&pd);
  printf("parent[%d] daemon %d, pause: %d\n", getpid(), pd, shmp[3]);
  printf("parent: %s\n", ptr);
  //sleep(1);
  shmdt(shmp);
  shmctl(shmid, IPC_RMID, 0);
  return 1;
}
