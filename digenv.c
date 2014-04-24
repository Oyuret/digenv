/**
 * NAME:
 *   digenv - for browsing environment variables
 * 
 * SYNTAX:
 *   digenv [PARAMETERS]
 *
 * DESCRIPTION:
 *  The digenv program is a way of listing environment variables.
 *  It works as an alias of 
 *       printenv | grep PARAMETERS | sort | less
 *  If no parameters are given it works as an alias of
 *       printenv | sort | less
 *
 * EXAMPLES:
 *   digenv
 *   digenv PATH
 *   digenv P.+T 
 * 
 * AUTHOR:
 *   Johan Storby, Yuri Stange, 2014
 *
 * SEE ALSO:
 * printenv(1), grep(1), sort(1), less(1)
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define READ 0
#define WRITE 1


/*The pipes used*/
int pipeEnvToGrep[2], pipeGrepToSort[2], pipeSortToLess[2];

/*Print out the possible error*/
void err(char* msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

/*Close the given pipe*/
void closePipe(int pipe[2]){

    /*Attempt to close the READ end of the pipe*/
    if (close(pipe[READ]) == -1)
        err("error when closing pipe, read");

    /*Attempt to close the WRITE end of the pipe*/
    if (close(pipe[WRITE]) == -1)
        err("error when closing pipe, write");
}

/*Main function*/
int main(int argc, char** argv, char** envp){

    /*The id's where we will store the different threads*/
    int sortid, lessid, grepid, envpid;
    
    /*Initialize the varibales to avoid warnings*/
    sortid=lessid=grepid=envpid=-1;


    /* Create pipes */
    if (pipe(pipeEnvToGrep) == -1){
        err("pipe create failed (Env -> Grep)");
    }
    if (pipe(pipeGrepToSort) == -1){
        err("pipe create failed (Grep -> Sort)");
    }
    if (pipe(pipeSortToLess) == -1){
        err("pipe create failed (Sort -> Less)");
    }

    /* Fork to the envp process */
    envpid = fork();

    /* Were we successful? */
    if(envpid==-1) {
        err("fork to envp failed");
    }

    /* Main or envp? */
    if(envpid == 0) {

        /* We are the child, envp */
        
        /* Set the output to be piped to the write end of pipeEnvToGrep
         rather than stdout */
        if (dup2(pipeEnvToGrep[WRITE],STDOUT_FILENO)== -1){
              err("dup2 failed (Env -> Grep write)");
        }

        /*Close the pipes not used anymore*/
        closePipe(pipeEnvToGrep);
        closePipe(pipeGrepToSort);
        closePipe(pipeSortToLess);

        /*Execute printenv, call it with only itself as parameter*/
        if (execlp("printenv", "printenv", NULL) == -1){
             err("execlp printenv failed miserably");
        }

    } else {

        /* We are in the main process */

		  /*Fork to grepid*/
        grepid = fork();

        /*Did we manage to fork to grepid?*/
        if (grepid == -1) {
            err("fork to grep failed");
        }


        if (grepid == 0) {
            /* We are the child, grepid */

            /*Let grep read from the pipe coming from printenv*/
            if (dup2(pipeEnvToGrep[READ],STDIN_FILENO) == -1) {
                err( "dup2 failed (Env -> Grep read)" );
             }

            /*Let grep write to the pipe going to sort rather than stdout*/
            if (dup2(pipeGrepToSort[WRITE], STDOUT_FILENO) == -1) {
                 err("dup2 failed (Grep -> Sort write)");
            }

            /*Close the pipes we are not using*/
            closePipe(pipeSortToLess);
            closePipe(pipeGrepToSort);
            closePipe(pipeEnvToGrep);


            /*Check if we actually got any parameters from the digenv call*/
            if (argc > 1) {
            
            	/*We need a parameter list to send to grep*/
            	char * grepcommand[argc+1];

            	/*Add grep as the first parameter*/
            	grepcommand[0] = "grep";

            	/*Copy the rest of the parameters*/
            	int i;
            	for(i=1; i<argc; i++) {
               	grepcommand[i] = argv[i];
            	}

            	/*NULL at the end to terminate it*/
            	grepcommand[argc] = NULL;

               /*We have parameters, call grep with those*/
               if (execvp("grep", grepcommand) == -1) {
                  err("exevp went wrong (grep)");
               }

           } else {

                /*No parameters to send. Just let grep grab everything*/
                if (execlp("grep", "grep", ".*", NULL) == -1) {
                    err("execlp grep .* went wrong");
                }

           }



        } else {
            /* We are main/parent */

				/*Fork to sort*/
            sortid = fork();

            /*Did we manage to fork?*/
            if (sortid == -1) {
                err("failed to fork to sort");
            }

            if (sortid == 0) {
                /* We are the child, sort */

                /*Let sort read from the pipe coming from grep instead of stdin*/
                if (dup2(pipeGrepToSort[READ],STDIN_FILENO) == -1){
                    err( "dup2 failed (Grep -> Sort read)" );
                }

                /*Let sort write to the pipe going to less instead of stdout*/
                if (dup2(pipeSortToLess[WRITE], STDOUT_FILENO) == -1){
                    err("dup2 failed (Sort -> Less write)");
                }

                /*Close the unnecesary pipes*/
                closePipe(pipeEnvToGrep);
                closePipe(pipeGrepToSort);
                closePipe(pipeSortToLess);

                /*Call sort with just itself as a parameter*/
                if (execlp("sort", "sort", NULL) == -1){
                    err("execlp sort went wrong");
                }

            } else {
                /* We are the parent, fork to less */
                lessid = fork();

                /*Did we manage to fork?*/
                if (lessid == -1) {
                    err("failed to fork to less");
                }

                if (lessid == 0) {
                    /* We are the child, less */

                        /*Let Less read from the pipe coming from sort*/
                        if (dup2(pipeSortToLess[READ], STDIN_FILENO) == -1){
                            err("dup2 failed (Sort -> Less read)");
                        }

                        /*Close the unnecesary pipes*/
                        closePipe(pipeSortToLess);
                        closePipe(pipeGrepToSort);
                        closePipe(pipeEnvToGrep);


                        /*Call less with just itself as parameter*/
                        if (execlp("less", "less", NULL) == -1){
                            err("execlp less failed miserably");
                        }

                } else {
                    /* We are the parent */

                    /* We don't need to do anything */
                }
            }
        }
    }

    /*Close all the pipes now*/
    closePipe(pipeSortToLess);
    closePipe(pipeGrepToSort);
    closePipe(pipeEnvToGrep);

    /*Wait for less to be done, so we don't close prematurely*/
    waitpid(lessid, NULL, 0);

    return 0;
}
