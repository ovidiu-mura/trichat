// Students: Alex Davidoff, Kamakshi Nayak, Ovidiu Mura
// Date: 07/19/19
// 
// Program Shell 2
// Control-D: display exit mmessage
// Exit built-in
// builtin funtions: _setuid, _getuid todo: setgid, getgid
// Simple Pipe

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <error.h>
#include <sys/resource.h>
#include <sys/time.h>


int count(char **d); // count the used indexes
void print_usage(int, int, struct rusage); // print pid usage stats

char *tokens[1024]; // store tokens from command line
char *dirs[1024]; // store all the directories parsed from PATH environment variable
char **args; // command line arguments
pid_t pid; // current child id
int status; // current child status
struct rusage usage; // stats about the current child
int isP = 0; // checks if the command line has a pipe

int fds[2]; // two descriptor for simple pipe

// it represents a cmd
struct cmd {
  char *args[20];
  char *name;
};


// it represents a pipe
struct pipe {
  struct cmd* left;
  struct cmd* right;
};


struct pipe p;

// It parses the input from the command line and split in tokens separated by spaces.
// It returns a two dimensional array of chars conaining the tokens.
// cmd - the input command line to be split in tokens
// Returns an array of chars array containing the parsed tokens
char ** parse_cmd(char *cmd)
{ 
  char* token = strtok(cmd," ");
  int i=0;
  while(token != NULL)
  {
    tokens[i] = malloc(strlen(token)*sizeof(char));
    strcpy(tokens[i],token);
    token = strtok(NULL, " ");
    i++;
  }
  tokens[i-1][strlen(tokens[i-1])-1] = '\0';
  tokens[i] = (char*)NULL;
  return tokens;
}


// It parses the PATH string and splits the string by ':' and stores
// each directory in an array of char arrays.
// path - the PATH string containg the direcories of envirnment variables
// Returns an array of char arrays, each token stored at an index
char ** get_path_dirs(char *path)
{
  char *token = strtok(path, ":");
  int i=0;
  while(token != NULL)
  {
    dirs[i] = malloc(strlen(token)*sizeof(char));
    strcpy(dirs[i],token);
    token = strtok(NULL, ":");
    i++;
  }
  dirs[i-1][strlen(dirs[i-1])-1] = '\0';
  dirs[i] = (char*)NULL;
  return dirs;
}

// It frees the dynamically allocated memory of 'dirs' and 'tokens' arrays.
void free_space()
{
  int count_dirs = count(dirs);
  int count_tokens = count(tokens);
  for(int i=0; i<count_dirs; i++)
  {
    free(dirs[i]);
  }
  for(int j=0; j<count_tokens; j++)
  {
    free(tokens[j]);
  }
}


// It implements a rapper of the fork() linux function
static int
Fork()
{
  pid_t pid;

  if ((pid = fork()) < 0)
  {
    write(STDERR_FILENO, "error fork", 10);
    write(STDOUT_FILENO, strerror(errno), 10);
    exit(EXIT_FAILURE);
  }
  return(pid);
}

// It counts the number of indexes allocated in the
// two dimensional chars array.
// dirs - the array of chars array do be parsed
// Returns the number of indexes used
int count(char ** dirs)
{
  int i=0;
  while(dirs[i]!=NULL)
  {
    i++;
  }
  return i;
}


// It searches in every directory from the path to see if the command is
// found and returns the directory where the command is found. If the command 
// is not found in any directory, NULL is returned. If the
// directory is included in the first token, it will be returned.
char * find_cmd_path()
{
  if(strchr(tokens[0], '/')==NULL)
  {
    int i=0;
    int max = count(dirs);
    char temp[1024];
    char tkn[100];
    while(i<max)
    {
      strcpy(temp, dirs[i]);
      strcpy(tkn, tokens[0]);
      if(access(strcat(strcat(temp,"/"),tkn),F_OK)==0)
        return dirs[i];
      i++;
    }
    return (char*)NULL;
  } else
    return tokens[0];
}

// It searches the existing path on every environment dir
// If there if the 'cmd' if found in any of the dir,
// the path is returned
// cmd - the name of the command to be serached in the environment dirs
// Returns the found path or NULL otherwise.
char * find_cpath(char *cmd)
{
  char temp[100];
  int m = count(dirs);
  int i = 0;
  while(i<m)
  {
    strcpy(temp, dirs[i]);
    strcat(temp, "/");
    strcat(temp, cmd);
    if(access(temp, F_OK)==0)
      return dirs[i];
    i++;
  }
  return (char*)NULL;
}

// built-in _exit function
// i - exit code
void _exit(int i)
{
  exit(i);
}


