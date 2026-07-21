#include "../include/ast.h"

Command* create_command(int start, int end) {
    Command *cmd = malloc(sizeof(Command));
    cmd->name = strdup(tokens[start]);

    int argc = end - start + 1;
    cmd->args = malloc((argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; i++) {
        cmd->args[i] = strdup(tokens[start + i]);
    }
    cmd->args[argc] = NULL;

    cmd->next = NULL;
    cmd->is_background = 0;
    return cmd;
}


Command* parse_tokens(){
    Command* head = NULL,*tail=NULL;
    int start=0;

    for(int i=0;tokens[i]!=NULL;i++){
        if(strcmp(tokens[i],";")==0){

            if(i>start){
                Command* node=create_command(start,i-1);
                if(!head){
                    head=node;
                }
                else{
                    tail->next=node;
                }
                tail=node;

            }
            start=i+1;
        }else if(strcmp(tokens[i],"|")==0){
            if(i>start){
                Command* node=create_command(start,i-1);
                if(!head){
                    head=node;
                }else{
                    tail->next=node;
                }
                tail=node;
            }
            Command* node=create_command(i,i);
            if(!head){
                head=node;
            }else{
                tail->next=node;
            }
            tail=node;
            start=i+1;
        }else if(strcmp(tokens[i],"&")==0){
            if(i>start){
                Command* node=create_command(start,i-1);
                node->is_background = 1;
                if(!head){
                    head=node;
                }
                else{
                    tail->next=node;
                }
                tail=node;
            }
            start=i+1;
        }
    }

    if(tokens[start]!=NULL){
        int end=start;
        while(tokens[end]!=NULL){
            end++;
        }
        end--;
        Command *node = create_command(start,end);
        if(!head) head=node;
        else tail->next=node;
    }
    return head;
}
