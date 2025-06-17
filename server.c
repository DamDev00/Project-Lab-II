#include "server.h"
#include <assert.h>
#include <math.h>

int num_emergency_avalaible;
int id_emergencies = 0;
int waiting_queue_len = 0;
emergency_type_t* emergency_avalaible = NULL;
emergency_id_t** queue_emergencies = NULL;
rescuer_data_t* rescuers_data = NULL;
waiting_queue_t** waiting_queue = NULL;
mtx_t lock_operation_on_waiting_queue;
mtx_t lock_operation_on_queue_emergency, l;
sem_t sem_waiting_queue;

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

int control_waiting_queue(void* args){

    emergency_id_t* current_emergency = NULL;
    char curr_twin[LENGTH_LINE];
    printf("[ATTIVATA LA FUNZIONE <control_waiting_queue>]\n");
    while(1){
        thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
        printf("[CONTROLLO EMERGENZE IN WAITING_QUEUE]\n");
        mtx_lock(&lock_operation_on_waiting_queue);
        if(waiting_queue_len > 0){
            for(int i = 0; waiting_queue_len > i; i++){
                current_emergency = queue_emergencies[waiting_queue[i]->id];
                for(int j = 0; current_emergency->num_twins > j; j++){
                    rescuer_digital_twin_t* twin = current_emergency->emergency->rescuers_dt[j];
                    if(current_emergency->emergency->type->priority == 0) continue;
                    if(j > 0 && strcmp(twin->rescuer->rescuer_type_name, curr_twin) == 0) continue;
                    strcpy(curr_twin, twin->rescuer->rescuer_type_name);
                    time_t now = time(NULL);
                    if(distance_manhattan(twin->x, current_emergency->emergency->x, twin->y, current_emergency->emergency->y, twin->rescuer->speed) > get_priority_limit(current_emergency->emergency->type->priority) - (now - current_emergency->emergency->time)) {
                        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                        current_emergency->emergency->status = TIMEOUT;
                        printf("[ELIMINATA DALLA CODA D'ATTESA (%d,%s) PER ERRORE DI DISTANZA DI UN SOCCORRITORE]\n",current_emergency->id, current_emergency->emergency->type->emergency_desc);
                    }
                } 
            }
        }
        mtx_unlock(&lock_operation_on_waiting_queue);
    }

}

int handler_waiting_queue(void* args){

    printf("[HANDLER WAITING QUEUE ATTIVATA]\n");
    int res = 0;
    int index_current = 0;
    thrd_t control_waiting_queue_t;
    if(thrd_create(&control_waiting_queue_t, control_waiting_queue, NULL) == thrd_success){
        thrd_detach(control_waiting_queue_t);
    } else {
        //ERROR
    }
    while(1){

        printf("[N° ELEMENTI NELLA CODA D'ATTESA: %d]\n", waiting_queue_len);
        sem_wait(&sem_waiting_queue);
        printf("[HANDLER WAITING QUEUE: 1 SECONDO DI ATTESA]\n");
        thrd_sleep(&(struct timespec){.tv_sec = 1.2}, NULL);
        mtx_lock(&lock_operation_on_waiting_queue);
        if(waiting_queue_len > 0){
            index_current = check_priority_waiting_queue(index_current, &waiting_queue, waiting_queue_len, &queue_emergencies);
            if(index_current == -1) index_current = 0;
            emergency_id_t* e = queue_emergencies[waiting_queue[index_current]->id];
            mtx_unlock(&lock_operation_on_waiting_queue);
            printf("[PRELEVATA DALLA CODA D'ATTESA (%d,%s)]\n", e->id, e->emergency->type->emergency_desc);
            if(!e->in_loading) res = start_emergency(e);
        } else mtx_unlock(&lock_operation_on_waiting_queue);
        mtx_lock(&lock_operation_on_waiting_queue);
        if(((res == thrd_success || res == -2) || res == -1 || res == 0) && waiting_queue_len > 0){
            int val;
            sem_getvalue(&sem_waiting_queue, &val);
            if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
        }
        mtx_unlock(&lock_operation_on_waiting_queue);
    }

}

