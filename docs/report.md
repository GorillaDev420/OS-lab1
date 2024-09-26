# lsh Lab Report

Group: Lab Group 28

Group Members: Xuanzheng Jiang, Akshara Naineni, Rasmus Sandblom

## Meeting Timeline (Implementation order included)
13rd Sept: Completed the warm-up quiz.

17th Sept: Finished `CTRL + D handling` and ` Baisc Program Execution`.

18th Spet: Finished built-in commands `cd` and `exit`. 

23rd Sept: Finished `Piping` and `I/O Redirection`.

24th Sept: Finished `Background Execution`, `No Zombies Clearout` and `Ctrl + C Keyboard Interrupt`.

26th Sept: Finished writing the lab report.

## Specifications

Our group have already met all the specifications outlined in the docs/README.md and have passed all the test cases used in the python test scripts.

1. **Ctrl-D Handling**: Respond to Ctrl-D (EOF) to exit.

2. **Basic Commands**: Execute simple commands like `ls`, `date`, and `who`. It should also recognize the `PATH` environment variable to locate commands.

3. **Background Execution**: Allow running commands in the background, e.g., `sleep 30 &`.

4. **Piping**: Support one or more pipes between commands, e.g., `ls | grep out | wc -w`.

5. **I/O Redirection**: Enable standard input and output redirection to files, e.g., `wc -l < /etc/passwd > accounts`.

6. **Built-ins**: Provide `cd` and `exit` as built-in functions.

7. **Ctrl-C Handling**: Ctrl-C should terminate the current foreground process but not the shell itself. Ctrl-C should not affect background jobs.

8. **No Zombies**: Your shell should not leave any zombie processes behind.

## How we implemented the specifications

1. **Ctrl-D Handling**: 

We utilized the `readline` function return value `line` (a char* pointer) to identity if the EOF is sent when user interacting with the shell. If the `readline` return value is NULL, which means CTRL + D is passed to the shell process, we can exit the shell process.

``` C
line = readline("> ");
if (line == NULL) {
    _exit(0);
}
```


2. **Basic Commands**: 

`execvp()`:

execvp() is the system call we chose to implement command execution. When execvp() is called, the process will replaces the current process image with a new process image hence any other code after execvp() will not be executed.


```C
int execvp(const char *file, char *const argv[]);
```

- exec: Execute a program
- v: The argument list is provided as an array of pointers (or a vector)
- p: The system searches for the file in the directories listed in the PATH environment variable
- file: This is the name of the program to execute. If the file doesn't contain a slash (/), the system searches for it in the directories specified by the PATH environment variable.
- argv[]: This is an array of arguments passed to the program. The first argument (argv[0]) is typically the name of the program itself.

For non-builtin commands(`cd` and `exit`), they should be executed by sub-processes instead of the shell process, so fork() system call should be used to spawn a child process to take care of the process execution bussiness. The following code shows how a single program is executed. (Simplified Version)

```C
void execute_program(char** pl){
  execvp(pl[0], pl);
}

execute_command (Command* cmd) {
    ......

    pid = fork();
    if (pid < 0)
        ...

    if (pid == 0) {
      execute_program(cmd->pgm->pgmlist);
    }
    else {
        waitpid(pid, NULL, 0);
    }
}
```

3. **Built-ins**: 

Built-ins are the commands handled by shell itself, instead of executing an external binary program. Before calling function `execute_command()`, pre-check should be made to ensure:
- only one `pgm` in the Command linked list.
- `pgm.pgmlist[0]` is `cd` or `exit`. 

If the first string is `cd`, we should make sure there are only 2 arguments in the argument vector (the second string should be a directory path). If the first string is `exit`, there are also 2 args allowed (the second string should be a return code).   

`exit` is an expilict way to exit the shell CLI (just like CTRL-D). We use function `strcmp()` to identify the exit command.
``` C
else if(strcmp(built_in_cmd[0], "exit")== 0){
    _exit(exit_code);
}
```

`chdir()` is used to change the current directory of the shell process, we need to take care of exceptions `~` and `cd NULL` (no path specify), which mean directing to the user's home directory (chdir() can't handle these). Under such circumstances, `strcat()` should be used to concatenate `/home` and current username. `getlogin()` for getting username. 

``` C
if (strcmp(built_in_cmd[0], "cd")==0){

    // "cd NULL" and "cd ~" check
    if(strcmp(built_in_cmd[1] == NULL || built_in_cmd[1], "~")== 0){

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
}
```

4. **Piping:**

Piping feature is implemented by duplicating pipe file descriptor to STDIN and STDOUT using system call `dup2()`. We define a 2D int array `fds[pgm_count - 1][2]` to store the pipe file descriptors. Two helper functions are defined to automate the fd array allocation and disposition.

``` C
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

```

Given the program linked list is arranged in a reverse order, we use the idea of recursive function to implement multiple pipes mechanism. 

```
pgm N ... | pgm N-1 ... | ... | pgm 1 ...
```

For a multiple pipes command, the shell process firstly spawn a child process for pgm 1, and recursively

- base case (pgm N): The first program to be executed, which is at the top of recursive function stack. Only the WRITE_END of pipe[N-1] should be duplicated.
- middle case (pgm i, i = 2, 3, ..., N-1): These processes need to read from the previous process's output(READ_END of pipe[i]) and write to the next process's input(WRITE_END of pipe[i - 1]).
- last case (pgm 1): last program to be executed, nly read from the READ_END of pipe[1].

``` C
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
    }
    // parent
    else{
      //chld_pid = pid;
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

```

5. **I/O Redirection:**

Redirection feature is also implemented using `dup2()` system calling while no pipes needed. Only the first and last processes need to take redirection into their account.

- First process (pgm N): check if input redirection needed
- Last process (pgm 1): check if output redirection needed


Here is how we define a rediction function:
``` C

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
    else {
      ... // error
    }
    break;
  case STDOUT_FILENO:
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd > 0) {
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    else {
      ... // error
    }
    break;
  
  default:
    break;
  }
  return fd;
}

```

The `redirect()` fucntion will automatically check if a process needs input/out redirection, so a process just need to call it before program execution.

``` C

// check redirection
redirect(infile_path, STDIN_FILENO);
redirect(outfile_path, STDOUT_FILENO);

execute_program(cmd->pgm->pgmlist);

```

6. **Background Execution:**

Only frontground processes need to blocking-wait for its children, while background processes can run quietly without blocking the shell process.

The parent-child pattern shall look as follows:

``` C
    pid = fork();
    if (pid < 0)
      // error

    // child
    if (pid == 0) {
      execute_program(cmd->pgm->pgmlist);
    }
    // parent
    else {
      if (!cmd->background) {
        waitpid(pid, NULL, 0);
      }
    }
```

However, if parents leave their children run without waiting, the chidren will become zombies when they exit. We will solve this problem in the **No Zombies** section.

7. **No Zombies:**

When children executions end, they will send a signal `SIGCHLD` back to their parents to notify their exit. By catching `SIGCHLD`, we can resolve the zombie issue easily.

``` C
// catch background singal
// fixed: multiple background processes running
void sigchld_handler(int sig) {
  // only wait the child hanged process 
  waitpid(-1, NULL, WNOHANG);
}

```

In order to handle the multiple background processes waiting, we discarded the original implementation tracking all the background children process ids  `child_pids` and blockingly wait. Instead, we prefer use `WHOHANG` to non-blockingly wait all the dead children.

To register a singal handler function:

``` C
signal(SIGCHLD, sigchld_handler);
```

8. **Ctrl-C Handling:**



## Chanllanges