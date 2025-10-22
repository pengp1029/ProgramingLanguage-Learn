#include<unistd.h>
#include<stdio.h>

//int pipe(int pipefd[2]);

int main(){
    int fd[2];
    if(pipe(fd) < 0)
        printf("pipe_error\n");
    else
        printf("pipe_success\n");
    pid_t pid = fork();
    if(pid < 0)
        printf("fork_error\n");
    else if(pid == 0){
        printf("child_process\n");
        close(fd[0]);
        write(fd[1], "hello\n", 6);
        close(fd[1]);
    }
    else{
        printf("parent_process\n");
        close(fd[1]);
        char buf[1024];
        read(fd[0], buf, 1024);
        printf("buf %s", buf);
        close(fd[0]);
    }
    return 0;
}