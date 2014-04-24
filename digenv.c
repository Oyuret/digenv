/**
 * Enkel implementation av digenv. Rättfram imperativ variant. Skapar 3 pipes och forkar
 * till de nödvändiga processer.
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

int main(int argc, char** argv, char** envp){

    /*The id's where we will store the different threads*/
    int sortid, lessid, grepid, envpid;
    sortid=lessid=grepid=envpid=-1;


    /* Create pipes */
    if (pipe(pipeEnvToGrep) == -1){
        err("pipe failed (Env -> Grep)");
    }
    if (pipe(pipeGrepToSort) == -1){
        err("pipe failed (Grep -> Sort)");
    }
    if (pipe(pipeSortToLess) == -1){
        err("pipe failed (Sort -> Less)");
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
        if (dup2(pipeEnvToGrep[WRITE],STDOUT_FILENO)== -1){
                  err("dup2 failed (Env -> Grep write)");
            }

            /*Close the pipes not used*/
            closePipe(pipeEnvToGrep);
            closePipe(pipeGrepToSort);
            closePipe(pipeSortToLess);

            /*Execute printenv, call it with only itself as parameter*/
            if (execlp("printenv", "printenv", NULL) == -1){
                  err("execlp printenv failed miserably");
            }

    } else {

        /* Vi är main */

        grepid = fork();

        /*Did we manage to fork?*/
        if (grepid == -1) {
            err("fork till grep misslyckades");
        }


        if (grepid == 0) {
            /* Vi är i child, dvs grep */

            /*Let grep read from the pipe coming from printenv*/
            if (dup2(pipeEnvToGrep[READ],STDIN_FILENO) == -1) {
                    err( "dup2 failed (Env -> Grep read)" );
              }

            /*Let Grep write to the pipe going to sort*/
            if (dup2(pipeGrepToSort[WRITE], STDOUT_FILENO) == -1) {
                    err("dup2 failed (Grep -> Sort write)");
              }

            /*Close the pipes we are not using*/
              closePipe(pipeSortToLess);
              closePipe(pipeGrepToSort);
              closePipe(pipeEnvToGrep);

            /*We need a parameter list to send to grep*/
            char * grepcommand[argc+1];

            /*Add grep as the first parameter*/
            grepcommand[0] = "grep";

            /*Copy the rest of the parameters*/
            int i;
            for(i=1; i<argc; i++) {
                grepcommand[i] = argv[i];
            }

            /*NULL terminate it*/
            grepcommand[argc] = NULL;


            /*Check if we actually got any parameters from the digenv call*/
            if (argc > 1) {

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
            /* Vi är i main/parent */

            sortid = fork();

            /*Did we manage to fork?*/
            if (sortid == -1) {
                err("failed to fork to sort");
            }

            if (sortid == 0) {

                /* Vi är i child, dvs sort */

                /*Let sort read from the pipe coming from Grep*/
                if (dup2(pipeGrepToSort[READ],STDIN_FILENO) == -1){
                    err( "dup2 failed (Grep -> Sort read)" );
                }

                /*Let sort write to the pipe going to less*/
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
                /* Vi är i parent */
                lessid = fork();

                /*Did we manage to fork?*/
                if (lessid == -1) {
                    err("failed to fork to less");
                }

                if (lessid == 0) {
                    /* Vi är i child, dvs less */


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
                    /* Vi är i parent */

                    /* Vi behöver inte göra något */
                }


            }


        }


    }

    /*Close the pipes not used*/
    closePipe(pipeSortToLess);
    closePipe(pipeGrepToSort);
    closePipe(pipeEnvToGrep);

    /*Wait for less to be done*/
    waitpid(lessid, NULL, 0);

    return 0;
}
