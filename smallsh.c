/*	Natthaphong Kongkaew
		Assignment 3: smallsh
		CS 344
		Oregon State University
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

char* getLine();	// Get input from user
void parseLine(char *line, char *arg[], char input[], char output[], int pid, int *backgroundProcess);	// Parse input to arguments
void cd(char *path, int *exitStatus);	// Directory
void printExitStatus(int exitStatus);	// Print exit status of the program
void catchSIGTSTP();	// Catch SIGTSTP signal
void execute(char *argument[], int *exitStatus, struct sigaction SIGINT_action, int *backgroundProcess, char input[], char output[]); // Execute the commands


// Main function
int main() {

	int pid = getpid();

	int exitStatus = 0;
	int backgroundProcess = 0;

	// initialize array of input file name and output file name
	char input[100] = {0};
	char output[100] = {0};

	// Initialize input argument to support maximum of 512 arguments
	char *line;
	char *argument[512];

	int i;
	for (i=0; i<512; i++) {
		argument[i] = NULL;
	}


	// Signal to ignore ^C
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Redirect ^Z to function catchSIGTSTP()
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	// Looping unless calling exit.
	do {
		line = getLine();	// Get input line from user
		parseLine(line, argument, input, output, pid, &backgroundProcess);	// Parse input line into arguments

		// Ignore blank line and comment #
		if (argument[0][0] != '#' && argument[0][0] != '\0') {

			// exit
			if (strcmp(argument[0], "exit") == 0) {
				_exit(0);
			}

			// redirect to directory
			else if (strcmp(argument[0], "cd") == 0) {
				cd(argument[1], &exitStatus);
			}

			// get status
			else if (strcmp(argument[0], "status") == 0) {
				printExitStatus(exitStatus);
			}

			// if arguments not above
			else {
				execute(argument, &exitStatus, SIGINT_action, &backgroundProcess, input, output); // Execute the argument
			}
		}

		// Reset arguments to NULL
		for (i=0; argument[i]; i++) {
			argument[i] = NULL;
		}

		backgroundProcess = 0;	// Set background process to 0
		input[0] = '\0';	// Reset input file name
		output[0] = '\0';	// Reset output file name

	} while (1);

		return 0;
}

/*	Get input from user and read line by line
		Return string line input from user
*/
char* getLine() {

	// Initialize pointer for line input
	char *line = (char *)malloc(MAX_COMMANDLENGTH * sizeof(char));
	if (line == NULL) {
			fprintf(stderr, "failed to allocate memory.");
			exit(1);
	}

	// Prompt user to get input command when ": " appear
	printf(": ");
	fflush(stdout);
	fgets(line, MAX_COMMANDLENGTH, stdin);

	return line;
}

/*	Input: 	char *line				pointer to input line
						char* arg[]				Argument array
						char input[]			array of input file name
						char output[]			array of output file name
						int pid						process ID
						int *backgroundProcess		integer defines background process status

		output: void
*/
void parseLine(char *line, char *arg[], char input[], char output[], int pid, int *backgroundProcess){

	// Set token to be white space and newline
	char *token = strtok(line, " \n");
	int i,j;

	// If input is blank, return blank
	if (!strcmp(line, "")) {
		arg[0] = strdup("");
		return;
	}

	for (i = 0; token; i++) {

		// Check for & and set background process on
		if (strcmp(token, "&") == 0) {
			*backgroundProcess = 1;
		}

		// Check for < define as input file
		else if (strcmp(token, "<") == 0) {
			token = strtok(NULL, " \n");
			strcpy(input, token);
		}

		// Check for > define as output file
		else if (strcmp(token, ">") == 0) {
			token = strtok(NULL, " \n");
			strcpy(output, token);
		}

		// else, it's command
		else {
			arg[i] = strdup(token);

			// Check for $$ to expand PID
			for (j=0; arg[i][j]; j++) {
				if (arg[i][j] == '$' &&
					 arg[i][j+1] == '$') {
					arg[i][j] = '\0';
					snprintf(arg[i], 256, "%s%d", arg[i], pid);
				}
			}
		}

		token = strtok(NULL, " \n");
	}
}

