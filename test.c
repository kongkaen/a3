/*	Assignment 3: smallsh
		Natthaphong Kongkaew
		ONID: kongkaen
		CS 344 Oregon State University
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#define MAX_COMMANDLENGTH 2048
#define MAX_ARGUMENT 512

int bgMode = 1;

char* getLine() {

	char *line = (char *)malloc(MAX_COMMANDLENGTH * sizeof(char));
	if (line == NULL) {
			fprintf(stderr, "failed to allocate memory.");
			exit(1);
	}

	//int i, j;

	// Prompt user to get input command when ": " appear
	printf(": ");
	fflush(stdout);
	fgets(line, MAX_COMMANDLENGTH, stdin);

	return line;
}

void parseLine(char *line, char* arg[], char input[], char output[], int pid, int *backgroundProcess){
	char *token = strtok(line, " \n");

	int i,j;

	// If it's blank, return blank
	if (!strcmp(line, "")) {
		arg[0] = strdup("");
		return;
	}
/*
	//check if string line contained $$
	char *p = strstr(line, "$$");
	if (p) { expandPid(line, p); }
*/
	for (i = 0; token; i++) {
		// Check for & to be a background process
		if (strcmp(token, "&") == 0) {
			*backgroundProcess = 1;
		}
		// Check for < to denote input file
		else if (strcmp(token, "<") == 0) {
			token = strtok(NULL, " \n");
			strcpy(input, token);
		}
		// Check for > to denote output file
		else if (strcmp(token, ">") == 0) {
			token = strtok(NULL, " \n");
			strcpy(output, token);
		}
		// Otherwise, it's part of the command!
		else {
			arg[i] = strdup(token);
			// Replace $$ with pid
			// Only occurs at end of string in testscirpt
			for (j=0; arg[i][j]; j++) {
				if (arg[i][j] == '$' &&
					 arg[i][j+1] == '$') {
					arg[i][j] = '\0';
					snprintf(arg[i], 256, "%s%d", arg[i], pid);
				}
			}
		}
		// Next!
		token = strtok(NULL, " \n");
	}
}

void cd(char *path, int *exitStatus) {
    int ret;

    // set path to HOME environment variable if not specified
    if (path == NULL) {
			path = getenv("HOME");
		}

    ret = chdir(path);

    if (ret < 0) {
			perror("chdir()"); *exitStatus = 1;
		}
}

void printExitStatus(int exitStatus) {
    if (WIFEXITED(exitStatus) != 0) {  // exited
        printf("exit value %d\n", WEXITSTATUS(exitStatus));
        fflush(stdout);  // flush output buffers
    }
    else if (WIFSIGNALED(exitStatus) != 0) {  // terminated by signal
        printf("terminated by signal %d\n", WTERMSIG(exitStatus));
        fflush(stdout);  // flush output buffers
    }
}

void catchSIGTSTP(int signo) {

	// If it's 1, set it to 0 and display a message reentrantly
	if (bgMode == 1) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout);
		bgMode = 0;
	}

	// If it's 0, set it to 1 and display a message reentrantly
	else {
		char* message = "Exiting foreground-only mode\n";
		write (1, message, 29);
		fflush(stdout);
		bgMode = 1;
	}
}

void execute(char* argument[], int* exitStatus, struct sigaction SIGINT_action, int* backgroundProcess, char input[], char output[]) {

	pid_t spawnPid = -5;  // initialize with junk
	int sourceFD, targetFD, result;

	spawnPid = fork();  // fork new process

	switch (spawnPid) {

		case -1:// fail to create child process
			perror("fork() failed");
			break;

		case 0:
			// The process will now take ^C as default
			SIGINT_action.sa_handler = SIG_DFL;
			sigaction(SIGINT, &SIGINT_action, NULL);

			// Handle input
			if (strcmp(input, "")) {
				// open it
				sourceFD = open(input, O_RDONLY);
				if (sourceFD == -1) {
					printf("Cannot open %s for input\n", input);
					exit(1);
				}
				// assign it
				result = dup2(sourceFD, 0);
				if (result == -1) {
					perror("Cannot assign input file\n");
					exit(2);
				}
				// trigger its close
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			// Handle output
			if (strcmp(output, "")) {
				// open it
				targetFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (targetFD == -1) {
					perror("Cannot open output file\n");
					exit(1);
				}
				// assign it
				result = dup2(targetFD, 1);
				if (result == -1) {
					perror("Cannot assign output file\n");
					exit(2);
				}
				// trigger its close
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}

			// Execute it!
			if (execvp(argument[0], (char* const*)argument)) {
				printf("%s: no such file or directory\n", argument[0]);
				fflush(stdout);
				exit(2);
			}
			break;

			default:
				// Execute a process in the background ONLY if bgMode
				if (*backgroundProcess && bgMode) {
					pid_t actualPid = waitpid(spawnPid, exitStatus, WNOHANG);
					printf("background pid is %d\n", spawnPid);
					fflush(stdout);
				}
				// Otherwise execute it like normal
				else {
					pid_t actualPid = waitpid(spawnPid, exitStatus, 0);
				}

			// Check for terminated background processes!
			while ((spawnPid = waitpid(-1, exitStatus, WNOHANG)) > 0) {
				printf("child %d terminated\n", spawnPid);
				printExitStatus(*exitStatus);
				fflush(stdout);
			}

	}

}

int main() {

	int pid = getpid();
	int inLoop = 1;
	int i;
	int exitStatus = 0;
	int backgroundProcess = 0;

	// initialize array of input file name and output file name
	char input[100] = {0};
	char output[100] = {0};

	// Initialize input argument to support maximum of 512 arguments
	char* line;
	char* argument[512];

	for (i=0; i<512; i++) {
		argument[i] = NULL;
	}


	// Signal Handlers

	// Ignore ^C
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Redirect ^Z to catchSIGTSTP()
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);


	do {

	line = getLine();
	//printf("line is %s ", line);

	parseLine(line, argument, input, output, pid, &backgroundProcess);

	//printf("argument 3 is %s\n", argument[2]);

	// ignore blank from command #
	if (argument[0][0] != '#' && argument[0][0] != '\0') {
		// exit
		if (strcmp(argument[0], "exit") == 0) {
			inLoop = 0;
		}

		else if (strcmp(argument[0], "cd") == 0) {
			cd(argument[1], &exitStatus);
		}

		// STATUS
		else if (strcmp(argument[0], "status") == 0) {
			printExitStatus(exitStatus);
		}

		// Anything else
		else {
			execute(argument, &exitStatus, SIGINT_action, &backgroundProcess, input, output);
		}
	}

	// Reset variables
	for (i=0; argument[i]; i++) {
		argument[i] = NULL;
	}
	backgroundProcess = 0;
	input[0] = '\0';
	output[0] = '\0';


	} while (inLoop);

	return 0;
}
