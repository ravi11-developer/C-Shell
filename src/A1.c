#include "../include/A1.h"
#include "../include/shell.h"
void computername(char* res,char* shellcwd){
    char systemname[1024];
    gethostname(systemname,sizeof(systemname));


    int pid=getuid();
    struct passwd *pw=getpwuid(pid);

    char *username;
    if(pw != NULL){
        username=pw->pw_name;
    }
    strcpy(res,username);

    int temp=strncmp(shellcwd,home,strlen(home));


    strcat(res,"@");
    strcat(res,systemname);
    strcat(res,":");
    if(temp){
        strcat(res,shellcwd);
    }else{
        strcat(res,"~");
        strcat(res,shellcwd+strlen(home));   
    }
}