/*	Redirect user to directory
		Input: 	char *path			pointer to directory
						int *exitStatus	integer of exit status

		output: void
*/
void cd(char *path, int *exitStatus) {


		int newPath;

		// set path to HOME environment variable if not specified
    if (path == NULL) {
			path = getenv("HOME");
		}

    newPath = chdir(path);

		// if directory doesn't exist
    if (newPath < 0) {
			perror("chdir()"); *exitStatus = 1;
		}
}

/*
		Function to get exit status of the command
*/
void printExitStatus(int exitStatus) {
		// If exit by status
    if (WIFEXITED(exitStatus) != 0) {
        printf("exit value %d\n", WEXITSTATUS(exitStatus));
    }

		// If exit by signal
    else if (WIFSIGNALED(exitStatus) != 0) {
        printf("terminated by signal %d\n", WTERMSIG(exitStatus));
    }
}

/*
		Function to switch to/out background mode when user enters ^z
*/
void catchSIGTSTP() {

	// If background mode is on, switch to forground mode
	if (bgMode == 1) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout);
		bgMode = 0;
	}

	// If background mode is off, turn off foreground mode
	else {
		char* message = "Exiting foreground-only mode\n";
		write (1, message, 29);
		fflush(stdout);
		bgMode = 1;
	}
}

/*
		Function to execute the command from argument array
*/
void execute(char* argument[], int* exitStatus, struct sigaction SIGINT_action, int* backgroundProcess, char input[], char output[]) {

	pid_t spawnPid = -5;  // initialize with junk
	int sourceFD, targetFD, result;

	spawnPid = fork();  // fork a new process

	switch (spawnPid) {

		case -1:	// fail to create child process
			perror("fork() failed");
			break;

		case 0:
			// allow foreground process to be interrupted
			SIGINT_action.sa_handler = SIG_DFL;
			sigaction(SIGINT, &SIGINT_action, NULL);

			// Handle input file
			if (strcmp(input, "")) {
				// open source file
				sourceFD = open(input, O_RDONLY);
				if (sourceFD == -1) {
					printf("Cannot open %s for input\n", input);
					exit(1);
				}

				 // Redirect stdin to source file
				result = dup2(sourceFD, 0);
				if (result == -1) {
					perror("Cannot assign input file\n");
					exit(2);
				}
				// close
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			// Handle output file
			if (strcmp(output, "")) {
				// Open target file
				targetFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0640);
				if (targetFD == -1) {
					perror("Cannot open output file\n");
					exit(1);
				}

				// Redirect stdout to target file
				result = dup2(targetFD, 1);
				if (result == -1) {
					perror("Cannot assign output file\n");
					exit(2);
				}
				// close
				fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}

			// Execute the commands
			if (execvp(argument[0], (char* const*)argument)) {
				printf("%s: no such file or directory\n", argument[0]);
				fflush(stdout); // flush output buffers
				exit(2);
			}
			break;

			default: // not -1 or 0, it's parent
				// If background mode is on, use WNOHANG to get pid
				if (*backgroundProcess && bgMode) {
					pid_t waitPid = waitpid(spawnPid, exitStatus, WNOHANG);
					printf("background pid is %d\n", spawnPid);
					fflush(stdout);
				}
				// else, get pid as normal
				else {
					pid_t waitPid = waitpid(spawnPid, exitStatus, 0);
				}

			// if the process terminated by signal
			while ((spawnPid = waitpid(-1, exitStatus, WNOHANG)) > 0) {
				printf("child %d terminated\n", spawnPid);
				printExitStatus(*exitStatus);
				fflush(stdout);
			}

	}

}
