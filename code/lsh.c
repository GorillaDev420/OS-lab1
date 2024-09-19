/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!sssssssssssssss
 */
#define READ_END 0 
#define WRITE_END 1
#define CHILD_PROCESS 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define MAX 1000

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>
#include <sys/wait.h>

#include "parse.h"

static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
int execute_child_process(Command*);
int execute_program(char** pl);
int execute_child_process_with_pipes(Command* cmd);
void close_all_pipes(int, int[MAX][2]);

//Catching the ctrl+c
void catch_signal(int sig){
  printf("signal caught");
  if(sig == SIGINT){
    kill(0, SIGKILL);
  }
}
int check_builtin(Command *cmd){
  char** built_in_cmd = cmd->pgm->pgmlist;
  if (strcmp(built_in_cmd[0], "cd")==0){

    if(strcmp(built_in_cmd[1], "~")== 0){

      char* username = getlogin();
      //Changed the hard coded path to fit the school computer: For arch do: "/home/"
      char* home_path = "/chalmers/users/";
      int path_len = strlen(home_path) + strlen(username) + 1;
      char* path = (char *) malloc(sizeof(char) * path_len);
      strcpy(path, home_path);
      strcat(path, username);

      chdir(path);
      free(path);
    }
    else {
      chdir(built_in_cmd[1]);
    }

    return 1;
  }
  else if(strcmp(built_in_cmd[0], "exit")== 0){
    // _exit(0) did not work using the school computer
    exit(0);
    return 2;
  }

  return 0;
}

int main(void)
{
  for (;;)
  {
    char *line;
    line = readline("> ");
    if(line == NULL){
      exit(0);
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        // Just prints cmd
        print_cmd(&cmd);
        if((check_builtin(&cmd))==0){
          execute_child_process(&cmd);
        };
      
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Clear memory
    free(line);
  }

  return 0;
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}

int execute_program(char** pl){
  int ret = execvp(pl[0], pl);
  if(ret == 1){

  }
  else {
    printf("command was not executed properly %d", ret);
    return -1;
  }
}

int execute_child_process(Command* cmd){
  
  int pid, ret;
  Pgm* p_command = cmd->pgm;
  while(p_command != NULL){
    // int pipe_parent[2];
    // int pipe_child[2];

    // int pipe_fd[2];
    // pipe(pipe_fd);
    pid = fork(); int pipe_fd[2];
    if (pid == 0) {

      if(!cmd->background){
        printf("This is not a bg proc\n");
        signal(SIGINT, catch_signal);
      }
        ret = execute_program(cmd->pgm->pgmlist);
      

    }  
      
    
    else {
      wait(NULL);
      // code here
      break;
    }
    p_command = p_command->next;
  }
  return 0;
}

int execute_child_process_with_pipes(Command* cmd){
  Pgm* next_command = cmd->pgm->next;
  int nr_pipes = 0;
  while(next_command != NULL){
    nr_pipes++;
    next_command = next_command->next;
  }


  int pipes[nr_pipes][2];
  int i = 0;
  for(i; i<nr_pipes; i++){
    if(pipe(pipes[i]) < 0){
        return -1;
    }
  }

  int n = 0;
  Pgm* current_pgm = cmd->pgm;
  while(current_pgm != NULL){
    int pid = fork();
    //"first item in LinkedList, only read on pipe"
    if(pid == CHILD_PROCESS && n == 0){
      waitpid(0,NULL,0);
      dup2(pipes[n][READ_END],STDIN_FILENO);
      int ret = execute_program(current_pgm->pgmlist);
      close_all_pipes(i,pipes);
      return ret;
    }
    //"Is parent and a child, has both ends of a pipe"
    else if(pid == CHILD_PROCESS && n > 0 && current_pgm->next != NULL){
      int read_pipe_index = n;
      int write_pipe_index = n-1;
      waitpid(0,NULL,0);
      dup2(pipes[read_pipe_index][READ_END],STDIN_FILENO);
      dup2(pipes[write_pipe_index][WRITE_END],STDOUT_FILENO);
      int ret = execute_program(current_pgm->pgmlist);
      close_all_pipes(i,pipes);
    }
    //"last item in linkedlist Only write on pipe"
    else if(pid == CHILD_PROCESS && current_pgm->next == NULL){
      dup2(pipes[n][WRITE_END],STDOUT_FILENO);
      int ret = execute_program(current_pgm->pgmlist);
      close_all_pipes(i,pipes);
      return ret;
    }
    n++;
    current_pgm = current_pgm->next;
  }

}

void close_all_pipes(int i, int pipes[MAX][2]){
  for(int n = 0; n<i; n++){
    close(pipes[n][READ_END]);
    close(pipes[n][WRITE_END]);  
  }
}

