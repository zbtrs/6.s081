#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc,char *argv[]) {

    char buf[512];
    int len = read(0,buf,512),cur = 0;
    char *childargv[MAXARG];
    for (int i = 0; i + 1 < argc; i++) {
        childargv[i] = argv[i + 1];
    }

    while (cur < len) {
        int ncur = cur;
        while (ncur < len && buf[ncur] != '\n') {
            ncur++;
        }
        char newargv[512], *p;
        memset(newargv,0,sizeof(newargv));
        p = newargv;
        for (int i = cur; i < ncur; i++) {
            *p++ = buf[i];
        }
        cur = ncur + 1;
        childargv[argc - 1] = newargv;
        if (fork() == 0) {
            exec(childargv[0],childargv);
            printf("cannot exec %s\n",childargv[0]);
            exit(1);
        }
        wait(0);
    }

    exit(0);
}