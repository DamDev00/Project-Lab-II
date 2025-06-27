#include "parse_env.h"

env_t* parser_env(char* filename){

    FILE* file = fopen(filename, "r");
    char* id = __FILE__;
    char* event = "FILE_PARSING";
    char message[LENGTH_LINE];

    if(file == NULL){
        printf("{type error: FILE_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(FILE_ERROR);
    }

    snprintf(message, LENGTH_LINE, "File %s aperto correttamente", filename);
    write_log_file(time(NULL), id, event, message);
    
    env_t* res = (env_t*)malloc(sizeof(env_t));
    res->queue_name = (char*)malloc(sizeof(char)*LENGTH_LINE);
    char* line = (char*)malloc(sizeof(char)*LENGTH_LINE);
    if(line == NULL || res->queue_name == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    while(fgets(line, LENGTH_LINE, file)){
        if(line[strlen(line)-1]=='\n') line[strlen(line)-1] = '\0';
        snprintf(message, LENGTH_LINE, "Nuova riga estratta %s", line);
        write_log_file(time(NULL), id, event, message);
        if(strlen(line) == 0) break;
        char* key = strtok(line, "=");
        char* value = strtok(NULL, " ");
        if(strcmp(key, "queue") == 0) strcpy(res->queue_name, value);
        else if(strcmp(key, "height") == 0) res->y = atoi(value);
        else if(strcmp(key, "width") == 0) res->x = atoi(value);
        else {
            snprintf(message, LENGTH_LINE, "Parsing non completato correttamente a causa della linea: %s", line);
            write_log_file(time(NULL), id, event, message);
            printf("{qualcosa è andato storto nel parser}\nriga: %d\nfile: %s\n",__LINE__,__FILE__);
            exit(0);
        }
    }

    fclose(file);

    return res;

}

void print_env(env_t* env){
    
    if(env == NULL){
        printf("Ambiente vuoto\n");
        exit(0);
    }

    printf("queue name: %s\nx: %d\ny: %d\n", env->queue_name, env->x, env->y);
    return;
}
