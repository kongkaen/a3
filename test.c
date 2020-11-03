//
//  Patrick Dicks
//  Program 3 - smallsh
//  smallsh.c
//  CS 344 - Operating Systems
//  11/14/2018
//

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LINE_LEN 2048
#define MAX_NUM_ARG 512

int fgMode = 0;  // boolean foreground only mode

void catchSIGINT(int);  // terminate foreground child process with ^c
void catchSIGTSTP(int);  // enable/disable foreground only mode
void exitFunc(void);  // exit shell and kills any other processes or jobs
void cd(char *, int *);  // change working directory of shell
void status(int);  // print exit status/signal of last foreground process
char *readLine();  // read user input
void expandPid(char *, char *);  // expand "$$" into shell process ID
char **processLine(char *, char **, char **, int *);  // parse input
void bgProcessCheck(int *);  // check for completed backbround process
// non-built in command
void execute(char **, char **, char **, int, struct sigaction, int *);
// run inputted comand
void runCommand(char **, char **, char **, int *, struct sigaction, int *);
void smallshLoop(void);  // loop smallsh

int main(int argc, char *argv[]) { smallshLoop(); }

// catches SIGINT signal sent when user enters ^c
void catchSIGINT(int signo) {
    char *message = "Caught SIGINT.\n";
    write(STDOUT_FILENO, message, 15);
}

// catches SIGTSTP signal sent when user enters ^z
void catchSIGTSTP(int signo) {
    if (!fgMode) {  // turn foreground only mode on
        char *message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 49);
        fflush(stdout);  // flush output buffers
        fgMode = 1;
    }
    else {  // turn foreground only mode off
        char *message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 29);
        fflush(stdout);  // flush output buffers
        fgMode = 0;
    }
}

void exitFunc() { _exit(0); }

void cd(char *path, int *exitStatus) {
    int ret;

    // set path to HOME environment variable if not specified
    if (path == NULL) { path = getenv("HOME"); }

    ret = chdir(path);
    if (ret < 0) { perror("chdir()"); *exitStatus = 1; }
}

void status(int exitStatus) {
    if (WIFEXITED(exitStatus) != 0) {  // exited
        printf("exit value %d\n", WEXITSTATUS(exitStatus));
        fflush(stdout);  // flush output buffers
    }
    else if (WIFSIGNALED(exitStatus) != 0) {  // terminated by signal
        printf("terminated by signal %d\n", WTERMSIG(exitStatus));
        fflush(stdout);  // flush output buffers
    }
}

char *readLine() {
    int len = MAX_LINE_LEN;
    char *line = (char *)malloc(len * sizeof(char));
    if (line == NULL) {
        fprintf(stderr, "failed to allocate memory.");
        exit(1);
    }
    fgets(line, len, stdin);
    return line;
}

void expandPid(char *line, char *p) {
    char firstPart[256];
    memset(firstPart, '\0', sizeof(firstPart));
    char secondPart[256];
    memset(secondPart, '\0', sizeof(secondPart));

    int count = 0;
    char *c;
    for (c = line; c != p; c++) {
        firstPart[count] = *c;
        count++;
    }

    count = 0;
    for (c += 2; *c != '\0'; c++) {
        secondPart[count] = *c;
        count++;
    }

        sprintf(line, "%s%d%s", firstPart, getpid(), secondPart);
}

char **processLine(char *line, char **input, char **output, int *bgProcess) {
    char **args = malloc(MAX_NUM_ARG * sizeof(char *));
    if (args == NULL) {
        fprintf(stderr, "failed to allocate memory.");
        exit(1);
    }

    char *p = strstr(line, "$$");  // check if line contains "$$"
    if (p) { expandPid(line, p); }  // if so, expand it

    char *token = strtok(line, " \n");
    int count = 0;
    while (token != NULL) {
        if (strcmp(token, "<") == 0) {  // stdin
            token = strtok(NULL, " \n");
            *input = token;
            token = strtok(NULL, " \n");
        }
        else if (strcmp(token, ">") == 0) {  // stdout
            token = strtok(NULL, " \n");
            *output = token;
            token = strtok(NULL, " \n");
        }
        else {
            args[count] = token;
            token = strtok(NULL, " \n");
            count++;
        }
    }

    if (count > 0) {  // if user did not enter blank line
        if (strcmp(args[count - 1], "&") == 0) {  // check for "&"
            if (fgMode == 0) {
                *bgProcess = 1;  // set background process to true
            }
            count--;  // decrement count to replace "&" in array
        }

        args[count] = NULL;  // indicates end of parameters
    }

    return args;
}

