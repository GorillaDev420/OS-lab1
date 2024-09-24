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
 * All the best!
 */
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
#include <fcntl.h>

#include "parse.h"

#define MAX_PROCESSES 10
#define READ_END 0
#define WRITE_END 1


static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);


void execute_program(char** pl);
int recursive_forking(Pgm* pgm, int** fds, int);
int execute_child_with_pipes(Command* cmd);
int execute_command(Command* cmd);


char* infile_path = NULL;
char* outfile_path = NULL;
int bg_flag = 0;
int chld_pid = -1;



// catching background singal
void sigchld_handler(int sig) {
  //wait();
}

int check_builtin(Command *cmd){
  char** built_in_cmd = cmd->pgm->pgmlist;
  if (strcmp(built_in_cmd[0], "cd")==0){

    if(strcmp(built_in_cmd[1], "~")== 0){

      char* username = getlogin();
      char* home_path = "/home/";
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
    _exit(0);
    return 2;
  }

  return 0;
}

int main(void)
{
  // shell process ignores ctrl+C
  signal(SIGCHLD, sigchld_handler);

  for (;;)
  {
    char *line;
    line = readline("> ");
    if (line == NULL) {
      _exit(0);
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
          execute_command(&cmd);
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

int redirect(char* path, int redirect_fd) {
  if (path == NULL)
    return -1;
  
  int fd;
  switch (redirect_fd)
  {
  case STDIN_FILENO:
    fd = open(path, O_RDONLY);
    if (fd > 0) {
      dup2(fd, STDIN_FILENO);
      close(fd);
    }
    break;
  case STDOUT_FILENO:
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd > 0) {
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    break;
  
  default:
    break;
  }
  return fd;
}

void conditional_wait(int* _start_loc, int bg) {
  if (!bg) {
    wait(_start_loc);
  }
  else {
    signal(SIGCHLD, SIG_IGN);
  }
}

void execute_program(char** pl){
  execvp(pl[0], pl);
}

int recursive_forking(Pgm* pgm, int** fds, int process_nr){
  // base case
  if(pgm->next == NULL){

    // check input file redirection
    redirect(infile_path, STDIN_FILENO);

    dup2(fds[process_nr - 1][WRITE_END], STDOUT_FILENO);
    close(fds[process_nr - 1][WRITE_END]);
    close(fds[process_nr - 1][READ_END]);

    execute_program(pgm->pgmlist);
  }
  else{
    pipe(fds[process_nr]);

    int pid = fork();
    // child
    if(pid == 0){
      recursive_forking(pgm->next, fds, process_nr + 1);
      //wait(NULL);
    }
    // parent
    else{
      wait(NULL);
      // last program to be exec
      if(process_nr == 0){
        // check input file redirection
        redirect(outfile_path, STDOUT_FILENO);

        dup2(fds[process_nr][READ_END],STDIN_FILENO);
        close(fds[process_nr][WRITE_END]);
        close(fds[process_nr][READ_END]);

        execute_program(pgm->pgmlist);

      }
      // programs exec in the middle of the pipe
      else{
        dup2(fds[process_nr-1][WRITE_END], STDOUT_FILENO);
        dup2(fds[process_nr][READ_END], STDIN_FILENO);

        close(fds[process_nr-1][WRITE_END]);
        close(fds[process_nr-1][READ_END]);
        close(fds[process_nr][WRITE_END]);
        close(fds[process_nr][READ_END]);

        execute_program(pgm->pgmlist);
      }
    }
  }
}

int** alloc_fds(Pgm* pgm, int* count) {
  while (pgm) {
    (*count)++;
    pgm = pgm->next;
  }

  int** fds = (int** ) malloc(sizeof(int *) * (*count));
  int i = 0;
  for (i = 0; i < (*count); i++) {
    fds[i] = (int *) malloc(sizeof(int) * 2);
    memset(fds[i], -1, sizeof(int) * 2);
  }
  return fds;
}

void free_fds(int** fds, int count) {
  int i = 0;
  for (i = 0; i < count; i++) {
    free(fds[i]);
  }
  free(fds);
}


int execute_child_with_pipes(Command* cmd){
  int pgm_count = 0;
  Pgm* pgm = cmd->pgm;
  int** fds = alloc_fds(pgm, &pgm_count);
  Pgm* first_program = pgm;
  
  
  int pid = fork();
  if (pid == 0) {
    // set background flag
    bg_flag = cmd->background;
    recursive_forking(first_program, fds, 0);
    
  }
  else {
    waitpid(pid, NULL, 0);
  }
  free_fds(fds, pgm_count);
}

int execute_command(Command* cmd){
  int pid, ret;
  Pgm* p_command = cmd->pgm;

  // redirection file paths
  infile_path = (cmd->rstdin) ? cmd->rstdin : NULL;
  outfile_path = (cmd->rstdout) ? cmd->rstdout : NULL;
  


  if(p_command->next != NULL){
    execute_child_with_pipes(cmd);
  }
  else {
    pid = fork();
    if (pid == 0) {
      // set background flag
      bg_flag = cmd->background;
      if(bg_flag){
         signal(SIGCHLD, sigchld_handler);

      }

      // check redirection
      redirect(infile_path, STDIN_FILENO);
      redirect(outfile_path, STDOUT_FILENO);

      execute_program(cmd->pgm->pgmlist);
    }
    else {
      conditional_wait(NULL, cmd->background);
    }
  }
  return 0;
}