int handle_rescuer(void* args){

    int id = *(int*)args;
    id--;
    printf("[SOCCORRITORE: %s;N°:%d] E' STATO ATTIVATO!\n", rescuers_data[id].twin->rescuer->rescuer_type_name, rescuers_data[id].twin->id);

    while(1){
        mtx_lock(&rescuers_data[id].lock);

            while(rescuers_data[id].twin->status == IDLE){
                cnd_wait(&rescuers_data[id].cnd, &rescuers_data[id].lock);
                printf("risvegliato (%d,%s)\n",rescuers_data[id].id_current_emergency,rescuers_data[id].twin->rescuer->rescuer_type_name);
            }
            printf("[SOCCORRITORE:%s;N°:%d] ATTIVATO PER L'EMERGENZA (%d,%s)\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,rescuers_data[id].id_current_emergency,rescuers_data[id].current_emergency);
            
            //calcolo la distanza 
            int from_x = rescuers_data[id].twin->x;
            int to_x = rescuers_data[id].x_emergency;
            int from_y = rescuers_data[id].twin->y;
            int to_y = rescuers_data[id].y_emergency;

            int distance = distance_manhattan(from_x, to_x, from_y,to_y, rescuers_data[id].twin->rescuer->speed);
           
            printf("[(%s, %d) TEMPO STIMATO DI ARRIVO IN SECONDI: %d]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,distance);
            while((rescuers_data[id].twin->x != to_x || rescuers_data[id].twin->y != to_y)){
                int dx = to_x - rescuers_data[id].twin->x;
                int dy = to_y - rescuers_data[id].twin->y;

                if(dx != 0){
                    int verse = (dx > 0) ? 1 : -1;
                    int step = (abs(dx) < rescuers_data[id].twin->rescuer->speed) ? abs(dx) : rescuers_data[id].twin->rescuer->speed;
                    rescuers_data[id].twin->x += step * verse;
                }

                if(dy != 0){
                    int verse = (dy > 0) ? 1 : -1;
                    int step = (abs(dy) < rescuers_data[id].twin->rescuer->speed) ? abs(dy) : rescuers_data[id].twin->rescuer->speed;
                    rescuers_data[id].twin->y += step * verse;
                }

                thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
            }

            atomic_int* rescuers_finished = &queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished;
            atomic_int* tot_rescuers_required = &queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required;
            mtx_t* lock = &queue_emergencies[rescuers_data[id].id_current_emergency]->lock_emergency;
            cnd_t* cnd = &queue_emergencies[rescuers_data[id].id_current_emergency]->cnd_emergency;

            barrier_rescuers(queue_emergencies[rescuers_data[id].id_current_emergency],rescuers_finished, tot_rescuers_required, lock, cnd); 

            rescuers_data[id].twin->status = ON_SCENE;
            
           
            
            printf("[(%s,%d) ARRIVATO, TEMPO PER IL LAVORO RICHIESTO: %d SEC]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, rescuers_data[id].time_to_manage);
            thrd_sleep(&(struct timespec){.tv_sec = rescuers_data[id].time_to_manage}, NULL);
            
            mtx_lock(&l);
            atomic_fetch_add(&queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished, 1);
            if(queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished == queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required){
                printf("[EMERGENZA: (%d,%s)] TERMINATA\n", queue_emergencies[rescuers_data[id].id_current_emergency]->id, queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->type->emergency_desc);
                queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->status = COMPLETED;
            }
            mtx_unlock(&l);
            
            rescuers_data[id].twin->status = RETURNING_TO_BASE;
            distance = distance_manhattan(to_x, from_x, to_y, from_y, rescuers_data[id].twin->rescuer->speed);
            printf("[(%s,%d) LAVORO TERMINATO, TEMPO: %d SEC PER TORNARE ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, distance);
            
            while((rescuers_data[id].twin->x != from_x || rescuers_data[id].twin->y != from_y)){
                int dx = from_x - rescuers_data[id].twin->x;
                int dy = from_y - rescuers_data[id].twin->y;

                if(dx != 0){
                    int verse = (dx > 0) ? 1 : -1;
                    int step = (abs(dx) < rescuers_data[id].twin->rescuer->speed) ? abs(dx) : rescuers_data[id].twin->rescuer->speed;
                    rescuers_data[id].twin->x += step * verse;
                }

                if(dy != 0){
                    int verse = (dy > 0) ? 1 : -1;
                    int step = (abs(dy) < rescuers_data[id].twin->rescuer->speed) ? abs(dy) : rescuers_data[id].twin->rescuer->speed;
                    rescuers_data[id].twin->y += step * verse;
                }

                thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
            }
            
            printf("[(%s,%d) TORNATO ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1);
            rescuers_data[id].twin->status = IDLE;
            mtx_unlock(&rescuers_data[id].lock);

    }
}

int start_emergency(emergency_id_t* current_emergency){

    emergency_t* emergency = current_emergency->emergency;
    current_emergency->in_loading = true;

    int* id_locks = (int*)malloc(sizeof(int)*current_emergency->num_twins);
    if(id_locks == NULL) exit(0);

    for(int i = 0; current_emergency->num_twins > i; i++){
        id_locks[i] = -1;
    }
    
    int tot_request_rescuers = 0;
    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        tot_request_rescuers += emergency->type->rescuers[i].required_count;
    }

    int tot_avalaible_rescuers = 0;

    
  
    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        rescuer_request_t req = emergency->type->rescuers[i];
        short int num_request = 0;
        short int manage = 0;
        for(int j = 0; current_emergency->num_twins > j; j++){
            int miss_rescuers_n = miss_rescuers(current_emergency, i); 
            if(req.type != NULL && strcmp(req.type->rescuer_type_name, emergency->rescuers_dt[j]->rescuer->rescuer_type_name) == 0){
                manage = req.time_to_manage * req.required_count;
                if(mtx_trylock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock) == thrd_success){
                    if(rescuers_data[emergency->rescuers_dt[j]->id-1].twin->status == IDLE){
                        manage = (miss_rescuers_n > 0) ? manage / miss_rescuers_n : req.time_to_manage;
                        num_request++;
                        tot_avalaible_rescuers++;
                        printf("manage per %s: %d\n", emergency->rescuers_dt[j]->rescuer->rescuer_type_name, manage);
                        id_locks[j] = emergency->rescuers_dt[j]->id-1;
                        rescuers_data[emergency->rescuers_dt[j]->id-1].time_to_manage = manage;
                    } else mtx_unlock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock);
                }
            }
            if(num_request == req.required_count) break;
        }
        
        if(num_request == 0){
            current_emergency->in_loading = false;
            for(int k = 0; current_emergency->num_twins > k; k++){
                if(id_locks[k] != -1) mtx_unlock(&rescuers_data[id_locks[k]].lock);
                id_locks[k] = -1;
            }
            printf("[NON E' STATO POSSIBILE AVVIARE L'EMERGENZA (%d,%s) PER NON DISPONIBILITà DI ALCUNI SOCCORRITORI]\n", current_emergency->id, emergency->type->emergency_desc);
            free(id_locks);
            
            if(current_emergency->emergency->status != PAUSED){
                printf("[EMERGENZA (%d,%s) AGGIUNTA IN CODA D'ATTESA]\n", current_emergency->id, emergency->type->emergency_desc);
                current_emergency->emergency->status = PAUSED;
                current_emergency->was_in_waiting_queue = true;
                mtx_lock(&lock_operation_on_waiting_queue);
                add_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                int val;
                sem_getvalue(&sem_waiting_queue, &val);
                if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
                mtx_unlock(&lock_operation_on_waiting_queue);
                
            } else if(current_emergency->emergency->status == PAUSED ){
                printf("[ANCORA NON è POSSIBILE SODDISFARE L'EMERGENZA (%d,%s) IN CODA]\n", current_emergency->id,current_emergency->emergency->type->emergency_desc);
            }
                
            return -1;
        }
    }

    if(current_emergency->was_in_waiting_queue){
        mtx_lock(&lock_operation_on_waiting_queue);
        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
        int val;
        sem_getvalue(&sem_waiting_queue,&val);
        if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
        mtx_unlock(&lock_operation_on_waiting_queue);
    }    

    if(get_priority_limit(emergency->type->priority) != 0){
        for(int i = 0; current_emergency->num_twins > i; i++){
            if(id_locks[i] != -1){
                int id = id_locks[i];
                if(distance_manhattan(rescuers_data[id].twin->x, emergency->x, rescuers_data[id].twin->y, emergency->y, rescuers_data[id].twin->rescuer->speed) > get_priority_limit(emergency->type->priority) - (time(NULL) - emergency->time)){
                    printf("[L'EMERGENZA (%d,%s) NON E' STATA ESEGUITA A CAUSA DI DISTANZA/PROPRITA PER IL SOCCORRITORE: %s']\n",current_emergency->id ,emergency->type->emergency_desc, rescuers_data[id].twin->rescuer->rescuer_type_name);
                    if(emergency->status == PAUSED){
                        mtx_lock(&lock_operation_on_waiting_queue);
                        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                        mtx_unlock(&lock_operation_on_waiting_queue);
                    }
                    time_t now = time(NULL);
                    char* desc = "dei soccorritori non fanno in tempo a soddisfare l'emergenza";
                    write_log_file(now, emergency->type->emergency_desc, EMERGENCY_TIMEOUT, desc);
                    for(int k = 0; current_emergency->num_twins > k; k++){
                        if(id_locks[k] != -1) mtx_unlock(&rescuers_data[id_locks[k]].lock);
                    }
                    free(id_locks);
                    emergency->status = TIMEOUT;
                    return -2;
                }
            }
        }
    }
        
    current_emergency->emergency->status = WAITING;
    current_emergency->tot_rescuers_required = tot_avalaible_rescuers;
    current_emergency->rescuers_finished = 0;
        

    for(int i = 0; current_emergency->num_twins > i; i++){
        if(id_locks[i] != -1){
            int id = id_locks[i];
            rescuers_data[id].current_emergency = (char*)malloc(sizeof(char)*LENGTH_LINE);
            if(rescuers_data[id].current_emergency == NULL){
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
            strcpy(rescuers_data[id].current_emergency, emergency->type->emergency_desc);
            rescuers_data[id].id_current_emergency = current_emergency->id;
            rescuers_data[id].x_emergency = emergency->x;
            rescuers_data[id].y_emergency = emergency->y;
            rescuers_data[id].twin->status = EN_ROUTE_TO_SCENE;
            cnd_signal(&rescuers_data[id].cnd);
            mtx_unlock(&rescuers_data[id].lock);
        }
    }
    
    return thrd_success;

}

int handler_emergency(void* args){

    params_handler_emergency_t* p = (params_handler_emergency_t*)args;
    rescuer_digital_twin_t** twins = p->params->rp_rescuers->rd_twins;
    int num_twins = p->params->rp_rescuers->num_twins;
    p->emergency_avalaible = emergency_avalaible;
    
    emergency_t* emergency = set_new_emergency(p);
    int rescuers_avalaible = rescuers_is_avalaible(twins, num_twins, emergency->type->rescuers, emergency->type->rescuers_req_number, emergency->type->emergency_desc);
    if(rescuers_avalaible == -1){
        printf("l'emergenza: %s; non è possibile soddisfarla\n", emergency->type->emergency_desc);
        time_t time_now = time(NULL);
        char* desc_error = "carenza di risorse a disposizione";
        write_log_file(time_now, emergency->type->emergency_desc, EMERGENCY_CANCELED, desc_error);
        emergency->status = CANCELED;
        mtx_lock(&lock_operation_on_queue_emergency);
        add_emergency(&id_emergencies, emergency, &queue_emergencies);
        mtx_unlock(&lock_operation_on_queue_emergency);
        return -1;
    } 

    emergency->rescuers_dt = (rescuer_digital_twin_t**)malloc(sizeof(rescuer_digital_twin_t*));
    if(emergency->rescuers_dt == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    int current_twins = 0;

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        if(emergency_avalaible[p->index_emergencies].rescuers[i].type != NULL){
            rescuer_request_t req = emergency_avalaible[p->index_emergencies].rescuers[i];
            for(int j = 0; p->params->rp_rescuers->num_twins > j; j++){
                if(strcmp(req.type->rescuer_type_name, p->params->rp_rescuers->rd_twins[j]->rescuer->rescuer_type_name) == 0){
                    emergency->rescuers_dt[current_twins] = twins[j];
                    current_twins++;
                    rescuer_digital_twin_t** tmp = realloc(emergency->rescuers_dt, sizeof(rescuer_digital_twin_t*)*(current_twins+1));
                    if(tmp == NULL){
                        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                        exit(MALLOC_ERROR);
                    }
                    emergency->rescuers_dt = tmp;
                }
            }
        }
    }
   
    mtx_lock(&lock_operation_on_queue_emergency);
    emergency_id_t* current_emergency = add_emergency(&id_emergencies, emergency, &queue_emergencies);
    mtx_unlock(&lock_operation_on_queue_emergency);

    mtx_init(&current_emergency->lock_emergency, mtx_plain);
    cnd_init(&current_emergency->cnd_emergency);
    current_emergency->num_twins = current_twins;
    current_emergency->was_in_waiting_queue = false;
    current_emergency->in_loading = false;
    if(rescuers_avalaible == 0){
        current_emergency->miss_rescuers = true;
    } else current_emergency->miss_rescuers = false;

    
    start_emergency(current_emergency);

    mtx_lock(&lock_operation_on_waiting_queue);
    print_waiting_emergencies(waiting_queue, waiting_queue_len);
    mtx_unlock(&lock_operation_on_waiting_queue);

    return thrd_success;
  
}


int handler_queue_emergency(void* args){

    params_handler_queue_t* params = (params_handler_queue_t*)args;

    assert(params->environment != NULL);
    assert(params->environment->queue_name != NULL);
    assert(strlen(params->environment->queue_name) < LENGTH_LINE-2);
   
    mqd_t message_queue;
    struct mq_attr attr;
    
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    char mq_name[LENGTH_LINE];
    mq_name[0] = '/';
    strncpy(&mq_name[1], params->environment->queue_name, LENGTH_LINE-2);
    mq_name[LENGTH_LINE-1] = '\0';

    if((message_queue = mq_open(mq_name, O_CREAT | O_RDONLY, 0644, &attr)) == -1){
        perror("mq_open");
        return 0;
    } else printf("message queue %s creata con successo!\n", mq_name);
    thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
    printf("Server in ascolto...\n\n");
    void* msg = malloc(MAX_SIZE);

    if(msg == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }
    
    for(;;){

        size_t bytes_read = mq_receive(message_queue, (char*)msg, MAX_SIZE, NULL);
        if(!(bytes_read > 0)){
            perror("mq_receive bytes read");
            exit(0);
        }
        
        emergency_request_t* request = (emergency_request_t*)msg;

        char* desc_error = (char*)malloc(sizeof(char)*LENGTH_LINE);
        if(desc_error == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
       
        int index_emergency = -1;
        if(!is_valid_request(request, params->environment->x, params->environment->y, desc_error, &index_emergency, emergency_avalaible, num_emergency_avalaible)){
            printf("richiesta non valida!\n");
            write_log_file(request->timestamp, request->emergency_name, MESSAGE_QUEUE, desc_error);
            continue;
        }

        params_handler_emergency_t* params_handler_emergency = (params_handler_emergency_t*)malloc(sizeof(params_handler_emergency_t));
        if(params_handler_emergency == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        params_handler_emergency->index_emergencies = index_emergency;
        params_handler_emergency->x = request->x;
        params_handler_emergency->y = request->y;
        params_handler_emergency->params = params;
        params_handler_emergency->timestamp = request->timestamp;
        
        thrd_t thread_handler_emergency;

        thrd_create(&thread_handler_emergency, handler_emergency, params_handler_emergency);
        
    }

    mq_close(message_queue);
    mq_unlink(mq_name);

    return 1;
}

int print_dt(void* args){
    result_parser_rescuers* r = (result_parser_rescuers*)args;
    while(1){
        thrd_sleep(&(struct timespec){.tv_sec = 4}, NULL);
        print_digitals_twins(r->rd_twins, r->num_twins);
        printf("\n\n");
    }
}

void free_emergency_avalaible(emergency_type_t* emergencies, int num){

    if(emergencies == NULL) return;

    for(int i = 0; num > i; i++){
        free(emergencies[i].emergency_desc);
        free(emergencies[i].rescuers);
        emergencies[i].emergency_desc = NULL;
        emergencies[i].rescuers = NULL;
    }

    free(emergencies);
    printf("[GESTORE DELLA MEMORIA: EMERGENZE DISPONIBILI LIBERATE]\n");
    return;

}

int main(){

    mtx_init(&lock_operation_on_waiting_queue, mtx_plain);
    mtx_unlock(&lock_operation_on_waiting_queue);
    mtx_init(&lock_operation_on_queue_emergency, mtx_plain);
    mtx_init(&l, mtx_plain);
    sem_init(&sem_waiting_queue, 0, 0);

    thrd_t handler_queue, handler_waiting_queue_t,a;
    env_t* environment = parser_env(ENVIRONMENT_FILENAME);
    num_emergency_avalaible = 0;
    emergency_avalaible = parser_emergency(EMERGENCY_FILENAME, &num_emergency_avalaible);
    result_parser_rescuers* rp_rescuers = parse_rescuers(RESCUERS_FILENAME);
    
    params_handler_queue_t* params = (params_handler_queue_t*)malloc(sizeof(params_handler_queue_t));
    if(params == NULL) exit(0);
    params->environment = environment;
    params->rp_rescuers = rp_rescuers;

    rescuers_data = (rescuer_data_t*)malloc(sizeof(rescuer_data_t)*rp_rescuers->num_twins);
    if(rescuers_data == NULL) exit(0);
    thrd_t rescuers_active[rp_rescuers->num_twins];

    for(int i = 0; rp_rescuers->num_twins > i; i++){
        rescuers_data[i].twin = rp_rescuers->rd_twins[i];
        mtx_init(&rescuers_data[i].lock, mtx_plain);
        cnd_init(&rescuers_data[i].cnd);
        rescuers_data[i].time_to_manage = 0;
        if(thrd_create(&rescuers_active[i], handle_rescuer, &rp_rescuers->rd_twins[i]->id) == thrd_success){
            thrd_detach(rescuers_active[i]);
        } else {
            //ERROR
        }
    }
    thrd_create(&handler_queue, handler_queue_emergency, params);

    if(thrd_create(&handler_waiting_queue_t, handler_waiting_queue, NULL) == thrd_success){
        thrd_detach(handler_waiting_queue_t);
    } else {
        //ERROR
    }

    thrd_create(&a, print_dt, rp_rescuers);
    
    thrd_join(handler_queue, NULL);
    
    mtx_destroy(&lock_operation_on_waiting_queue);
    mtx_destroy(&lock_operation_on_queue_emergency);
    sem_destroy(&sem_waiting_queue);
    mtx_destroy(&l);

    free_rescuers(rp_rescuers->rescuers_type, rp_rescuers->num_rescuers);
    free_emergency_avalaible(emergency_avalaible, num_emergency_avalaible);
    free_rescuers_digital_twins(rp_rescuers->rd_twins, rp_rescuers->num_twins);

    return 0;
}
