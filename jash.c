#include <bits/stdc++.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>

using namespace std;

#define MAXLINE 1000
#define DEBUG 0

/* Function declarations and globals */
int parent_pid, status;
char ** tokenize(char*) ;
char** tokens;
char** tokens2;
int execute_command(char** tokens) ;
void hdl_par(int signum);
void hdl_child(int signum);
void parse(char **tokens, vector<char**> &commands);
void clean();
bool bgd=false;
vector<int> pid_list;
vector<int> bgd_list;
int g_cronindex = 0;

void setNext(int mm, int hh, int &nextmm, int &nexthh) {
	time_t t = time(0);
	struct tm * now = localtime(&t);
	if (mm == -1) {
		if (hh == -1) {
			if (now->tm_min == 59) {
				nextmm = 0;
				nexthh = (now->tm_hour+1) % 24;
			}
			else {
				nextmm = now->tm_min+1;
				nexthh = now->tm_hour;
			}
		} else {
			if (now->tm_hour != hh) {
				nexthh = hh;
				nextmm = 0;
			} else {
				nexthh = hh;
				nextmm = (now->tm_min+1) % 60;
			}
		}
	}
	else {
		if (hh == -1) {
			if (now->tm_min < mm) {
				nextmm = mm;
				nexthh = now->tm_hour;
			} else {
				nextmm = mm;
				nexthh = (now->tm_hour+1) % 24;
			}
		} else {
			nextmm = mm;
			nexthh = hh;
		}
	}
}

int timediff(int m1, int h1) {
	time_t t = time(0);
	struct tm * now = localtime(&t);
	int curmm = now->tm_min, curhh = now->tm_hour;
	int ret = 0;
	
	// printf("curmm = %i\n", curmm);
	// printf("curhh = %i\n", curhh);

	if (m1 == curmm && h1 == curhh) {
		if (curmm == 59) {
			curmm = 0;
			curhh = (curhh+1)%24;
		}
		else {
			curmm++;
		}
	}

	// printf("curmm = %i\n", curmm);
	// printf("curhh = %i\n", curhh);
	// printf("m1 = %i\n", m1);
	// printf("h1 = %i\n", h1);
	while((m1 != curmm) || (h1 != curhh)) {
		ret++;
		if (curmm == 59) {
			curmm = 0;
			curhh = (curhh+1)%24;
		}
		else {
			curmm++;
		}
	}
	return ret;
}

struct cronjob {
	int mm, hh;
	char command[MAXLINE];
	// string command;
	int nextmm, nexthh;
	int sleepfor; // in minutes
	int cronindex;
	cronjob (int m, int h, char c[]) {
		mm = m;
		hh = h;
		strcpy(command, c);
		cronindex = ++g_cronindex;
		setNext(mm, hh, nextmm, nexthh);
		sleepfor = timediff(nextmm, nexthh);
	}
};

bool operator<(cronjob c1, cronjob c2) {
	if (c1.sleepfor == c2.sleepfor) {
		return c1.cronindex < c2.cronindex;
	}
	return c1.sleepfor < c2.sleepfor;
}

int checkIORedirect(char **tokens, int pos[3]) {
	int i = 0;
	pos[0] = pos[1] = pos[2] = -1;
	int incount = 0, outcount = 0;

	while(tokens[i] != NULL) {
		if (!strcmp(tokens[i], "<")) {
			incount ++;
			pos[0] = i;
			// tokens[i] = NULL;

		}
		if (!strcmp(tokens[i], ">")) {
			outcount++;
			pos[1] = i;
			// tokens[i] = NULL;
		}
		if (!strcmp(tokens[i], ">>")) {
			outcount++;
			pos[2] = i;
			// tokens[i] = NULL;
		}
		i++;
	}
	if (outcount > 1 || incount > 1)
		return -1;
	else
		return 0;
}

void findPipes(char **tokens, vector<char**> &commands) {
	/*findPipes will store the "tokens" split at the symbol '|' into the vector commands */
	char temp[MAXLINE]; 
	temp[0] = '\0'; 
	char **tt;
	for(int i = 0; tokens[i] != NULL; i++) {

		if (strcmp(tokens[i], "|")) {
			/*If cuurent word is NOT |, current word is part of current command*/
			strcat(temp, " ");
			strcat(temp, tokens[i]);		

		} else {
			/*If cuurent word is |, push the current command into vector*/
			int s1 = strlen(temp);
			temp[s1] = '\n';
			temp[s1+1] = '\0';
			tt = tokenize(temp);
			commands.push_back(tt);
			temp[0] = '\0';
		}
	}
	int s1 = strlen(temp);
	temp[s1] = '\n';
	temp[s1+1] = '\0';
	tt = tokenize(temp);
	commands.push_back(tt);
}

