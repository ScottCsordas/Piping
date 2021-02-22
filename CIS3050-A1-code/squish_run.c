/*
*scsordas 0998149
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glob.h>
#include "squish_run.h"
#include "squish_tokenize.h"

/**
 * Print a prompt if the input is coming from a TTY
 */
static void prompt(FILE *pfp, FILE *ifp)
{
	if (isatty(fileno(ifp))) {
		fputs(PROMPT_STRING, pfp);
	}
}

//handle the command if file redirection is necessary
void isFile(char ** const tokens){
	char* input;
	char* output;
	int i = 0;

	//find the end of the command and replace the file redirection with null to end the tokens there. then get the command after into a sting
	while(tokens[i] != NULL){
		if(strcmp(tokens[i], ">") == 0){
			tokens[i] = NULL;
			output = tokens[i+1];
		}
		else if(strcmp(tokens[i], "<") == 0){
			tokens[i] = NULL;
			input = tokens[i+1];
		}
		i++;
	}

	int status = 0;
	int pid, exitting;
	//fork and run the command with the file redirection
	if ((pid = fork())< 0){
		perror("fork");
	}

	if(pid == 0){
		if(output != NULL){
			freopen(output, "w", stdout);
		}
		if(input != NULL){
			if(access(input, F_OK) != 1){
				freopen(input, "r", stdin);
			}
			else{
				printf("error input file does not exist \n");
			}
		}
		execvp(tokens[0], tokens);
	}
	exitting = wait(&status);
	if (WEXITSTATUS(status) == 0)
	{
		printf("Child (%d) exitted -- success (status %d)\n",
				exitting, WEXITSTATUS(status));
	}else if (WEXITSTATUS(status) != 0){
		printf("Child (%d) exitted -- failure (status %d)\n",
				exitting, WEXITSTATUS(status));
	} else {
		printf("Child (%d) did not exit (crashed!)\n", exitting);
	}

	return;
}

void isPipe(int index, char ** const tokens, char*** CommandArray){
		int watcherPID;
		int watcherstat;
		int pipefds[2];
		int childpid;
		int exitting;
		int status = 0;



		if ((watcherPID = fork())< 0){
			perror("fork");
		}
		if (watcherPID == 0){

			//loop through the commands with pipes forking for each pipe
				for(int i = 0; i < index; i++){

					if (pipe(pipefds) < 0 ) {
						perror("pipe");
						exit(1);
					}

					if ((childpid = fork()) < 0)
					{
						perror("fork");
						exit(1);
					}
					if (childpid == 0) {

						//close(0);
						dup2(pipefds[1], 1);
						close(pipefds[1]);
						close(pipefds[0]);
						execvp(*(CommandArray[i]), CommandArray[i]);
					}
					exitting = wait(&status);


					dup2(pipefds[0], 0);
					close(pipefds[0]);
					close(pipefds[1]);

					if (WEXITSTATUS(status) == 0)
					{
						printf("Child (%d) exitted -- success (status %d)\n",
								exitting, WEXITSTATUS(status));
					}else if (WEXITSTATUS(status) != 0){
						printf("Child (%d) exitted -- failure (status %d)\n",
								exitting, WEXITSTATUS(status));
					} else {
						printf("Child (%d) did not exit (crashed!)\n", exitting);
					}
				}

			//base case receive the final pipe
			if (pipe(pipefds) < 0 ) {
				perror("pipe");
				exit(1);
			}

			if ((childpid = fork()) < 0)
			{
				perror("fork");
				exit(1);
			}
			if (childpid == 0) {

				//close(0);
				dup2(pipefds[0], 0);
				close(pipefds[0]);
				close(pipefds[1]);
			}

			execvp(*(CommandArray[index]), CommandArray[index]);
		}
		exitting = wait(&status);
		if (WEXITSTATUS(status) == 0)
		{
			printf("Child (%d) exitted -- success (status %d)\n",
					exitting, WEXITSTATUS(status));
		}else if (WEXITSTATUS(status) != 0){
			printf("Child (%d) exitted -- failure (status %d)\n",
					exitting, WEXITSTATUS(status));
		} else {
			printf("Child (%d) did not exit (crashed!)\n", exitting);
		}
		waitpid(watcherPID, &watcherstat, 0);
		return;
}

/**
 * Actually do the work
 */
