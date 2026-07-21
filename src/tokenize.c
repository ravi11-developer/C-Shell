#include "tokenize.h"
#include "shell.h"

int tokenize_input(char *input) {
    int count = 0;
    char *token = strtok(input, " \t\r\n");
   while (token != NULL && count < MAX_TOKENS) {
    char *p = token;
    char buffer[1024];
    int buf_i = 0;

    while (*p != '\0') {
        if (*p == ';' || *p=='|' || *p == '&') {
            if (buf_i > 0) {
                buffer[buf_i] = '\0';
                tokens[count++] = strdup(buffer);
                buf_i = 0;
            }

            if(*p == ';') tokens[count++]=strdup(";");
            if(*p=='|') tokens[count++]=strdup("|");
            if(*p=='&') tokens[count++]=strdup("&");
        } else {
            buffer[buf_i++] = *p;
        }
        p++;
    }

    if (buf_i > 0) {
        buffer[buf_i] = '\0';
        tokens[count++] = strdup(buffer);
    }

    token = strtok(NULL, " \t\r\n");
}

    tokens[count] = NULL;
    return count;
}