// Students: Alex Davidoff, Kamakshi Nayak, Ovidiu Mura
// Date: 07/19/19
// 
// Shell 2
// Control-D
// Exit
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


int count(char **d); // count the used indexes


char *tokens[1024]; // store tokens from command line
char *dirs[1024]; // store all the directories parsed from PATH environment variable
char **args;
pid_t pid;
int status;
struct rusage usage;

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

// built-in _exit function
// i - exit code
void _exit(int i)
{
  exit(i);
}

int _getuid()
{
  printf("%d\n", getuid());
  return 0;
}



int _setuid(int i)
{
  if(setuid(i) < 0)
    return -1;
  return 0;
}

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
  } else
  {
    pid = Fork();
    if(pid == 0) {
      execv(args[0], args);
      write(STDERR_FILENO, "exec failure", 12);
      write(STDOUT_FILENO, strerror(errno), 13);
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


void build_pipe(char **as)
{
  int cnt = 0;
  int fr = 0;
  int lidx=0;
  int ridx=0;
  if(isPipe(args)){
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
              perror("dup2 error at read of pipe\n");
              _exit(0);
      }
      close(fds[0]);
      close(fds[1]);
      strcpy(temp, pth);
      strcat(temp, "/");
      strcat(temp, p.left->name);
      execv(temp, p.left->args);
      perror("bad exec ps");
      _exit(-1);
    } 
    pid = Fork();
    if(pid == 0) {
      dup2(fds[0], 0);
      close(fds[0]);
      close(fds[1]);
      strcpy(temp, pth);
      strcat(temp, "/");
      strcat(temp, p.right->name);
      execv(temp, p.right->args);
      perror("error executing right pipe command");
      _exit(-1);
    }
    wait(NULL);
    return 0;
}



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
   
    exec_pipe(args, cmd_path);
    return 0;

    if(n <0)
      break;
    cmd_path = find_cmd_path();
    if(cmd_path==NULL && args[0][0] == '_'){

      int pid = Fork();
      int ret = 0;
      if(pid == 0){
        ret = _run(args[0]);
      }
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
        write(STDERR_FILENO, "waitpid error", 13);
        write(STDOUT_FILENO, strerror(errno), 13);
        _exit(EXIT_FAILURE);
      }
    }
    printf("user time: %ld\n", usage.ru_utime.tv_usec);
  } while(1);
  free_space(); // free the memory allocated
  exit(EXIT_SUCCESS);
}