void parse(char **tokens, vector<char**> &commands) {
	/*parse input in case of parallel/sequential etc..*/

	char temp[MAXLINE];
	temp[0] = '\0';
	char **tt;
	for(int i = 1; tokens[i] != NULL; i++) {

		if (strcmp(tokens[i], ":::")) {
			/*If cuurent word is NOT :::, current word is part of current command*/
			strcat(temp, " ");
			strcat(temp, tokens[i]);		

		} else {
			/*If cuurent word is :::, push the current command into vector*/
			int s1 = strlen(temp);
			temp[s1] = '\n';
			temp[s1+1] = '\0';
			tt = tokenize(temp);
			commands.push_back(tt);
			temp[0] = '\0';
		}
	}
	int s1 = strlen(temp);
	temp[s1] = '\n';
	temp[s1+1] = '\0';
	tt = tokenize(temp);
	commands.push_back(tt);
}

void hdl_par(int signum)
{
	/*Signal handler for parent process.*/
	int z = 0;
	if(signum == SIGCHLD)
	{
		int pid_catch=waitpid(-1,&status,WNOHANG);
	//	cout<<"Status "<<status<<endl;
		
	//	bgd_list.erase(std::remove(bgd_list.begin(), bgd_list.end(), pid_catch), bgd_list.end());
		fflush(stdout);
	}
	if(signum == SIGQUIT || signum == SIGINT)
	{
		//cout<<"debug"<<endl;
		for(int i = 0; i < pid_list.size(); i++) {
			/*Kill child processes.*/
			z++;
			//cout<<pid_list[i]<<endl;
			kill(SIGINT,pid_list[i]);
		}

		pid_list.clear();

		/*If there are no children, do nothing.*/
		if(z == 0)
			printf("\n$ ");
		fflush(stdout);
	}
}

void hdl_child(int signum)
{
	/*Signal handler for child process.*/
	if(signum == SIGINT || signum == SIGQUIT)
	{
			clean();
			fflush(stdout);	
			exit(0);
	}

}
void hdl_child2(int signum)
{
	
	/*Signal handler for background_child process.*/
	if(signum == SIGINT || signum == SIGQUIT)
	{
		cout<<"Background Process with PID halted "<<getpid()<<endl;
		int j=0;
		cout<<"Command :";
        while(tokens2[j] != NULL) {
              printf("%s ", tokens2[j]);
              j++;
        }
		cout<<"Status "<<"130"<<endl;
		clean();
		fflush(stdout);	
		exit(0);
	}

}
void clean() {
///prevents memory leaks by freeing the allocated memory
	int i ;
	for(i = 0; tokens[i] != NULL; i++){
		free(tokens[i]);
	}
	free(tokens);
		for(i = 0; tokens2[i] != NULL; i++){
		free(tokens2[i]);
	}
	free(tokens2);
}

int main(int argc, char** argv){
	parent_pid = getpid() ;
	
	/* Set (and define) appropriate signal handlers */
	/* Exact signals and handlers will depend on your implementation */
	// signal(SIGINT, quit);
	// signal(SIGTERM, quit);

	char input[MAXLINE];
	
	signal (SIGINT, hdl_par);
	signal (SIGQUIT, hdl_par);
	signal (SIGCHLD, hdl_par);
	
	
	while(1) { 
		printf("$ "); // The prompt
		fflush(stdin);

		char *in = fgets(input, MAXLINE, stdin); // Taking input one line at a time
		//Checking for EOF
		if (in == NULL){
			if (DEBUG) printf("jash: EOF found. Program will exit.\n");
			break ;
		}

		// Calling the tokenizer function on the input line
		tokens = tokenize(input);	
		// Executing the command parsed by the tokenizer
		execute_command(tokens);
		if (!pid_list.empty()){
			waitpid(pid_list[0], &status, 0);
		}
		pid_list.clear();

		// Freeing the allocated memory	
		int i ;
		for(i = 0; tokens[i] != NULL; i++){
			free(tokens[i]);
		}
		free(tokens);
	}
	
	/* Kill all Children Processes and Quit Parent Process */
	return 0 ;
}

