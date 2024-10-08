# lsh Lab Report

Group: Lab Group 28

Group Members: Xuanzheng Jiang, Akshara Naineni, Rasmus Sandblom

## Meeting Timeline (Implementation order included)
13th Sept: Completed the warm-up quiz.

17th Sept: Finished `CTRL + D handling` and ` Baisc Program Execution`.

18th Spet: Finished built-in commands `cd` and `exit`. 

23rd Sept: Finished `Piping` and `I/O Redirection`.

24th Sept: Finished `Background Execution`, `No Zombies Clearout` and `Ctrl + C Keyboard Interrupt`.

26th Sept: Finished writing the lab report.

## Specifications

Our group has already met all the specifications outlined in the docs/README.md and has passed all the test cases used in the Python test scripts on local machines.

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

We utilized the `readline` function return value `line` (a char* pointer) to identify if the EOF is sent when the user interacts with the shell. If the `readline` return value is NULL, which means CTRL + D is passed to the shell process. In this case, we can simply exit the shell process with the _exit() function.

``` C
line = readline("> ");
if (line == NULL) {
    _exit(0);
}
```


2. **Basic Commands**: 

`execvp()`:

execvp() is the system call we chose to implement command execution. When execvp() is called, the process will replace the current process image with a new process image. This call makes sure that any other code after execvp() will not be executed.


```C
int execvp(const char *file, char *const argv[]);
```

- exec: Execute a program
- v: The argument list is provided as an array of pointers (or a vector)
- p: The system searches for the file in the directories listed in the PATH environment variable
- file: This is the name of the program to execute. If the file doesn't contain a slash (/), the system searches for it in the directories specified by the PATH environment variable.
- argv[]: This is an array of arguments passed to the program. The first argument (argv[0]) is typically the name of the program itself.

For non-builtin commands(`cd` and `exit`), they should be executed by sub-processes instead of the shell process, so fork() system call should be used to spawn a child process to take care of the process execution. The following code shows how a single program is executed. (Simplified Version)

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

Built-ins are the commands handled by shell itself, instead of executing an external binary program. Before calling function `execute_command()`, a pre-check should be made to ensure:
- only one `pgm` in the Command linked list.
- `pgm.pgmlist[0]` is `cd` or `exit`. 

If the first string is `cd`, we should make sure there are only 2 arguments in the argument vector (the second string should be a directory path). If the first string is `exit`, there are also 2 args allowed (the second string should be a return code).   

`exit` is an explicit way to exit the shell CLI (just like CTRL-D). We use the function `strcmp()` to identify that the string is indeed the  exit command.
``` C
else if(strcmp(built_in_cmd[0], "exit")== 0){
    _exit(exit_code);
}
```

`chdir()` is used to change the current directory of the shell process, we need to take care of exceptions `~` and `cd NULL` (no path specified), which means directing to the user's home directory (chdir() can't handle these). Under such circumstances, `strcat()` should be used to concatenate the home folder and current username. `getlogin()` is used for getting the username of the current user. 

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

The piping feature is implemented by duplicating pipe file descriptor to STDIN and STDOUT using system call `dup2()`. We define a two dimensional array of integers `fds[pgm_count - 1][2]` to store the pipe file descriptors. Two helper functions are defined in order to automate the fds array allocation and disposal.

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

Given the program's linked list is arranged in a reverse order, a recursive method of execution was chosen. One of the reasons for this was to improve the readability of the code, but also because of the nature of the data structure. The pipe function are able to arbitrarily execute any number of piped programs, with the proper syntax described below: 

```
pgm N ... | pgm N-1 ... | ... | pgm 1 ...
```

For a multiple pipes command, the shell process initially spawns a child process for pgm 1, and recursively check the following conditions:

- base case (pgm N): The first program to be executed, which is at the top of the recursive function stack. Only the WRITE_END of pipe[N-1] should be duplicated.
- middle case (pgm i, i = 2, 3, ..., N-1): These processes need to read from the previous process's output(READ_END of pipe[i]) and write to the next process's input(WRITE_END of pipe[i - 1]).
- last case (pgm 1): last program to be executed, only read from the READ_END of pipe[1].

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
Worth noting is that since the pipe functionality is a much more complicated endevour than the singel program execution. The choice was made to have it as a special case with its own subroutine `execute_child_with_pipes`.

```C
  if(p_command->next != NULL){
    execute_child_with_pipes(cmd);
  }
```

5. **I/O Redirection:**

The redirection feature is also implemented using `dup2()` system calling while no pipes are needed. Only the first and last processes need to take redirection into their account.

