#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void checkname(char *path,char *filename) {
    char buf[512];
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p--) 
    ;
    p++;
    memset(buf,0,sizeof(buf));
    memmove(buf,p,strlen(p));
    if (0 == strcmp(filename,buf)) {
        printf("%s\n",path);
    }
}

void find(char *path,char *filename,int opt) {
    int fd;
    struct dirent de;
    struct stat st;
    char buf[512],*p;

    if ((fd = open(path,0)) < 0) {
        return;
    }

    if (fstat(fd,&st) < 0) {
        fprintf(2,"find: cannot stat %s\n",path);
        close(fd);
        exit(1);
    }

    switch(st.type) {
        case T_FILE: {
            checkname(path,filename);
            break;
        }
        case T_DIR: {
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf,path);
            p = buf + strlen(buf);
            *p++ = '/';
            while (read(fd,&de,sizeof(de)) == sizeof(de)) {
                if (de.inum == 0) {
                    continue;
                }
                if (0 == strcmp(".",de.name) || 0 == strcmp("..",de.name)) {
                    continue;
                }
                memmove(p,de.name,DIRSIZ);
                p[DIRSIZ] = 0;
                find(buf,filename,0);
            }
            break;
        }
    }
    close(fd);
}

int main(int argc,char *argv[]) {
    if (argc != 3) {
        fprintf(2,"argument error!\n");
        exit(1);
    }
    find(argv[1],argv[2],1);

    exit(0);
}