/*The tokenizer function takes a string of chars and forms tokens out of it*/
char ** tokenize(char* input){
	int i, doubleQuotes = 0, tokenIndex = 0, tokenNo = 0, colons = 0 ;
	char *token = (char *)malloc(MAXLINE*sizeof(char));

	tokens = (char **) malloc(MAXLINE*sizeof(char*));

	for(i = 0; i < strlen(input); i++){
		char readChar = input[i];

		if (readChar == '"'){
			doubleQuotes = (doubleQuotes + 1) % 2;
			if (doubleQuotes == 0){
				token[tokenIndex] = '\0';
				if (tokenIndex != 0){
					tokens[tokenNo] = (char*)malloc(MAXLINE*sizeof(char));
					strcpy(tokens[tokenNo++], token);
					tokenIndex = 0; 
				}
			}
		} else if (doubleQuotes == 1){
			token[tokenIndex++] = readChar;
		} else if (readChar == ' ' || readChar == '\n' || readChar == '\t'){
			token[tokenIndex] = '\0';
			if (tokenIndex != 0){
				tokens[tokenNo] = (char*)malloc(MAXLINE*sizeof(char));
				strcpy(tokens[tokenNo++], token);
				tokenIndex = 0; 
			}
		} 
		else {
			token[tokenIndex++] = readChar;
		}
	}

	if (doubleQuotes == 1){
		token[tokenIndex] = '\0';
		if (tokenIndex != 0){
			tokens[tokenNo] = (char*)malloc(MAXLINE*sizeof(char));
			strcpy(tokens[tokenNo++], token);
		}
	}

	tokens[tokenNo] = NULL ;

	free(token);

	return tokens;
}