void bgProcessCheck(int *exitStatus) {
    pid_t wpid = waitpid(-1, exitStatus, WNOHANG);
    while (wpid > 0) {
        printf("background pid %d is done:\n", wpid);
        fflush(stdout);  // flush output buffer
        status(*exitStatus);
        // check for completed child background processes
        wpid = waitpid(-1, exitStatus, WNOHANG);
    }
}

// Creates new process using fork and runs command using exec
void execute(char **args, char **input, char **output, int bgProcess,
struct sigaction SIGINT_action, int *exitStatus) {
    pid_t spawnPid = -5;  // initialize with junk value
    int sourceFD, targetFD, result;

    spawnPid = fork();  // create new process
    switch (spawnPid) {
        case -1:  // child process creation unsuccessful
            perror("fork()");
            break;
        case 0:  // child
            if (bgProcess) {
                // redirect stdin if not specified
                if (*input == NULL) { *input = "/dev/null"; }
                // redirect stdout if not specified
                if (*output == NULL) { *output = "/dev/null"; }
            }
            else {  // allow foreground process to be interrupted
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }

            if (*input != NULL) {  // redirect stdin to specified file
                sourceFD = open(*input, O_RDONLY);
                if (sourceFD == -1) {
                    printf("cannot open %s for input\n", *input);
                    fflush(stdout);  // flush output buffers
                    *exitStatus = 1;
                    exit(1);
                }

                result = dup2(sourceFD, 0);
                if (result == -1) {
                    perror("source dup2()");
                    *exitStatus = 1;
                    exit(1);
                }
            }

            if (*output != NULL) {  // redirect stdout to specified file
                targetFD = open(*output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (targetFD == -1) {
                    perror("target open()");
                    *exitStatus = 1;
                    exit(1);
                }

                result = dup2(targetFD, 1);
                if (result == -1) {
                    perror("target dup2()");
                    *exitStatus = 1;
                    exit(1);
                }
            }

            if (execvp(args[0], args) < 0) {  // execute command
                printf("%s: no such file or directory", args[0]);
                fflush(stdout);  // flush output buffers
                *exitStatus = 1;
                exit(1);
            }
            break;  // arbitrary
        default:  // parent
            if (bgProcess) {
                printf("background pid is %d\n", spawnPid);
                fflush(stdout);  // flush output buffers
            }
            else {
                waitpid(spawnPid, exitStatus, 0);
                // if process was terminated by a signal, print its status
                if (WIFSIGNALED(*exitStatus) != 0) { status(*exitStatus); }
            }

            // check for completed background processes
            bgProcessCheck(exitStatus);
    }
}

void runCommand(char **args, char **input, char **output, int *bgProcess,
struct sigaction SIGINT_action, int *exitStatus) {
    // Ignore blank lines and comments.
    if ((args[0] != NULL) && (*(args[0]) != '#')) {
        if (strcmp(args[0], "exit") == 0) { exitFunc(); }
        else if (strcmp(args[0], "cd") == 0) { cd(args[1], exitStatus); }
        else if (strcmp(args[0], "status") == 0) { status(*exitStatus); }
        else {  // non-built in command
            execute(args, input, output, *bgProcess, SIGINT_action, exitStatus);
        }
    }
}

void smallshLoop(void) {
    int exitStatus = 0;
    struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0};

    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;

    sigaction(SIGINT, &SIGINT_action, NULL);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while (1)  // break free once exit command is run
    {
        int bgProcess = 0;  // boolean background process value

        char **input = malloc(sizeof(char *));
        if (input == NULL) {
            fprintf(stderr, "failed to allocate memory.");
            exit(1);
        }
        *input = NULL;

        char **output = malloc(sizeof(char *));
        if (output == NULL) {
            fprintf(stderr, "failed to allocate memory.");
            exit(1);
        }
        *output = NULL;

        printf(": ");
        fflush(stdout);  // flush output buffers

        char *line = readLine();
        char **args = processLine(line, input, output, &bgProcess);
        runCommand(args, input, output, &bgProcess, SIGINT_action, &exitStatus);

        // clean up
        memset(args, 0, (MAX_NUM_ARG*sizeof(args)));
        free(line);
        free(input);
        free(output);
        free(args);
    }
}
