#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1
#define INT_LEN 4

int read_first_prime(int p[2],int *fp) {
    if (read(p[RD],fp,INT_LEN) == INT_LEN) {
        printf("prime %d\n",*fp);
        return 0;
    }
    return -1;
}

void transmit(int lp[2],int p[2],int fp) {
    int n;
    while (read(lp[RD], &n, INT_LEN) == INT_LEN) {
        if (n % fp != 0) {
            write(p[WR],&n,INT_LEN);
        }
    }
    close(lp[RD]);
}

void primes(int lp[2]) {
    close(lp[WR]);
    int fp = 0;
    if (0 == read_first_prime(lp,&fp)) {
        int p[2];
        pipe(p);
        transmit(lp,p,fp);
        if (fork() == 0) {
            primes(p);
        } else {
            close(p[WR]);
            close(p[RD]);
            wait(0);
        }
    }
    
    exit(0);
}

int main(int argc,char *argv[]) {   
    int p[2];
    pipe(p);
    for (int i = 2; i <= 35; i++) {
        write(p[WR],&i,INT_LEN);
    }
    if (fork() == 0) {
        primes(p);
    } else {
        close(p[WR]);
        close(p[RD]);
        wait(0);
    }

    exit(0);
}