#include "parse_env.h"

#define MATRICOLA "671944"

env_t* parser_env(char* filename){

    /*
        Inizializzo i parametri necessari per la scrittura
        sul file.log
    */

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

    /*
        Alloco memoria per il tipo di ritorno della funzione
    */
    
    env_t* res = (env_t*)malloc(sizeof(env_t));
    
    // Alloco memoria per il nome della message queue
    
    res->queue_name = (char*)malloc(sizeof(char)*LENGTH_LINE);

    // Alloco memoria per l'array di caratteri per estrapolare righe dal file

    char* line = (char*)malloc(sizeof(char)*LENGTH_LINE);
    if(line == NULL || res->queue_name == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    while(fgets(line, LENGTH_LINE, file)){

        // Elimino tutti gli spazi della riga corrente

        char* line_trimmed = trim(line);
        if(line_trimmed[strlen(line_trimmed)-1]=='\n') line_trimmed[strlen(line_trimmed)-1] = '\0';
        if(strlen(line_trimmed) == 0) break;

        /*
            Verifico se la sintassi per l'i-esima riga è corretta.
            Se è corretta, i passi successivi vengono svolti
            correttamente
        */
        
        if(format_check_env(line_trimmed, MATRICOLA) == -1) exit(0); 
        

        /*
            riporto sul file.log
        */

        snprintf(message, LENGTH_LINE, "Nuova riga estratta %s", line);
        write_log_file(time(NULL), id, event, message);

        /*
            Estraggo la chiave e il valore e, attraverso delle compare verifico
            quale campo inizializzare.
            Da questa parte in poi tutti i campi della struct env verranno 
            inizializzati correttamente, questa cosa è assicurata dalla verifica
            della sintassi, altrimenti il programma viene bloccato prima.
        */

        char* key = strtok(line, "=");
        char* value = strtok(NULL, " ");
        if(strcmp(key, "queue") == 0) strcpy(res->queue_name, value);
        else if(strcmp(key, "height") == 0) res->y = atoi(value);
        else if(strcmp(key, "width") == 0) res->x = atoi(value);
        
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
