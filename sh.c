// Student: Ovidiu Mura
// Date: 06/27/19
// 
// simple shell
// This program takes whatever is input and executes it as a single command.
// This is problematic. While "ls" works, try "ls -FC" or "ls "
// Based on APUE3e p. 12
// 1. Remove fgets and replace with read.
// 2. Remove printf and replace with write.
// 3. Remove call to error() and replace with write, strerror, and exit.
// 4. If commnand path is not specified, then use the PATH environment variable
//    to find the command.
// 5. Add "exit" command to leave the shell
// 6. Replace execlp with execv.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <error.h>
#include <sys/acct.h>

int count(char **d); // count the used indexes

char *tokens[1024]; // store tokens from command line
char *dirs[1024]; // store all the directories parsed from PATH environment variable
char **args;
pid_t pid;
int status;

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


int _run(char *cmd)
{
  if(strncmp(cmd, "exit",4)==0)
  {
    _exit(EXIT_SUCCESS);
  } else {
    pid = Fork();
    if(pid == 0) {
      execv(args[0], args);
      write(STDERR_FILENO, "exec failure", 12);
      write(STDOUT_FILENO, strerror(errno), 13);
      _exit(EXIT_FAILURE);
    }
    if((pid = waitpid(pid, &status, 0)) < 0)
    {
      write(STDERR_FILENO, "waitpid error", 13);
      write(STDOUT_FILENO, strerror(errno), 13);
      _exit(EXIT_FAILURE);
    }
  }
  return 0;
}


int main(void)
{ 
  struct acct dacct;
  char*path = getenv("PATH");
  char **a = get_path_dirs(path);
  char *cmd;
  char tmp[1024];
  long MAX = sysconf(_SC_LINE_MAX);
  char buf[MAX];
//  pid_t pid;
//  int status;
  char *const argv[] = {"exit",(char*)NULL};
//  char ** args;
  if(acct("acct_data") != 0)
    fprintf(stderr, "%s\n", strerror(errno));
  do {
    write(STDOUT_FILENO,"%%",2);
    fflush(STDIN_FILENO);
    int n = 0;
    memset(buf, 0, MAX);
    n = read(STDIN_FILENO, buf,MAX);
//    _run(buf);
    if(n > 0 && buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't')
    {
      _run(buf);
    }
    if (n == 0) // Control-D: only EOF is placed in stdin
    {
      write(STDOUT_FILENO, "\n",2);
      fflush(STDIN_FILENO);
      return 0;
    } 
    else if((buf[n-1] > 31) && (buf[n-1] < 127)) // Control-D: EOF is placed in stdin after one or more ASCII characters
    {
      write(STDOUT_FILENO,"\n",2);
      fflush(STDIN_FILENO);
      return 0;
    }
    args = parse_cmd(buf);
    if(n <0)
      break;
    cmd = find_cmd_path();
    if(cmd==NULL)
      continue;
    strcpy(tmp,cmd);
    if(strchr(args[0],'/')==NULL)
      strcat(strcat(tmp,"/"),args[0]);
    buf[strlen(buf)-1] = 0; // chomp '\n'
    args[0] = tmp;
    _run(args[0]);
/*    pid = Fork();
    if (pid == 0) {  // child
      args[0] = tmp;
      execv(args[0], args);
      write(STDERR_FILENO, "exec failure", 12);
      write(STDOUT_FILENO, strerror(errno), 13);
      exit(EXIT_FAILURE);
    }
    // parent
    if ((pid = waitpid(pid, &status, 0)) < 0)
    {
      write(STDERR_FILENO, "waitpid error", 13);
      write(STDOUT_FILENO, strerror(errno), 13);
      exit(EXIT_FAILURE);
    }*/
  } while(1);
  free_space(); // free the memory allocated
  exit(EXIT_SUCCESS);
}

