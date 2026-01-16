#include "server.h"

void print_waiting_emergencies(waiting_queue_t** queue, int len){

    /*
        stampo le emergenze in coda di attesa
    */

    if(queue == NULL || !(len > 0)) return;
    printf("-- LISTA DELLE EMERGENZE IN ATTESA --\n");
    for(int i = 0; len > i; i++){
        if(queue[i] != NULL){
            printf("(%d,%s)\n", queue[i]->id,queue[i]->desc);
        }
    }

}

char* get_state_rescuer(rescuer_status_t status){

    /*
        restituisce lo stato del soccorritore
    */

    char* result;
    switch(status){
        case IDLE: result = "IDLE"; break;
        case EN_ROUTE_TO_SCENE: result = "EN_ROUTE_TO_SCENE"; break;
        case ON_SCENE: result = "ON_SCENE"; break;
        case RETURNING_TO_BASE: result = "RETURNING_TO_BASE"; break;
    }
    return result;
}

int print_state_digital_rescuer(void* args){

    /*
        stampa fino a quando è disponibile la coda
        lo stato dei gemelli digitali
    */

    result_parser_rescuers* r = (result_parser_rescuers*)args;
    while(MESSAGE_QUEUE){
        thrd_sleep(&(struct timespec){.tv_sec = 4}, NULL);
        if(!MESSAGE_QUEUE) break;
        print_digitals_twins(r->rd_twins, r->num_twins);
        printf("\n\n");
    }

    return thrd_success;
}