// built-in _getuid function, wrapper of the getuid linux function
int _getuid()
{
  printf("%d\n", getuid());
  return 0;
}

// built-in _setuid function, wrapper of the setuid linux function
// i - the user id to be set
// returns 0 on success, -1 otherwise
int _setuid(int i)
{
  if(setuid(i) < 0)
    return -1;
  return 0;
}

// built-in funtion, runs setgid of linux function
// i - the user id to be set
// 0 on success, -1 otherwise
int _setgid(int i)
{
  if(setgid(i) < 0)
    return -1;
  return 0;
}

// built-in function, runs getgid of linux function
// it prints the current process group id
int _getgid()
{
  printf("%d\n", getgid());
  return 0;
}

// It runs the built-in functions
// Returns -1 on error, command result on success
// cmd - the name of the built-in command to be run
int _run(char *cmd)
{ 
  if(strncmp(cmd, "exit",4)==0)
  {
    _exit(EXIT_SUCCESS);
  } else if(strncmp(cmd, "_setuid", 7) == 0) {
    if(count(args) == 2)
    {
      printf("_setuid: %d\n", atoi(args[1]));
      return _setuid(atoi(args[1]));
    } else
      printf("_setuid arguments: %d\n", count(args));
  } else if(strncmp(cmd, "_getuid", 7) == 0){
    if(count(args) == 1){
      return _getuid();
    }
  } else if(strncmp(cmd, "_setgid", 7) == 0){
    if(count(args)==2)
    {
      printf("_setgid: %d\n", atoi(args[1]));
      return _setgid(atoi(args[1]));
    }
  } else if(strncmp(cmd, "_getgid", 7) == 0) {
    if(count(args) == 1) {
      return _getgid();
    }
  } else {
    pid = Fork();
    if(pid == 0) {
      execv(args[0], args);
      fprintf (stderr, "\n%s: exec failure\n", strerror (errno));
      _exit(EXIT_FAILURE);
    }
  }
  return -1;
}


// It checks in the as if there is a pipe
// Returns 1 if there is a pipe, 0 otherwise
int isPipe(char **as)
{
  int cnt = count(as);
  for(int i=0; i<cnt; i++)
  {
    if(strstr(as[i], "|") != NULL)
      return 1;
  }
  return 0;
}

// It parses the list of the command line arguments and 
// if there is a pipe, it builds the pipe commands structure with the parameters
// as - list of the command line arguments
void build_pipe(char **as)
{
  int cnt = 0;
  int fr = 0;
  int lidx=0;
  int ridx=0;
  if(isPipe(args)){
    isP = 1;
    p.left = malloc(sizeof(struct cmd));
    p.left->name = malloc(sizeof(char)*10);
    p.right = malloc(sizeof(struct cmd));
    p.right->name = malloc(sizeof(char)*10);
    for (int i=0; i< count(args); i++)
    {
      if(strstr(args[i],"|") != NULL) {
        cnt += 1;
	fr += 1;
      } else {
        if(cnt == 0) {
          if(i == 0)
            strncpy(p.left->name, args[i], strlen(args[i]));
          p.left->args[i] = malloc(sizeof(args[i]));
          strncpy(p.left->args[i], args[i], strlen(args[i]));
	  lidx = i+1;
	  p.left->args[lidx] = NULL;
        } else if(cnt == 1) {
	  ridx += 1;
          if(fr == 1){
            strncpy(p.right->name, args[i], strlen(args[i]));
	    fr += 1;
	  }
          p.right->args[i-(lidx+1)] = malloc(sizeof(args[i]));
          strncpy(p.right->args[i-(lidx+1)], args[i], strlen(args[i]));
	  p.right->args[ridx] = NULL;
        }
      }
    }
  }
}

// It executes a one pipe, then prints the result in the stdout
// as - the list of all command line arguments
// pth - the absolute path of the first command
// Returns 0 on success, exits with -1 otherwise
int exec_pipe(char **as, char *pth)
{   
    char temp[100];
    if(pipe(fds) == -1)
    {
      perror("pipe fds failed");
      _exit(-1);
    }
    pid = Fork();
    if (pid == 0)
    {
      if(dup2(fds[1], 1)<0) {
              perror("dup2 error at read of pipe");
              _exit(-1);
      }
      close(fds[0]);
      close(fds[1]);
      strcpy(temp, pth);
      strcat(temp, "/");
      strcat(temp, p.left->name);
      execv(temp, p.left->args);
      perror("error executing left pipe command");
      _exit(-1);
    } 
    pid = Fork();
    if(pid == 0) {
      char *pa = find_cpath(p.right->name);
      dup2(fds[0], 0);
      close(fds[0]);
      close(fds[1]);
      strcpy(temp, pa);
      strcat(temp, "/");
      strcat(temp, p.right->name);
      execv(temp, p.right->args);
      perror("error executing right pipe command");
      _exit(-1);
    }
    if((pid=wait3(&status, 0, &usage))<0)
    {
      perror("wait3 error right pipe command");
    }
    print_usage(pid, status, usage);
    return 0;
}