int execute_command(char** tokens) {
	/* 
	Takes the tokens from the parser and based on the type of command 
	and proceeds to perform the necessary actions. 
	Returns 0 on success, -1 on failure. 
	*/
	int last;
	for(int i=0;i>-1;i++)
	{
		if(tokens[i]==NULL){last=i-1; break;}
	}
	if (tokens == NULL) {

		return -1 ; 				// Null Command
	} else if (tokens[0] == NULL) {
		return -1 ;					// Empty Command
	} else if (!strcmp(tokens[0], "cron")) {
		/*For the cron command*/
		int pid = fork();
		if (pid == 0) 
		{
			prctl(PR_SET_PDEATHSIG,SIGINT);
			setpgid(getpid(),getpid());
			//bgd=true;
			signal (SIGINT, hdl_child2);
			signal (SIGQUIT, hdl_child2);

			set<cronjob> jobs;

			if (FILE *file = fopen(tokens[1], "r")) {

				char line[MAXLINE];
				char *in;
				char **crtokens;

				/*Read lines from the opened file and schedule*/
				while(in = fgets(line, MAXLINE, file)) {
					crtokens = tokenize(line);
					int mm, hh;
					if (!strcmp(crtokens[0], "*")) {
						mm = -1;
					} else {
						mm = atoi(crtokens[0]);
					}
					if (!strcmp(crtokens[1], "*")) {
						hh = -1;
					} else {
						hh = atoi(crtokens[1]);
					}
					char command[MAXLINE];
					memset(command, '\0', MAXLINE);

					for(int i = 2; crtokens[i] != NULL; i++) {
						strcat(command, " ");
						strcat(command, crtokens[i]);		
					}
					strcat(command, " &");
					int s1 = strlen(command);
					command[s1] = '\n';
					command[s1+1] = '\0';
					
					cronjob t(mm, hh, command);
					jobs.insert(t);
					// printf("inserted command: %s\n", command);
					// printf("next time: %i\n", t.nextmm);
					// printf("sleepfor: %i\n", t.sleepfor);
				}
				int length = 0;
				while(tokens[length]!=NULL) length++;

				tokens2 = (char**)malloc(length*sizeof(char*));

				for ( int i = 0; i < length; ++i ){
    //width must be known (see below)
					int width = strlen(tokens[i]) + 1;
					tokens2[i] = (char*)malloc(width);

					memcpy(tokens2[i], tokens[i], width);
				}
				
				while(true) {
					set<cronjob>::iterator it = jobs.begin();


					int sl = 60*it->sleepfor;
					char slstr[MAXLINE];
					sprintf(slstr, "%d", sl);
					char slstr2[80];
					strcpy(slstr2, "sleep ");
					strcat(slstr2, slstr);
					strcat(slstr2, "\n");


					execute_command(tokenize(slstr2));
					waitpid(pid_list[0], &status, 0);
					pid_list.clear();

					char line[MAXLINE];
					strcpy(line, it->command);
					
					execute_command(tokenize(line));
					cronjob t(it->mm, it->hh, line);
					jobs.erase(it);
					vector<cronjob> nextcrons;
					nextcrons.push_back(t);
					for(it = jobs.begin(); it != jobs.end(); it++) {
						time_t t = time(0);
						struct tm * now = localtime(&t);
						if (now->tm_hour == it->nexthh && now->tm_min == it->nextmm) {
							char line[MAXLINE];
							strcpy(line, it->command);

							execute_command(tokenize(line));
							cronjob t(it->mm, it->hh, line);
							jobs.erase(it);
							nextcrons.push_back(t);
						}
					}
					for(int i = 0; i < nextcrons.size(); i++) {
						jobs.insert(nextcrons[i]);	
					}
					

				}

				/*free memory before exiting*/
				clean();
				_exit(0);
			}
			else{
				/*If there is error opening the file*/
				perror(tokens[1]);
				fflush(stderr);
				clean();
				exit(-1);
			}
		}

	}
	else if(!strcmp(tokens[last],"&"))
	{
		///for background processing
		tokens[last]=NULL;
	//	token2=tokens;
		int pid=fork();
		if(pid==0)
		{
			prctl(PR_SET_PDEATHSIG,SIGINT);
			setpgid(getpid(),getpid());
			//bgd=true;
			signal (SIGINT, hdl_child2);
			signal (SIGQUIT, hdl_child2);
			int length = 0;
			while(tokens[length]!=NULL) length++;

			tokens2 = (char**)malloc(length*sizeof(char*));

			for ( int i = 0; i < length; ++i ){
    //width must be known (see below)
				int width = strlen(tokens[i]) + 1;
				tokens2[i] = (char*)malloc(width);

				memcpy(tokens2[i], tokens[i], width);
			}

			int ret=0;
			execute_command(tokens);
			for(int j = 0; j < pid_list.size(); j++) {
	                                /*wait for all children to exit, before going to START state*/
			waitpid(pid_list[j], &status, 0);
			if(status!=0) {ret=status;break;}
			}
			pid_list.clear();
			cout<<"Background Process with PID completed "<<getpid()<<endl;
			int j=0;
			cout<<"Command :";
			while(tokens2[j] != NULL) {
              printf("%s ", tokens2[j]);
              j++;
			}
			cout<<endl<<"Status "<< ret <<endl;
			printf("\n$ "); 
			clean();
			fflush(stdout);
			_exit(ret);
		}
		return 0;
	//	bgd_list.push_back(pid);
	} 
	 else if (!strcmp(tokens[0],"exit")) {
	 	// code for exit
		clean();
		exit(0);
	} else if (!strcmp(tokens[0],"cd")) {
		/* Change Directory, or print error on failure */


		if (tokens[1] == NULL) {
			/*if cd is given without any argument, cd to $HOME*/
			errno = 0;
			chdir(getenv("HOME"));
			if (errno != 0) {
				perror("");
				return errno;
			}
			return 0;
		}

		/*If the path starts with ~, use $HOME environment variable.*/
		if (tokens[1][0] == '~') {
			char temp[1000];
			memcpy(temp, &tokens[1][1], 998);
			strcpy(tokens[1], getenv("HOME"));
			strcat(tokens[1], temp);
		}

		errno = 0;
		chdir(tokens[1]);
		
		if (errno != 0) 
			perror("");
		
		/*In DEGBUG mode, print the cwd after doing chdir.*/
		if (DEBUG) {
			char out[80];
			getcwd(out, 80);
			printf("cwd: %s\n", out);
		}
		return errno;

	} else if (!strcmp(tokens[0],"parallel")) {
		/* Analyze the command to get the jobs */
		/* Run jobs in parallel, or print error on failure */

		/*Parse the input*/
		vector<char**> commands;
		parse(tokens, commands);
		

		for(int i = 0; i < commands.size() && commands[i][0] != NULL; i++) {

			if (!strcmp(commands[i][0], "cd") || !strcmp(commands[i][0], "exit")) {
				/*if command is cd or exit, fork a process*/
				int pid = fork(); 
				if (pid == 0) {
					prctl(PR_SET_PDEATHSIG,SIGINT);
				//	bgd=false;
					signal (SIGINT, hdl_child);
					signal (SIGQUIT, hdl_child);
					/*exit with the return value of execute_command*/
					exit(execute_command(commands[i]));
				}
				/*push this forked process's pid to pid_list*/
				pid_list.push_back(pid);
			}
			else{
				/*just execute the command! (no waitpid here)*/
				execute_command(commands[i]);
			}

		}
		for(int i = 0; i < pid_list.size(); i++) {
			/*wait for all children to exit, before going to START state*/
			waitpid(pid_list[i], &status, 0);
		}
		pid_list.clear();

		return 0 ;

	} else if (!strcmp(tokens[0],"sequential")) {
		/* Analyze the command to get the jobs */
		/* Run jobs sequentially, print error on failure */
		/* Stop on failure or if all jobs are completed */
		
		/*parse input*/
		vector<char**> commands;
		parse(tokens, commands);

		/*execute commands one by one*/
		for(int i = 0; i < commands.size(); i++) {
			int err = execute_command(commands[i]);

			if(!pid_list.empty())
			{
				/*wait for the last executed command*/	
				waitpid(pid_list[0],&status,0);
				pid_list.clear();

				/*if the last executed command didn't finish successfuly, break execution*/
				if(status != 0) return status;
			}
			else if(err!=0)
			{
				/*if the last executed command didn't finish successfuly, break execution*/
				return err;
			}
			
		}


		return 0 ;					// Return value accordingly
	} else if (!strcmp(tokens[0],"sequential_or")) {
		/* Analyze the command to get the jobs */
		/* Run jobs sequentially, print error on failure */
		/* Stop on failure or if all jobs are completed */
		
		/*parse input*/
		vector<char**> commands;
		parse(tokens, commands);

		for(int i = 0; i < commands.size(); i++) {
			int err = execute_command(commands[i]);
			if(!pid_list.empty())
			{	
				/*wait for the last executed command*/	
				waitpid(pid_list[0],&status,0);
				pid_list.clear();

				/*if the last executed command finished successfuly, break execution*/
				if(status ==0) return status;
			}
			else if(err==0)
			{
				/*if the last executed command finished successfuly, break execution*/
				return 0;
			}
			
		}


		return -1 ;	// Return value accordingly
	} else {
		/* Either file is to be executed or batch file to be run */
		/* Child process creation (needed for both cases) */
		int pos[3], fd[2];

		/*check if the input line has any >, < or >>. Give error if there is invalid number of these.*/
		int ret = checkIORedirect(tokens, pos);
		if (ret == -1) {
			fprintf(stderr, "Invalid I/O redirection\n");
			return -1;
		}


		int pid = fork();
		
		if (pid < 0) {
			/*if error in forkin'*/
			perror("");
			fflush(stderr);
			return -1;
		}
		else if (pid == 0) {
		//	bgd=false;
			prctl(PR_SET_PDEATHSIG,SIGINT);
			/*Handling pipes*/

			/*findPipes will store the "tokens" split at the symbol '|' into pipe_vec */
			vector<char **> pipe_vec;
			findPipes(tokens, pipe_vec);
			signal (SIGINT, hdl_child);
			signal (SIGQUIT, hdl_child);
			if(pipe_vec.size()>1)
			{/*if there are pipe(s)*/

				vector<int> pid_list2;

				int pc=pipe_vec.size()-1; // Number of '|' symbols
				
				/*we need 'pc' number of pipes*/
				int fd[pc][2];
				for(int i=0;i<pc;i++)
				{
					pipe(fd[i]);
				}

				
				int pid2=fork();
				if (pid2 < 0) {
					/*if error in forkin'*/
					perror("");
					fflush(stderr);
					return -1; 
				}

				if(pid2==0)
				{
					/*Execute the first command before '|' symbol */
					dup2(fd[0][1],1);
					for(int j=0;j<pc;j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					execute_command(pipe_vec[0]);
					
					for(int j = 0; j < pid_list.size(); j++) {
						waitpid(pid_list[j], &status, 0);
						if(status!=0) _exit(-1);

					}
					pid_list.clear();
					clean();
					_exit(0);
				}
				pid_list2.push_back(pid2);

				for(int i=1;i<pc;i++)
				{/*execute all but the last command*/

					pid2=fork();
						if (pid2 < 0) {
						/*if error in forkin'*/
						perror("");
						fflush(stderr);
						return -1; 
					}
					if(pid2==0)
					{
						dup2(fd[i-1][0],0);
						dup2(fd[i][1],1);
						for(int j=0;j<pc;j++)
						{
							close(fd[j][0]);
							close(fd[j][1]);
						}

						execute_command(pipe_vec[i]);
						for(int j = 0; j < pid_list.size(); j++) {
	                                /*wait for all children to exit, before going to START state*/
							waitpid(pid_list[j], &status, 0);
							if(status!=0) _exit(-1);
						}
						pid_list.clear();
						clean();
						_exit(0);
					}
					pid_list2.push_back(pid2);
				}
				pid2=fork();
				if (pid2 < 0) {
						/*if error in forkin'*/
					perror("");
					fflush(stderr);
					return -1; 
				}
				if(pid2==0)
				{/*execute the last command*/
					dup2(fd[pc-1][0],0);
					for(int j=0;j<pc;j++)
					{
						close(fd[j][0]);
						close(fd[j][1]);
					}
					execute_command(pipe_vec[pc]);
					for(int j = 0; j < pid_list.size(); j++) {
		                                /*wait for all children to exit, before going to START state*/
						waitpid(pid_list[j], &status, 0);
						if(status!=0) _exit(-1);
					}
					pid_list.clear();
					clean();
					_exit(0);
				}
				pid_list2.push_back(pid2);
				for(int j=0;j<pc;j++)
				{
					close(fd[j][0]);
					close(fd[j][1]);
				}
				for(int i = 0; i < pid_list2.size(); i++) {
		                                /*wait for all children to exit, before going to START state*/
					waitpid(pid_list2[i], &status, 0);
					if(status!=0) _exit(-1);
				}
				pid_list.clear();
				clean();
				_exit(0);
			}

		/* Handling I/O redirection */
			if (pos[0] != -1 || pos[1] != -1 || pos[2] != -1) {

			if (pos[0] != -1) {
				tokens[pos[0]] = NULL;
				close(fileno(stdin));
				FILE *file = fopen(tokens[pos[0]+1], "r");
				if (file) {
					dup2(fileno(file), 0);
				}
				else {
					perror(tokens[pos[0]+1]);
				}
			}
			if (pos[1] != -1) {
				tokens[pos[1]] = NULL;
				close(fileno(stdout));
				FILE *file = fopen(tokens[pos[1]+1], "w");
				if (file) {
					dup2(fileno(file), 1);
				}
				else {
					perror(tokens[pos[1]+1]);
				}
			}
			if (pos[2] != -1) {
				tokens[pos[2]] = NULL;
				close(fileno(stdout));
				FILE *file = fopen(tokens[pos[2]+1], "a");
				if (file) {
					dup2(fileno(file), 1);
				}
				else {
					perror(tokens[pos[2]+1]);
				}
			}
		}

		if (!strcmp(tokens[0],"run")) {
					/* Locate file and run commands */

			if (FILE *file = fopen(tokens[1], "r")) {

				char line[MAXLINE];
				char *in;
				char **tokens;

						/*Read lines from the opened file and execute*/
				while(in = fgets(line, MAXLINE, file)) {
					tokens = tokenize(line);
					execute_command(tokens);

							/*wait for the last executed command */
					if (!pid_list.empty()){
						waitpid(pid_list[0], &status, 0);
					}

					pid_list.clear();
				}
						/*free memory before exiting*/
				clean();
				_exit(0);
			}
			else{
						/*If there is error opening the file*/
				perror(tokens[1]);
				fflush(stderr);
				clean();
				exit(-1);
			}
					/* Exit child with or without error */
			clean();
			exit (0);
		}
		else {
				/* File Execution */
			errno = 0;



			execvp(tokens[0], tokens);

				/* Print error on failure, exit with error*/
			if (errno != 0) {
						/*if error, print the input word and error description*/
				perror(tokens[0]);
				fflush(stderr);
				clean();
				exit(errno);
			}
				/*free memory before exiting*/
			clean();
			exit(0) ;
		}
	}
	else {
			/* Parent Process */
	pid_list.push_back(pid);

		}
	}
	return 1 ;
}
