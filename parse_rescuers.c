#include "parse_rescuers.h"
#include "functions.h"


rescuer_type_t* find_rescuer(rescuer_type_t** rescuers, char* name, int num_rescuers){

    
    if(rescuers == NULL || strlen(name) == 0 || !(num_rescuers > 0)) return NULL;
    
    for(int i = 0; num_rescuers > i; i++){
        rescuer_type_t* rescuer = rescuers[i];
        if(rescuer != NULL && strcmp(rescuer->rescuer_type_name, name) == 0) return rescuer;
    }
    

    return NULL;

}

result_parser_rescuers* parse_rescuers(char* filename){

    time_t now = time(NULL);
    char* id = __FILE__;
    char* event = "FILE_PARSING";
    char* messaggio;
    
    FILE* file = fopen(filename, "r");
    
    if(file == NULL){
        messaggio = "errore nell'apertura del file";
        write_log_file(now, id, event, messaggio);
        printf("{type error: FILE_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(FILE_ERROR);
    }

    messaggio = "file aperto correttamente";
    write_log_file(now, id, event, messaggio);

    rescuer_digital_twin_t** rd_twins = NULL;
    rescuer_type_t** rescuers_type = NULL;
    int num_rd_twins = 0;
    int num_rescuers_type = 0;

    char* line = (char*)malloc(sizeof(char)*LENGTH_LINE);
    short int id_twins = 1;

    while (fgets(line, LENGTH_LINE, file)) {

        char* line_trimmed = trim(line);
        if(line_trimmed[strlen(line_trimmed)-1]=='\n') line_trimmed[strlen(line_trimmed)-1] = '\0';
        if(strlen(line_trimmed) == 0) break;

        now = time(NULL);
        messaggio = "nuova riga estratta correttamente";
        write_log_file(now, id, event, messaggio);


         if(rescuers_type == NULL){
            rescuers_type = (rescuer_type_t**)malloc(sizeof(rescuer_type_t*)*(num_rescuers_type+1));
            if(rescuers_type == NULL){
                messaggio = "errore nell'allocazione della memoria";
                write_log_file(now, id, event, messaggio);
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
        } else {
            rescuer_type_t** tmp = realloc(rescuers_type, sizeof(rescuer_type_t*)*(num_rescuers_type+1));
            if(tmp == NULL){
                messaggio = "errore nell'allocazione della memoria";
                write_log_file(now, id, event, messaggio);
                printf("{type error: REALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(REALLOC_ERROR);
            }
            rescuers_type = tmp;
        }
        
        const char* delimiters = "[]";
        short int field = 0;
        short int num;

        rescuer_type_t* rescuer_type = (rescuer_type_t*)malloc(sizeof(rescuer_type_t));
        if(rescuer_type == NULL){
            messaggio = "errore nell'allocazione della memoria";
            write_log_file(now, id, event, messaggio);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
        
        rescuer_type->rescuer_type_name = (char*)malloc(sizeof(char)*LENGTH_LINE);

        if(rescuer_type->rescuer_type_name == NULL){
            messaggio = "errore nell'allocazione della memoria";
            write_log_file(now, id, event, messaggio);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        char* valori_estratti = (char*)malloc(sizeof(char)*LENGTH_LINE);
        if(valori_estratti == NULL){
            messaggio = "errore nell'allocazione della memoria";
            write_log_file(now, id, event, messaggio);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        char* name;
        char* num_c;
        char* speed;
        char* x;
        char* y;
        char* delimiter = ",";

        for(char* token = strtok(line_trimmed, delimiters); token != NULL; token = strtok(NULL, delimiters), field++){  
            switch(field){
                case 0: {
                    strcpy(rescuer_type->rescuer_type_name, token); 
                    name = token;
                } break;
                case 1: {
                    num = atoi(token);
                    num_c = token; 
                } break;
                case 2: {
                    rescuer_type->speed = atoi(token); 
                    speed = token;
                } break;
                case 3: {
                    char* t = strtok(token, ";");
                    x = t;
                    rescuer_type->x = atoi(t);
                    t = strtok(NULL, ";");
                    y = t;
                    rescuer_type->y = atoi(t);
                    break; 
                }
            } 
        }

        strcpy(valori_estratti, name);
        strcat(valori_estratti, delimiter);
        strcat(valori_estratti, num_c);
        strcat(valori_estratti, delimiter);
        strcat(valori_estratti, speed);
        strcat(valori_estratti, delimiter);
        strcat(valori_estratti, x);
        strcat(valori_estratti, delimiter);
        strcat(valori_estratti, y);
        
        now = time(NULL);
        messaggio = "(valori estratti)";
        strcat(valori_estratti, messaggio);
        write_log_file(now, id, event, valori_estratti);
    

        if(rd_twins == NULL){
            rd_twins = (rescuer_digital_twin_t**)malloc(sizeof(rescuer_digital_twin_t*)*num);
            if(rd_twins == NULL){
                now = time(NULL);
                messaggio = "errore nell'allocazione della memoria";
                write_log_file(now, id, event, messaggio);
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
        } else {
            rescuer_digital_twin_t** tmp = realloc(rd_twins, sizeof(rescuer_digital_twin_t*)*(num+num_rd_twins));
            if(tmp == NULL){
                messaggio = "errore nell'allocazione della memoria";
                write_log_file(now, id, event, messaggio);
                printf("{type error: REALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(REALLOC_ERROR);
            }
            rd_twins = tmp;
        }
    
        
        for(int i = num_rd_twins; num+num_rd_twins > i; i++){
            rd_twins[i] = (rescuer_digital_twin_t*)malloc(sizeof(rescuer_digital_twin_t));
            if(rd_twins[i] == NULL){
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
            rd_twins[i]->id = id_twins++;
            rd_twins[i]->rescuer = rescuer_type;
            rd_twins[i]->status = IDLE;
            rd_twins[i]->x = rescuer_type->x;
            rd_twins[i]->y = rescuer_type->y;
        }
    
        rescuers_type[num_rescuers_type] = (rescuer_type_t*)malloc(sizeof(rescuer_type_t));
        if(rescuers_type == NULL){
            messaggio = "errore nell'allocazione della memoria";
            write_log_file(now, id, event, messaggio);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        rescuers_type[num_rescuers_type] = rescuer_type;

        num_rd_twins += num;
        num_rescuers_type++;

    
        
    }
    
    free(line);
    fclose(file);

    result_parser_rescuers* result = (result_parser_rescuers*)malloc(sizeof(result_parser_rescuers));
    if(result == NULL){
        now = time(NULL);
        messaggio = "errore nell'allocazione della memoria";
        write_log_file(now, id, event, messaggio);
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    result->num_rescuers = num_rescuers_type;
    result->num_twins = num_rd_twins;
    result->rescuers_type = rescuers_type;
    result->rd_twins = rd_twins;

    return result;

}

void free_rescuers_digital_twins(rescuer_digital_twin_t** rd_twin, int n_twins){

    if(rd_twin == NULL || !(n_twins > 0)) return;

    for(int i = 0; n_twins > i; i++){
        free(rd_twin[i]);      
        rd_twin[i] = NULL; 
    }

    free(rd_twin);
    printf("[GESTIONE DELLA MEMORIA: SOCCORRITORI DIGITALI LIBERATI]\n");
    return;
}

void free_rescuers(rescuer_type_t** rescuers, int num){

    if(rescuers == NULL) return;

    for(int i = 0; num > i; i++){
        if(rescuers[i] != NULL){
            free(rescuers[i]->rescuer_type_name);
            rescuers[i]->rescuer_type_name = NULL;
            free(rescuers[i]);
            rescuers[i] = NULL;
        }
    }

    free(rescuers);
    printf("[GESTORE DELLA MEMORIA: LIBERATI I SOCCORRITORI]\n");
    return;

}

void print_digitals_twins(rescuer_digital_twin_t** rd_twins, int num){
    
    if(rd_twins == NULL || !(num > 0)) {
        printf("nessun gemello digitale da stampare\n");
        return;
    }

    for(int i = 0; num > i; i++){
        char* state;
        switch(rd_twins[i]->status){
            case IDLE: state = "IDLE"; break;
            case EN_ROUTE_TO_SCENE: state = "EN_ROUTE_TO_SCENE"; break;
            case ON_SCENE: state = "ON_SCENE"; break;
            case RETURNING_TO_BASE: state = "RETURNING_TO_BASE"; break;
        }
        printf("id: %d, soccorritore: %s, posizione: (%d,%d), velocità: %d, stato: %s\n",
        rd_twins[i]->id, rd_twins[i]->rescuer->rescuer_type_name, rd_twins[i]->x,
        rd_twins[i]->y, rd_twins[i]->rescuer->speed, state);
    }

    return;

}

void print_rescuers(rescuer_type_t** rescuers, int num){

    if(rescuers == NULL || !(num > 0)) {
        printf("nessun soccorritore da stampare\n");
        return;
    }

    for(int i = 0; num > i; i++){
        if(rescuers[i] != NULL)
            printf("soccorritore: %s, speed, %d, (x,y) = (%d,%d)\n", rescuers[i]->rescuer_type_name,
                rescuers[i]->speed, rescuers[i]->x, rescuers[i]->y);
    }

    return;

}