// It prints all the stats of the child process
// pi - process id the stats to be printed
// st - status of the child process
// rusage - rusage structure which contains all the stats of the process
void print_usage(int pi, int st, struct rusage rusage)
{
  printf("\n\n\nUsage information for pid %d\n", (int)pid);
  printf("status = %d\n", WEXITSTATUS(status));
  printf("User: %ld.%0ld\n", rusage.ru_utime.tv_sec, rusage.ru_utime.tv_usec);
  printf("Sys:  %ld.%0ld\n", rusage.ru_stime.tv_sec, rusage.ru_stime.tv_usec);
  printf("Max resident set size:  %ld\n", rusage.ru_maxrss);
  printf("Shared memory size:  %ld\n", rusage.ru_ixrss);
  printf("Unshared data size:  %ld\n", rusage.ru_idrss);
  printf("Unshared stack size:  %ld\n", rusage.ru_isrss);
  printf("Soft page faults:  %ld\n", rusage.ru_minflt);
  printf("Hard page faults:  %ld\n", rusage.ru_majflt);
  printf("Swaps:  %ld\n", rusage.ru_nswap);
  printf("Block input ops:  %ld\n", rusage.ru_inblock);
  printf("Block output ops:  %ld\n", rusage.ru_oublock);
  printf("IPC messages sent:  %ld\n", rusage.ru_msgsnd);
  printf("IPC messages received:  %ld\n", rusage.ru_msgrcv);
  printf("Signals received:  %ld\n", rusage.ru_nsignals);
  printf("Voluntary context switches:  %ld\n", rusage.ru_nvcsw);
  printf("Involuntary context switches:  %ld\n", rusage.ru_nivcsw);
  exit (EXIT_SUCCESS);
}

// It controls the execution of the program using a loop
// It reads the command line arguments and calls the properly functions 
// to execute accordingly.
// Returns 0 on success, exit with -1 on failure
int main(void)
{ 
  char*path = getenv("PATH");
  char **a = get_path_dirs(path);
  char *cmd_path;
  char tmp[1024];
  long MAX = sysconf(_SC_LINE_MAX);
  char buf[MAX];
  char *const argv[] = {"exit",(char*)NULL};
  do {
    write(STDOUT_FILENO,"%%",2);
    fflush(STDIN_FILENO);
    int n = 0;
    memset(buf, 0, MAX);
    n = read(STDIN_FILENO, buf,MAX);
    if(n > 0 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't')
    {
      _run(buf);
    }
    if (n == 0) // Control-D: only EOF is placed in stdin
    {
      write(STDOUT_FILENO, "Enter 'exit' to leave the shell\n",33);
      fflush(STDIN_FILENO);
      continue;
    } 
    else if((buf[n-1] > 31) && (buf[n-1] < 127)) // Control-D: EOF is placed in stdin after one or more ASCII characters
    {
      write(STDOUT_FILENO,"\nEnter 'exit' to leave the shell\n",34);
      fflush(STDIN_FILENO);
      continue;
    }
    args = parse_cmd(buf);
    cmd_path = find_cmd_path();
    build_pipe(args);
    if(isP) {
      exec_pipe(args, cmd_path);
      isP = 0;
      continue;
    }
    if(n <0)
      break;
    cmd_path = find_cmd_path();
    if(cmd_path==NULL && args[0][0] == '_'){

      int pid = Fork();
      int ret = 0;
      if(pid == 0){
        ret = _run(args[0]);
      }
      pid = wait3(&status, 0, &usage);
      print_usage(pid, status, usage);
      continue;
    } else if(cmd_path==NULL)
      continue;
    else {
      strcpy(tmp,cmd_path);
      if(strchr(args[0],'/')==NULL)
        strcat(strcat(tmp,"/"),args[0]);
      buf[strlen(buf)-1] = 0; // chomp '\n'
      args[0] = tmp;
      _run(args[0]);
      int pid = wait3(&status, 0, &usage);
      if(pid<0)
      {
        fprintf (stderr, "%s: waitpid error\n",strerror (errno));
        _exit(EXIT_FAILURE);
      }
    }
    print_usage(pid, status, usage);
    //    printf("user time: %ld\n", usage.ru_utime.tv_usec);
  } while(1);
  free_space(); // free the memory allocated
  exit(EXIT_SUCCESS);
}