int execFullCommandLine(
		FILE *ofp,
		char ** const tokens,
		int nTokens,
		int verbosity)
{
	if (verbosity > 0) {
		fprintf(stderr, " + ");
		fprintfTokens(stderr, tokens, 1);
	}
	/** Now actually do something with this command, or command set */

	/*fprintf(stderr, " test \n");*/
	/*prints the command as separate tokens*/
  fprintfTokens(stderr, tokens, 1);

	//handle globbing if it exsists
	glob_t g;
	memset(&g, 0, sizeof(glob_t));

	glob(tokens[0], GLOB_NOCHECK, NULL, &g);

	for(int i = 1; i < nTokens; i++){
		glob(tokens[i],  GLOB_NOCHECK | GLOB_APPEND, NULL, &g);
	}


	/*
	*check for pipes in the command if none run normally
	*if pipes exist break up the tokens into each set of commands to be piped
  */
	//find the number of pipes
	//find if there is redirection to be done
	int numPipes = 0;
	int redirect = 0;
	for (int i = 0; i < nTokens; i++){
		if(strcmp(g.gl_pathv[i], "|") == 0){
			numPipes++;
		}
		if(strcmp(g.gl_pathv[i], "<") == 0){
			redirect++;
		}
		if(strcmp(g.gl_pathv[i], ">") == 0){
			redirect++;
		}
	}

	/*
	fprintfTokens(stderr, CommandArray[0], 1);
	fprintfTokens(stderr, CommandArray[index], 1);
	*/
 //now run the pipes
	if(numPipes != 0){
		//break up the amount of pipes into an array of commands to be executed within the pipes
		char*** CommandArray = malloc(sizeof(char**) * (numPipes + 1));
		CommandArray[0] = g.gl_pathv;
		int index = 0;
		for (int i = 0; i < nTokens; i++){

			if(strcmp(g.gl_pathv[i], "|") == 0){
				index++;
				g.gl_pathv[i] = 0;
				CommandArray[index] = &g.gl_pathv[i+1];
			}
		}
		isPipe(index, g.gl_pathv, CommandArray);
		free(CommandArray);
	}
	else if(redirect != 0){
		isFile(g.gl_pathv);
	}
	//functionality for cd
	else if(strcmp(tokens[0], "cd") == 0){
		if(tokens[1] != NULL){
			chdir(tokens[1]);
		}
		else{
			printf("invalid directory\n");
		}
	}
	//functionality for exit
	else if(strcmp(tokens[0], "exit") == 0){
		exit(0);
	}
	//if no pipe run normally
	else{
		/*
	  *code taken from example vssh.c
	  */
			int status = 0;
			int pid, exittingPid;
			/* fork amd run exec with the tokens command*/
			if ((pid = fork()) == 0)
			{
				execvp(g.gl_pathv[0], g.gl_pathv);
				perror("failed running exec()");
				exit (-1);
			}

			if (pid < 1)
			{
				perror("failed running fork()");
				exit (-1);
			}


			/** wait for our child command */
			exittingPid = wait(&status);
			/*print out exit status of child process*/
			if (WEXITSTATUS(status) == 0)
			{
				printf("Child (%d) exitted -- success (status %d)\n",
						exittingPid, WEXITSTATUS(status));
			}else if (WEXITSTATUS(status) != 0){
				printf("Child (%d) exitted -- failure (status %d)\n",
						exittingPid, WEXITSTATUS(status));
			} else {
				printf("Child (%d) did not exit (crashed!)\n", exittingPid);
			}
	}

	globfree(&g);
	return 1;
}

/**
 * Load each line and perform the work for it
 */
int
runScript(
		FILE *ofp, FILE *pfp, FILE *ifp,
		const char *filename, int verbosity
	)
{
	char linebuf[LINEBUFFERSIZE];
	char *tokens[MAXTOKENS];
	int lineNo = 1;
	int nTokens, executeStatus = 0;

	fprintf(stderr, "SHELL PID %ld\n", (long) getpid());

	prompt(pfp, ifp);
	while ((nTokens = parseLine(ifp,
				tokens, MAXTOKENS,
				linebuf, LINEBUFFERSIZE, verbosity - 3)) > 0) {
		lineNo++;
		/*fprintf(stderr, "%d\n", lineNo);*/
		if (nTokens > 0) {


			executeStatus = execFullCommandLine(ofp, tokens, nTokens, verbosity);

			if (executeStatus < 0) {
				fprintf(stderr, "Failure executing '%s' line %d:\n    ",
						filename, lineNo);
				fprintfTokens(stderr, tokens, 1);
				return executeStatus;
			}
		}
		prompt(pfp, ifp);
	}

	return (0);
}


/**
 * Open a file and run it as a script
 */
int
runScriptFile(FILE *ofp, FILE *pfp, const char *filename, int verbosity)
{
	FILE *ifp;
	int status;

	ifp = fopen(filename, "r");
	if (ifp == NULL) {
		fprintf(stderr, "Cannot open input script '%s' : %s\n",
				filename, strerror(errno));
		return -1;
	}

	status = runScript(ofp, pfp, ifp, filename, verbosity);
	fclose(ifp);
	return status;
}
