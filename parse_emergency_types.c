#include "parse_emergency_types.h"
#include <stdbool.h>

emergency_type_t* parser_emergency(char* filename, int* num_emergency_type){

    result_parser_rescuers* result = parse_rescuers(RESCUERS_FILENAME);
    rescuer_type_t** rescuers = result->rescuers_type;
    int num_rescuers = result->num_rescuers;
    emergency_type_t* emergency_type = NULL;
    
    char* id = __FILE__;
    char* event = "FILE_PARSING";
    char message[LENGTH_LINE];

    FILE* file = fopen(EMERGENCY_FILENAME, "r");
    
    if(file == NULL){
        printf("{type error: FILE_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(FILE_ERROR);
    }

    snprintf(message, LENGTH_LINE, "File %s aperto correttamente", EMERGENCY_FILENAME);
    write_log_file(time(NULL), id, event, message);

    char* line = (char*)malloc(sizeof(char)*LENGTH_LINE);    

    while(fgets(line, LENGTH_LINE, file)){

        char* line_trimmed = trim(line);
        if(line[strlen(line_trimmed)-1]=='\n') line_trimmed[strlen(line_trimmed)-1] = '\0';
        if(strlen(line_trimmed) == 0) break;
        if(format_check_emergency(line_trimmed) == -1) exit(0);

        snprintf(message, LENGTH_LINE, "Nuova riga estratta %s", line_trimmed);
        write_log_file(time(NULL), id, event, message);
        
        if(emergency_type == NULL){
            emergency_type = (emergency_type_t*)malloc(sizeof(emergency_type_t));
            if(emergency_type == NULL){
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
        } else {
            emergency_type_t* tmp = realloc(emergency_type, sizeof(emergency_type_t)*(*num_emergency_type+1));
            if(tmp == NULL){
                printf("{type error: REALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(REALLOC_ERROR);
            }
            emergency_type = tmp;
        }

        emergency_type[(*num_emergency_type)].emergency_desc = (char*)malloc(sizeof(char)*LENGTH_LINE);
        if(emergency_type[(*num_emergency_type)].emergency_desc == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        char* token = strtok(line_trimmed, "[]");

        strcpy(emergency_type[(*num_emergency_type)].emergency_desc, token);
     
        emergency_type[(*num_emergency_type)].priority = atoi(strtok(NULL, "[]"));

        char** rescuers_char = (char**)malloc(sizeof(char*)*(LENGTH_LINE+1));

        if(rescuers_char == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        short int rescuers_req_number = 0;
        for(char* v = strtok(NULL, ";"); v != NULL; v = strtok(NULL, ";"), rescuers_req_number++){
            int len = strlen(v);
            if(len <= 1) break;
            rescuers_char[rescuers_req_number] = malloc(LENGTH_LINE*(sizeof(char)));
            if(rescuers_char[rescuers_req_number] == NULL){
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
            strcpy(rescuers_char[rescuers_req_number], v);
            rescuers_char[rescuers_req_number][len] = '\0';
        }

        emergency_type[(*num_emergency_type)].rescuers_req_number = rescuers_req_number;
        
        rescuer_request_t* rescuers_request = (rescuer_request_t*)malloc(sizeof(rescuer_request_t)*rescuers_req_number);
        
        if(rescuers_request == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
        memset(rescuers_request, 0, sizeof(rescuer_request_t)*rescuers_req_number);
        
        short int count_rescuer_request = 0;
        for(int i = 0; rescuers_req_number > i; i++){
            short int counter = 1;
            bool failed = false;
            for(char* token = strtok(rescuers_char[i], ":"); token != NULL; token = strtok(NULL, ","),counter++){
                switch(counter){
                    case 1: {
                        rescuers_request[count_rescuer_request].type = find_rescuer(rescuers, token, num_rescuers);
                        if(rescuers_request[count_rescuer_request].type == NULL) failed = true;
                    } break;
                    case 2: rescuers_request[count_rescuer_request].required_count = atoi(token); break;
                    case 3: rescuers_request[count_rescuer_request].time_to_manage = atoi(token); break;
                }
                if(failed) break;
            }
            if(!failed) count_rescuer_request++;
        }

       emergency_type[(*num_emergency_type)].rescuers = rescuers_request;
       (*num_emergency_type)++;
      
    }

    free(line);
    fclose(file);

    strcpy(message, "Parsing completato");
    write_log_file(time(NULL), id, event, message);

    return emergency_type;
}

void print_emergencies(emergency_type_t* emergencies, int num){

    if(emergencies == NULL || !(num > 0)) {
        printf("non ci sono emergenze\n");
        return;
    }

    printf("-- LISTA EMERGENZE --\n\n");

    for(int i = 0; num > i; i++){
        printf("\nEmergenza numero: %d\ndescrizione: %s, priorità: %d, soccorritori necessari: %d\n\n", 
        (i+1),emergencies[i].emergency_desc,
        emergencies[i].priority,emergencies[i].rescuers_req_number);
        printf("-- soccorritori disponibili per l'emergenza --\n\n");
        rescuer_request_t* tmp = emergencies[i].rescuers;
        for(int j = 0; emergencies[i].rescuers_req_number > j; j++){
            if(tmp[j].type != NULL)
                printf("soccorritore: %s, istanze richieste: %d, tempo dedicato %d (s)\n", tmp[j].type->rescuer_type_name,
                tmp[j].required_count, tmp[j].time_to_manage);
        }
    }

    return;

}
