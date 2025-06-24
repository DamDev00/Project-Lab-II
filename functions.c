#include "functions.h"
#include <stdbool.h>

char* trim(char* line){
    int len = strlen(line);
    char* result = (char*)malloc(sizeof(char)*(len + 1)); 
    if (result == NULL) {
        printf("{MALLOC ERROR}\n");
        exit(MALLOC_ERROR); 
    }
    int c_result = 0;
    for (int i = 0; i < len; i++) {
        if ((line[i] != ' ') || (i+1 <= len-1 && ((line[i+1] >= 'a' && line[i+1] <= 'z') || (line[i+1] >= 'A' && line[i+1] <= 'Z')) && line[i] == ' ')) {
            result[c_result++] = line[i];
        }
    }
    result[c_result] = '\0';
    return result;
}

void write_log_file(time_t timestamp, char* id, char* evento, char* desc){

    int fd;
    SCALL(fd, open(LOG_FILENAME, O_WRONLY | O_APPEND, 0644), "apertura file");

    struct tm* tm_info = localtime(&timestamp);

    char line_log[LENGTH_LINE];
    snprintf(line_log, sizeof(line_log), "[%d:%d:%d] [%s] [%s] [%s] \n", tm_info->tm_hour,tm_info->tm_min, tm_info->tm_sec, id, evento, desc);

    size_t bytes = write(fd, line_log, strlen(line_log));

    if(bytes == -1){
        perror("Errore nella scrittura del file");
        SCALL(fd, close(fd), "close");
        exit(FILE_ERROR);
    }

    SCALL(fd, close(fd), "close");
    return;

}

int format_check_rescuers(char* line){

    // [string][n][n][x;y]

    if(!(strlen(line) > 0)) return -1;
    short int left_square_brackets = 0;
    short int right_square_brackets = 0;

    int count = 0;
    bool semicolon = false;

    while(line[count] != '\0'){

        if(line[count++] != '[') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
            return -1;
        }
        if(line[count] == ']') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
            return -1;
        }
        left_square_brackets++;
        
        while(line[count] != ']' && line[count] != '\0'){
            switch(left_square_brackets){
                case 1: if(!(line[count] >= 'a' && line[count] <= 'z') && !(line[count] >= 'A' && line[count] <= 'Z') && line[count] != ' ') {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [x][<int>][<int>][<int>;<int>]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                } break;
                case 2:
                case 3: if(!(line[count] >= '0' && line[count] <= '9')) {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [<string>][x][x][<int>;<int>]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                } break;
                case 4: if(((line[count-1] == '[' && line[count] == ';') || 
                (line[count] == ';' && line[count+1] == ']')) && !(line[count] >= '0' && line[count] <= '9')) {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [<string>][<int>][<int>][x]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                }
                if(line[count] == ';') semicolon = true;
            }
            count++;
        }
        if(line[count] == ']'){
            right_square_brackets++;
            count++;
        } else return -1;


    }

    if(!semicolon){
        printf("[ERRORE NEL PARSING DEL FILE: %s PER LA MANCANZA DEL ';' [<string>][<int>][<int>][x]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
        return -1;
    }

    return 1;
  

}

int format_check_emergency(char* line){
    // [<string>][<int>]<string>:<int>,<int>;<string>:<int>,<int>;

    if(!(strlen(line) > 0)) return -1;

    int count = 0;
    int close_square_brackets = 0;

    while(close_square_brackets < 2){
        if(line[count++] != '[') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        if(line[count] == ']'){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        while(line[count] != ']'){
            switch(close_square_brackets){
                case 0: if(!(line[count] >= 'a' && line[count] <= 'z') && !(line[count] >= 'A' && line[count] <= 'Z') && line[count] != ' '){
                            printf("[ERRORE NEL PARSING DEL FILE: %s PER CARATTERE NON APPROPRIATO]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                            return -1;
                        } break;
                case 1:  if(!(line[count] >= '0' && line[count] <= '9')) {
                            printf("[ERRORE NEL PARSING DEL FILE: %s PER CARATTERE NON APPROPRIATO]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                            return -1;
                        } break;
            }
            count++;
        }
        close_square_brackets++;
        count++;
    }

    char rest[LENGTH_LINE];
    strncpy(rest, line+count, sizeof(rest));
    char* copy = strdup(rest);
    char* tok = strtok(copy, ";");
    if(strcmp(tok, rest) == 0){
        printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
        return -1;
    }
    

    while(tok != NULL){
       int count_tok = 0;
       while(tok[count_tok] != ':'){
            if(!(tok[count_tok] >= 'a' && tok[count_tok] <= 'z') && !(tok[count_tok] >= 'A' && tok[count_tok] <= 'Z') && tok[count_tok] != ' '){
                printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                return -1;
            }
            count_tok++;
        }
        count_tok++;
        if(!(tok[count_tok] >= '0' && tok[count_tok] <= '9')) {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        count_tok++;
        while (tok[count_tok] >= '0' && tok[count_tok] <= '9') count_tok++;
        if(tok[count_tok++] != ',') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        if(!(tok[count_tok] >= '0' && tok[count_tok] <= 'z')) {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        while (tok[count_tok] >= '0' && tok[count_tok] <= '9') count_tok++;
        if(tok[count_tok] != '\0') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }

        tok = strtok(NULL, ";");
    }


    return 1;
}
