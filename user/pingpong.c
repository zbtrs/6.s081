#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char *argv[]) {
    int p1[2],p2[2];
    pipe(p1);
    pipe(p2);
    // p1 -- parent to child; p2 -- child to parent
    if (fork() == 0) {
        // child process
        close(p1[1]);
        close(p2[0]);
        char buf[10];
        while (1) {
            if (read(p1[0],(void*)buf,1) > 0) {
                printf("%d: received ping\n",getpid());
                write(p2[1],(void*)"a",1);
                exit(0);
            }
        }
    }
    char buf[10];
    close(p1[0]);
    close(p2[1]);
    write(p1[1],(void*)"a",1);
    while (1) {
        if (read(p2[0],(void*)buf,1) > 0) {
            printf("%d: received pong\n",getpid());
            break;
        }
    }    

    exit(0);
}