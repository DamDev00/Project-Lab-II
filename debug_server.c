#include "server.h"

int status_emergency(void* args){
    emergency_id_t* e = (emergency_id_t*)args;
    while(e->emergency->status != COMPLETED && e->emergency->status != TIMEOUT){
        thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
        char* state;
        switch(e->emergency->status){
            case WAITING: state = "WAITING"; break;
            case ASSIGNED: state = "ASSIGNED"; break;
            case IN_PROGRESS: state = "IN_PROGESS"; break;
            case PAUSED: state = "PAUSED"; break;
            case COMPLETED: state = "COMPLETED"; break;
            case CANCELED: state = "CANCELED"; break;
            case TIMEOUT: state = "TIMEOUT"; break;
        }
        printf("stato (%d,%s): %s\n",e->id, e->emergency->type->emergency_desc ,state);
    }
    return 1;
}

void print_waiting_emergencies(waiting_queue_t** queue, int len){

    if(queue == NULL || !(len > 0)) return;
    printf("-- lista di emergenze in coda d'attesa --\n");
    for(int i = 0; len > i; i++){
        if(queue[i] != NULL){
            printf("(%d,%s)\n", queue[i]->id,queue[i]->desc);
        }
    }

}

void print_requests_emergencies(emergency_id_t** queue, int num){


    for(int i = 0; num > i; i++){
        
        char* state;
        switch(queue[i]->emergency->status){
            case WAITING: state = "WAITING"; break;
            case ASSIGNED: state = "ASSIGNED"; break;
            case IN_PROGRESS: state = "IN_PROGESS"; break;
            case PAUSED: state = "PAUSED"; break;
            case COMPLETED: state = "COMPLETED"; break;
            case CANCELED: state = "CANCELED"; break;
            case TIMEOUT: state = "TIMEOUT"; break;
        }
        // DA MIGLIORARE PER L'ORARIO
        printf("id: %d, emergenza: %s, (x,y) = (%d,%d), status: %s, timestamp: %ld\n",queue[i]->id, queue[i]->emergency->type->emergency_desc,
        queue[i]->emergency->x, queue[i]->emergency->y, state, queue[i]->emergency->time);
    }

}

char* get_state_rescuer(rescuer_status_t status){
    char* result;
    switch(status){
        case IDLE: result = "IDLE"; break;
        case EN_ROUTE_TO_SCENE: result = "EN_ROUTE_TO_SCENE"; break;
        case ON_SCENE: result = "ON_SCENE"; break;
        case RETURNING_TO_BASE: result = "RETURNING_TO_BASE"; break;
    }
    return result;
}
