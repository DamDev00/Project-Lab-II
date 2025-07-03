#include "parse_rescuers.h"
#include "functions.h"
#include <stdbool.h>


rescuer_type_t* find_rescuer(rescuer_type_t** rescuers, char* name, int num_rescuers){

    /*
     Verifico che siano presenti dei soccorritori disponibili
     e che abbia un nome paragonabile.
     Se esiste, restituisco direttamente la referenza, altrimenti NULL
    */
    
    if(rescuers == NULL || strlen(name) == 0 || !(num_rescuers > 0)) return NULL;
    
    for(int i = 0; num_rescuers > i; i++){
        rescuer_type_t* rescuer = rescuers[i];
        if(rescuer != NULL && strcmp(rescuer->rescuer_type_name, name) == 0) return rescuer;
    }
    

    return NULL;

}

result_parser_rescuers* parse_rescuers(char* filename){

    /*
        Inizializzo i parametri necessari per la scrittura
        sul file.log
    */

    char* id = __FILE__;
    char* event = "FILE_PARSING";
    char message[LENGTH_LINE];
    
    FILE* file = fopen(filename, "r");
    
    if(file == NULL){
        strcpy(message,"errore nell'apertura del file");
        write_log_file(time(NULL), id, event, message);
        printf("{type error: FILE_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(FILE_ERROR);
    }

    /*
        Ad ogni punto del programma in cui scrive nel file.log
        viene sovrascritto l'array di carratteri 'message' in base
        al tipo di evento/operazione
    */

    snprintf(message, LENGTH_LINE, "File %s aperto correttamente", filename);
    write_log_file(time(NULL), id, event, message);

    /*
        Inizializzo l'array dei gemelli digitali e soccorritori disponibili
        che tenderanno a evolversi durante l'esecuzione
    */

    rescuer_digital_twin_t** rd_twins = NULL;
    rescuer_type_t** rescuers_type = NULL;
    
    int num_rd_twins = 0;
    int num_rescuers_type = 0;

    char* line = (char*)malloc(sizeof(char)*LENGTH_LINE);
    short int id_twins = 1;

    /*
        Variabile booleana che serve per capire se l'array dei soccorritori
        è stato aggiornato. l'idea è che se il campo del numero dei mezzi di un 
        soccorritore non è > 0 allora è inutile aggiungerlo, e quindi verifico 
        attraverso la condizione if(!update_rescuer) per capire se devo allocare/ri-allocare
        la memoria oppure no. Chiaramente se il ciclo arriva fino in fondo, update_server sarà
        uguale a false e quindi si può procedere con l'allocazione della memoria
    */

    bool update_rescuer = false;

    while (fgets(line, LENGTH_LINE, file)) {

        char* line_trimmed = trim(line);
        if(line_trimmed[strlen(line_trimmed)-1]=='\n') line_trimmed[strlen(line_trimmed)-1] = '\0';
        if(strlen(line_trimmed) == 0) break;
        if(format_check_rescuers(line_trimmed) == -1) exit(0);

        /*
            Prelevo una nuova riga, controllando che non sia vuota e
            verificando che abbia la sintassi corretta.
            successivamente riporto sul file.log che è stata
            estratta una nuova riga correttamente
        */
      
        snprintf(message, LENGTH_LINE, "Nuova riga estratta %s", line_trimmed);
        write_log_file(time(NULL), id, event, message);

        /*
            Se l'array dei soccorritori è vuoto, lo inizializzo con una malloc
            di lunghezza uno. Se non è vuoto utilizzo una realloc per 
            ri-allocare memoria con una lunghezza maggiore di +1
            rispetto alla lunghezza corrente.
        */

        if(!update_rescuer){

            if(rescuers_type == NULL){
                rescuers_type = (rescuer_type_t**)malloc(sizeof(rescuer_type_t*)*(num_rescuers_type+1));
                if(rescuers_type == NULL){
                    strcpy(message,"errore nell'allocazione della memoria");
                    write_log_file(time(NULL), id, event, message);
                    printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                    exit(MALLOC_ERROR);
                }
            } else {
                rescuer_type_t** tmp = realloc(rescuers_type, sizeof(rescuer_type_t*)*(num_rescuers_type+1));
                if(tmp == NULL){
                    strcpy(message,"errore nell'allocazione della memoria");
                    write_log_file(time(NULL), id, event, message);
                    printf("{type error: REALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                    exit(REALLOC_ERROR);
                }
                rescuers_type = tmp;
            }
            update_rescuer = true;
        }

        // Inizializzo il soccorritore corrente

        rescuer_type_t* rescuer_type = (rescuer_type_t*)malloc(sizeof(rescuer_type_t));
        if(rescuer_type == NULL){
            strcpy(message,"errore nell'allocazione della memoria");
            write_log_file(time(NULL), id, event, message);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        // Alloco memoria per il nome del soccorritore
        
        rescuer_type->rescuer_type_name = (char*)malloc(sizeof(char)*LENGTH_LINE);

        if(rescuer_type->rescuer_type_name == NULL){
            strcpy(message,"errore nell'allocazione della memoria");
            write_log_file(time(NULL), id, event, message);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        /*
            Inizializzo 3 variabili tra cui:
            - delimiters: serve per tokenizzare la stringa con strtok();
            - field: serve per capire in che campo mi trovo tra <nome soccorritore><numero di mezzi disponibili><velocità><posizione>
              che viene sfruttato con lo switch per capire che tipo di campo aggiornare;
            - num: è il numero di soccorritori disponibili per ogni soccorritore (secondo campo) 
        */
        
        const char* delimiters = "[]";
        short int field = 0;
        short int num = 0;

        for(char* token = strtok(line_trimmed, delimiters); token != NULL; token = strtok(NULL, delimiters), field++){  
            switch(field){
                // Nome
                case 0: {
                    strcpy(rescuer_type->rescuer_type_name, token); 
                } break;
                // Quanti soccorritori di un determinato tipo sono disponibili
                case 1: {
                    num = atoi(token);
                } break;
                // Velocità
                case 2: {
                    rescuer_type->speed = atoi(token); 
                } break;
                // Coordinate
                case 3: {
                    char* t = strtok(token, ";");
                    rescuer_type->x = atoi(t);
                    t = strtok(NULL, ";");
                    rescuer_type->y = atoi(t);
                } break;
            } 
        }

        /*
            Verifico se, per x soccorritore, è disponibile almeno un mezzo,
            se non è cosi è inutile continuare e passo alla riga successiva, liberando
            la memoria dal soccorritore appena allocato
        */

        if(!(num > 0)){
            free(rescuer_type->rescuer_type_name);
            free(rescuer_type);
            continue;
        }
        
        /*
            Se l'array dei gemelli digitali è vuoto, lo inizializzo con una malloc
            di lunghezza uno. Se non è vuoto utilizzo una realloc per 
            ri-allocare memoria con una lunghezza di (numero gemelli attuali) + num.
        */

        if(rd_twins == NULL){
            rd_twins = (rescuer_digital_twin_t**)malloc(sizeof(rescuer_digital_twin_t*)*num);
            if(rd_twins == NULL){
                strcpy(message,"errore nell'allocazione della memoria");
                write_log_file(time(NULL), id, event, message);
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
        } else {
            rescuer_digital_twin_t** tmp = realloc(rd_twins, sizeof(rescuer_digital_twin_t*)*(num+num_rd_twins));
            if(tmp == NULL){
                strcpy(message,"errore nell'allocazione della memoria");
                write_log_file(time(NULL), id, event, message);
                printf("{type error: REALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(REALLOC_ERROR);
            }
            rd_twins = tmp;
        }
    
        /*
            Ciclo per inizializzare l'i-esimo gemello digitale
            chiaramente con id diverso. Il ciclo viene ripetuto 'num' volte e inizia
            da dove ci si era fermati (num_rd_twins)
        */

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

        // Inizializzo l'i-esimo soccorritore nell'array dei soccorritori
    
        rescuers_type[num_rescuers_type] = (rescuer_type_t*)malloc(sizeof(rescuer_type_t));
        if(rescuers_type == NULL){
            strcpy(message,"errore nell'allocazione della memoria");
            write_log_file(time(NULL), id, event, message);
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        rescuers_type[num_rescuers_type] = rescuer_type;

        // Aggiorno il numero di soccorritori e gemelli digitali

        num_rd_twins += num;
        num_rescuers_type++;
        update_rescuer = false;

    }

    // Riporto tutto nel file.log

    strcpy(message, "Parsing completato");
    write_log_file(time(NULL), id, event, message);

    // Libero la memoria

    free(line);
    fclose(file);

    /*
        Inizializzo il tipo di ritorno della funzione che ha come campi:
        - array gemelli digitali e soccorritori;
        - grandezze dei rispettivi array;
    */ 

    result_parser_rescuers* result = (result_parser_rescuers*)malloc(sizeof(result_parser_rescuers));
    if(result == NULL){
        strcpy(message,"errore nell'allocazione della memoria");
        write_log_file(time(NULL), id, event, message);
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

    /*
        Verifico prima che l'array non sia vuoto
    */

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

    /*
        Verifico prima che l'array non sia vuoto
    */

    if(rescuers == NULL || !(num > 0)) return;

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

    /*
        Verifico che l'array non sia vuoto
    */
    
    if(rd_twins == NULL || !(num > 0)) {
        printf("nessun gemello digitale da stampare\n");
        return;
    }

    // Per ogni iterazione stampo ogni campo

    for(int i = 0; num > i; i++){
        if(rd_twins[i] != NULL){
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
    }

    return;

}

void print_rescuers(rescuer_type_t** rescuers, int num){

    /*
        Verifico che l'array non sia vuoto
    */

    if(rescuers == NULL || !(num > 0)) {
        printf("nessun soccorritore da stampare\n");
        return;
    }

    // Per ogni iterazione stampo ogni campo

    for(int i = 0; num > i; i++){
        if(rescuers[i] != NULL)
            printf("soccorritore: %s, speed, %d, (x,y) = (%d,%d)\n", rescuers[i]->rescuer_type_name,
                rescuers[i]->speed, rescuers[i]->x, rescuers[i]->y);
    }

    return;

}
