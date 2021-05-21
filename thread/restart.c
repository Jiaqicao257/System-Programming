//author: jiaqic7
//a program to restart the input executable whenaver a segfault occurs
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

pid_t child_process = -1;
void killall() {
    //control c: both program exit
    if (child_process == -1) return;
    kill(child_process,SIGKILL);
    exit(1);
}
int main(int argc, char* argv[]){
    signal(SIGINT, killall);
    //search PATH env to get the executable, if the executable cannot be found, return
    char* path = argv[1];
    char prefix[100]= "";
    //add ./ to executable
    strcpy(prefix, "./");
    strcat(prefix, path);
    path = prefix;
    //check if path is valid
    if(access(path, X_OK) == -1) {
        printf("not found\n"); 
        return 0;
    }
    while (1) {
        //fork
        pid_t pid = fork();
        if(pid == -1){ //failed
            fprintf(stderr, "Fork Failed\n");
            exit(1);
        } else if(pid > 0) { //parent
            //wait
            child_process = pid;
            int status;
            pid_t result = waitpid(pid, &status, 0);
            if(result == -1) {
                fprintf(stderr, "Wait Failed");
                exit(1);
            }      
            if (WIFSIGNALED(status) != 0 && WTERMSIG(status) == SIGSEGV) { //restrat
                pid = fork();
                printf("restart\n");
            } else if (WIFSIGNALED(status) != 0) {
                killall();
                exit(1);
            } else {
                return 0;
            }
        } else { //child
            execvp(path, argv+1);
            fprintf(stderr, "Exec Failed");
            exit(1);
        } 
    }
    
    return 0;
}