- First process (pgm N): check if input redirection needed
- Last process (pgm 1): check if output redirection needed


Here is how we define a redirection function:
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

The `redirect()` function will automatically check if a process needs input/out redirection, so a process just needs to call it before program execution.

``` C

// check redirection
redirect(infile_path, STDIN_FILENO);
redirect(outfile_path, STDOUT_FILENO);

execute_program(cmd->pgm->pgmlist);

```

6. **Background Execution:**

Only frontground processes need to blocking-wait for their children, while background processes can run quietly without blocking the shell process.

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

However, if parents leave their children to run without waiting, the children will become zombies when they exit. We will solve this problem in the **No Zombies** section.

7. **No Zombies:**

When children's executions end, they will send a signal `SIGCHLD` back to their parents to notify their exit. By catching `SIGCHLD`, we can resolve the zombie issue easily.

``` C
// catch background singal
// fixed: multiple background processes running
void sigchld_handler(int sig) {
  // only wait for the child hanged process 
  waitpid(-1, NULL, WNOHANG);
}

```

In order to handle the multiple background processes waiting, we discarded the original implementation tracking all the background children process ids  `child_pids` and blockingly wait. Instead, we prefer to use `WHOHANG` to non-blockingly wait for all the dead children.

To register a singal handler function:

``` C
signal(SIGCHLD, sigchld_handler);
```

8. **Ctrl-C Handling:**

Ctrl-C keyboard interrupt will send `SIGINT` signal to all the processes in the current process group, and a way to protect shell process and background processes from being killed is by registering `SIG_IGN` as the `SIGINT` handler. As for frontground processes, we can register `SIG_DFL` as their `SIGINT` handler, so that Ctrl-C interrupt can effortlessly kill all the frontground processes. 

Since a child process inherits signal settings from its parent during `fork()`, shell process can be set to:

```C
signal(SIGINT, SIG_IGN);
```

When frontground processes are spawned, they can explicitly register `SIG_DFL` to their SIGINT handler. Rest of the processes(background) will automatically ignore SIGINT during their execution.

```C
    pid = fork();
    if (pid < 0)
      ...
    if (pid == 0) {
      if (!cmd->background)
        signal(SIGINT, SIG_DFL);

      // check redirection
      redirect(infile_path, STDIN_FILENO);
      redirect(outfile_path, STDOUT_FILENO);

      execute_program(cmd->pgm->pgmlist);
    }
    else {

      if (!cmd->background) {
        fg_pid = pid;
        waitpid(pid, NULL, 0);
      }
    }
```

## Challanges

**Piping:**
With the pipes, there were a multitude of challenges regarding the specifics of how to implement it properly. One issue that occurred in the development process was how the pipes were initiated and closed. In the first iteration of the solution, we initialized the pipes with the `pipe()` command before the call to `recursive_forking()`. This proved to be troublesome however, since all the subsequent child processes created by `recursive_forking()` would inherit all the file descriptors. The consequence of this resulted in a very complicated collection of open file descriptors that proved troublesome to close properly. 

The solution to this issue was to not create a “global” array of file descriptors but rather create and assign file descriptors locally in the children. This makes sure that file descriptors are not inherited in an uncontrolled way and keeps the “bookkeeping” on which file descriptors are open simple. The subroutine `alloc_fds()` allocates the memory for the array of file descriptors based on the amount of processes to be run in each piped command.

**Built-in Commands:**
The development process for the built in commands went quite well. We had an issue where the file paths where different for different development environment. Since the code was mostly developed on Arch Linux, we needed to change the path to match the Debian based environment where the shell is supposed to operate. 

**Ctrl-C Handling:**
Sadly, we were certain we had a clean solution to the Ctrl+C handling, but last minute it appeared that one of the associated tests failed on the remote desktops. This error was unexpected and caught right before the deadline of the project, causing it to remain unfixed in the current version of `lsh.c`. We are overall content with our solution since it has been designed and implemented in a sound way, even though the failure of the test is regretfull.  

## Feedback 
We found that overall the lab sessions were an excellent challange and greatly improved our understanding of the underlying functionality of the operatingsystem. Reading up on how functions such as `pipe()`, `signal()` and other unix-calls actually worked and then be put to the test by implementing them felt very rewarding and a productive. Particularily, working with multiple processes was not only a challange in theoretical understanding, but also coding in general. Concerning the testing suite, we found it comprehensive and covering all major fault the program may have. In the development process, we used the tests when we thought we had a bug free program to catch additional bugs not noticed when doing manual testing. 
