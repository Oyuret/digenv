#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define READ 0
#define WRITE 1


int pipe1[2], pipe2[2], pipe3[2], pipe4[2];

void err(char* msg){
	perror(msg);
	exit(EXIT_FAILURE);
}

void closePipe(int pipeEnd[2]){
	if (close(pipeEnd[READ]) == -1)
		err("error when closing pipe, read");

	if (close(pipeEnd[WRITE]) == -1)
		err("error when closing pipe, write");
}

int main(int argc, char** argv, char** envp){

	int sortid, lessid, grepid, envpid;


	/* Create pipes */
	if (pipe(pipe1) == -1){
		err("pipe1");
	}
	if (pipe(pipe2) == -1){
		err("pipe2");
	}
	if (pipe(pipe3) == -1){
		err("pipe3");
	}
	if (pipe(pipe4) == -1){
		err("pipe4");
	}

	/* forka till envp */
	envpid = fork();
	
	/* kolla om vi lyckades */
	if(envpid==-1) {
		err("fork till envp misslyckades");
	}

	/* Main eller envp */
	if(envpid == 0) {
		/* vi är child, dvs envp */
		if (dup2(pipe1[WRITE],STDOUT_FILENO)== -1){
      			err("dup2 miss envp skickar vidare");
    		}

    		closePipe(pipe4);
    		closePipe(pipe3);
    		closePipe(pipe2);
    		closePipe(pipe1);

    		if (execlp("printenv", "printenv", NULL) == -1){
      			err("execlp printenv does not like you!");
    		}
	} else {
		/* Vi är main */
		grepid = fork();
		if (grepid == -1) {
			err("fork till grep misslyckades");
		}


		if (grepid == 0) {
			/* Vi är i child, dvs grep */

			if (dup2(pipe1[READ],STDIN_FILENO) == -1){
    				err( "dup2 p1 read" );
      			}
      			if (dup2(pipe2[WRITE], STDOUT_FILENO) == -1){
    				err("dup2 p2 write");
      			}

      			closePipe(pipe4);
      			closePipe(pipe3);
      			closePipe(pipe2);
      			closePipe(pipe1);
	
			char * grepcommand[argc];
			grepcommand[0] = "grep";
			int i;
			for(i=1; i<argc; i++) {
				grepcommand[i] = argv[i];
			}		
			

			//TODO Fixa!
			if (argc > 1) {
				if (execlp("grep", *grepcommand, NULL) == -1){
    					err("execlp grep does not like you!");
      				}
			} else {
				if (execlp("grep", "grep", ".*", NULL) == -1) {
					err("execlp echo went wrong");
				}
			}

      			

		} else {
			/* Vi är i main/parent */

			sortid = fork();

			if (sortid == -1) {
				err("sortid vill inte");
			}

			if (sortid == 0) {
				/* Vi är i child, dvs sort */
				if (dup2(pipe2[READ],STDIN_FILENO) == -1){
					err( "dup2 p2 read" );
				}
				
				if (dup2(pipe3[WRITE], STDOUT_FILENO) == -1){
					err("dup2 p3 write");
				}

				closePipe(pipe4);
				closePipe(pipe3);
				closePipe(pipe2);
				closePipe(pipe1);

				if (execlp("sort", "sort", NULL) == -1){
					err("execlp sort does not like you!");
				}

			} else {
				/* Vi är i parent */
				lessid = fork();

				if (lessid == -1) {
					err("lessid blev fel");
				}

				if (lessid == 0) {
					/* Vi är i child, dvs less */


					    if (dup2(pipe3[READ], STDIN_FILENO) == -1){
					    	err("err dup2 read");
					    }

					    closePipe(pipe4);
					    closePipe(pipe3);
					    closePipe(pipe2);
					    closePipe(pipe1);


					    if (execlp("less", "less", NULL) == -1){
					    	err("err in exelp pager");
					    }

				} else {
					/* Vi är i parent */

					/* Vi behöver inte göra något */
				}


			}


		}
		
		
	}
	
	  


  	closePipe(pipe1);
  	closePipe(pipe2);
  	closePipe(pipe3);
  	closePipe(pipe4);
	waitpid(lessid, NULL, 0);

	return 0;
}
