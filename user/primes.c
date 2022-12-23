#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char *argv[]) {
    int p[2];
    pipe(p);
    for (int i = 2; i <= 35; i++) {
        write(p[1],(void*)&i,4);
    }
    close(p[1]);
    if (fork() == 0) {
        int *temp = 0;
        int prime = read(p[0],temp,4);
        if (prime == 0) {
            exit(0);
        }
        printf("prime %d\n",*temp);
        int n = read(p[0],temp,4);
        if (n == 0) {
            exit(0);
        }
        
    }
    wait(0);
    close(p[0]);

    exit(0);
}