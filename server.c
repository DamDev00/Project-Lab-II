#include "server.h"
#include <assert.h>
#include <math.h>

bool MESSAGE_QUEUE_ACTIVE = true;

int num_emergency_avalaible;
emergency_type_t* emergency_avalaible = NULL;

int waiting_queue_len = 0;
waiting_queue_t** waiting_queue = NULL;

int id_emergencies = 0;
emergency_id_t** queue_emergencies = NULL;

rescuer_data_t* rescuers_data = NULL;

mtx_t lock_operation_on_waiting_queue;
mtx_t lock_operation_on_queue_emergency;

sem_t sem_waiting_queue;


int control_waiting_queue(void* args){

    emergency_id_t* current_emergency = NULL;
    char curr_twin[LENGTH_LINE];
    printf("[ATTIVATA LA FUNZIONE <control_waiting_queue>]\n");
    while(1){
        if(!MESSAGE_QUEUE_ACTIVE) break;
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
                        current_emergency->emergency->status = TIMEOUT;
                        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                        printf("[ELIMINATA DALLA CODA D'ATTESA (%d,%s) PER ERRORE DI DISTANZA DI UN SOCCORRITORE]\n",current_emergency->id, current_emergency->emergency->type->emergency_desc);
                        char id_log[64];
                        snprintf(id_log, sizeof(id_log), "(%d,%s)", current_emergency->id, current_emergency->emergency->type->emergency_desc);
                        write_log_file(time(NULL), id_log, EMERGENCY_OPERATION, "emergenza non gestita per la distaza di un soccorritore");
                        break;
                    }
                } 
            }
        }
        mtx_unlock(&lock_operation_on_waiting_queue);
    }

    printf("[CONTROL_WAITING_QUEUE TERMINATO]\n");
    return thrd_success;

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

        if(!MESSAGE_QUEUE_ACTIVE) break;
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

    printf("[HANDLER WAITING QUEUE TERMINATO]\n");
    return thrd_success;

}

int handle_rescuer(void* args){

    int id = *(int*)args;
    id--;
    printf("[SOCCORRITORE: %s;N°:%d] E' STATO ATTIVATO!\n", rescuers_data[id].twin->rescuer->rescuer_type_name, rescuers_data[id].twin->id);

    while(1){
        
        if(!MESSAGE_QUEUE_ACTIVE) break;
        mtx_lock(&rescuers_data[id].lock);
            rescuers_data[id].is_been_called = false;
            while(rescuers_data[id].twin->status == IDLE){
                cnd_wait(&rescuers_data[id].cnd, &rescuers_data[id].lock);
                printf("risvegliato (%d,%s)\n",rescuers_data[id].twin->id,rescuers_data[id].twin->rescuer->rescuer_type_name);
            }
            if(!MESSAGE_QUEUE_ACTIVE) break;
            char id_log[64];
            snprintf(id_log, sizeof(id_log), "(%d,%s)", id+1, rescuers_data[id].twin->rescuer->rescuer_type_name);
            write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da IDLE a EN_ROUTE_TO_SCENE");
            printf("[SOCCORRITORE:%s;N°:%d] ATTIVATO PER L'EMERGENZA (%d,%s)\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,rescuers_data[id].id_current_emergency,rescuers_data[id].current_emergency);
            
            //calcolo la distanza 
            int from_x = rescuers_data[id].twin->x;
            int to_x = rescuers_data[id].x_emergency;
            int from_y = rescuers_data[id].twin->y;
            int to_y = rescuers_data[id].y_emergency;

            int distance = distance_manhattan(from_x, to_x, from_y,to_y, rescuers_data[id].twin->rescuer->speed);
           
            printf("[(%s, %d) TEMPO STIMATO DI ARRIVO IN SECONDI: %d]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,distance);
            while((rescuers_data[id].twin->x != to_x || rescuers_data[id].twin->y != to_y)){
                if(!MESSAGE_QUEUE_ACTIVE) break;
                thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
                int dx = to_x - rescuers_data[id].twin->x;
                int dy = to_y - rescuers_data[id].twin->y;
                double tot_steps = rescuers_data[id].twin->rescuer->speed;

                if(dx != 0){
                    int verse = (dx > 0) ? 1 : -1;
                    double step = (abs(dx) < tot_steps) ? abs(dx) : ceil(tot_steps/2);
                    rescuers_data[id].twin->x +=  step * verse;
                    tot_steps = tot_steps - ceil(tot_steps/2);
                }
                
                if(dy != 0){
                    int verse = (dy > 0) ? 1 : -1;
                    int step = (abs(dy) < tot_steps) ? abs(dy) : tot_steps;
                    rescuers_data[id].twin->y +=  step * verse;
                }
            }

            atomic_int* rescuers_finished = &queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished;
            atomic_int* tot_rescuers_required = &queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required;
            mtx_t* lock = &queue_emergencies[rescuers_data[id].id_current_emergency]->lock_emergency;
            cnd_t* cnd = &queue_emergencies[rescuers_data[id].id_current_emergency]->cnd_emergency;
           
            barrier_rescuers(queue_emergencies[rescuers_data[id].id_current_emergency],rescuers_finished, tot_rescuers_required, lock, cnd, MESSAGE_QUEUE_ACTIVE); 
             if(!MESSAGE_QUEUE_ACTIVE) break;
            rescuers_data[id].twin->status = ON_SCENE;
            write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da EN_ROUTE_TO_SCENE a ON_SCENE");
           
            
            printf("[(%s,%d) ARRIVATO, TEMPO PER IL LAVORO RICHIESTO: %d SEC]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, rescuers_data[id].time_to_manage);
            thrd_sleep(&(struct timespec){.tv_sec = rescuers_data[id].time_to_manage}, NULL);
            
            atomic_fetch_add(&queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished, 1);
            if(queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished == queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required){
                printf("[EMERGENZA: (%d,%s)] TERMINATA\n", queue_emergencies[rescuers_data[id].id_current_emergency]->id, queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->type->emergency_desc);
                queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->status = COMPLETED;
                char id_log[LENGTH_LINE];
                snprintf(id_log, sizeof(id_log), "(%d,%s)",queue_emergencies[rescuers_data[id].id_current_emergency]->id , rescuers_data[id].current_emergency);
                write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da IN_PROGESS a COMPLETED");
            }
            
            rescuers_data[id].twin->status = RETURNING_TO_BASE;
            write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da ON_SCENE a RETURNING_TO_BASE");
            distance = distance_manhattan(to_x, from_x, to_y, from_y, rescuers_data[id].twin->rescuer->speed);
            printf("[(%s,%d) LAVORO TERMINATO, TEMPO: %d SEC PER TORNARE ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, distance);
            
            while((rescuers_data[id].twin->x != to_x || rescuers_data[id].twin->y != to_y)){
                if(!MESSAGE_QUEUE_ACTIVE) break;
                thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
                int dx = to_x - rescuers_data[id].twin->x;
                int dy = to_y - rescuers_data[id].twin->y;
                double tot_steps = rescuers_data[id].twin->rescuer->speed;

                if(dx != 0){
                    int verse = (dx > 0) ? 1 : -1;
                    double step = (abs(dx) < tot_steps) ? abs(dx) : ceil(tot_steps/2);
                    rescuers_data[id].twin->x +=  step * verse;
                    tot_steps = tot_steps - ceil(tot_steps/2);
                }
                
                if(dy != 0){
                    int verse = (dy > 0) ? 1 : -1;
                    int step = (abs(dy) < tot_steps) ? abs(dy) : tot_steps;
                    rescuers_data[id].twin->y +=  step * verse;
                }
            }

            
            printf("[(%s,%d) TORNATO ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1);
            rescuers_data[id].twin->status = IDLE;
            write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da RETURNING_TO_BASE a IDLE");
            mtx_unlock(&rescuers_data[id].lock);
    }
    printf("[RESCUER: (%d,%s) TERMINATO]\n", rescuers_data[id].twin->id+1, rescuers_data[id].twin->rescuer->rescuer_type_name);
    return thrd_success;
}

void free_locks_rescuers(rescuers_t** id_locks, int count){

    for(int i = 0; count > i; i++){
        rescuers_t* tmp = id_locks[i];
        for(int j = 0; tmp->avalaible_resc > j; j++) mtx_unlock(&rescuers_data[tmp->id_arr[j]].lock);
        free(tmp->id_arr);
        free(tmp);
    }

    free(id_locks);
    return;

}

int start_emergency(emergency_id_t* current_emergency){

    char id_log[256];
    snprintf(id_log, sizeof(id_log), "(%d,%s)", current_emergency->id, current_emergency->emergency->type->emergency_desc);

    emergency_t* emergency = current_emergency->emergency;
    current_emergency->in_loading = true;

    rescuers_t** id_locks = (rescuers_t**)malloc(sizeof(rescuers_t*));
    int count_id_locks = 0;
    if(id_locks == NULL) exit(0);
    
    int tot_request_rescuers = 0;
    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        tot_request_rescuers += emergency->type->rescuers[i].required_count;
    }

    int tot_avalaible_rescuers = 0;

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        rescuer_request_t req = emergency->type->rescuers[i];
        short int num_request = 0;
        short int count_id_arr = 0;
        rescuers_t* id_locks_tmp = (rescuers_t*)malloc(sizeof(rescuers_t));
        if(id_locks_tmp == NULL) exit(0);

        id_locks_tmp->id_arr = (int*)malloc(sizeof(int));
        if(id_locks_tmp->id_arr == NULL) exit(0);
        id_locks_tmp->tot_manage = req.required_count * req.time_to_manage;

        for(int j = 0; current_emergency->num_twins > j; j++){
            if(req.type != NULL && strcmp(req.type->rescuer_type_name, emergency->rescuers_dt[j]->rescuer->rescuer_type_name) == 0){
                if(mtx_trylock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock) == thrd_success){
                    if(rescuers_data[emergency->rescuers_dt[j]->id-1].twin->status == IDLE){
                        if(count_id_arr > 0){
                           int* tmp = realloc(id_locks_tmp->id_arr, sizeof(int)*(count_id_arr+1));
                           if(tmp == NULL) exit(0);
                           id_locks_tmp->id_arr = tmp;
                        }
                        num_request++;
                        tot_avalaible_rescuers++;
                        id_locks_tmp->id_arr[count_id_arr] = emergency->rescuers_dt[j]->id-1;
                        id_locks_tmp->avalaible_resc = num_request;
                        count_id_arr++;
                    } else mtx_unlock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock);
                }
            }
            if(num_request == req.required_count) break;
        }
        
        if(!(num_request == req.required_count) && (num_request == 0 || (id_locks_tmp->tot_manage / num_request) > TOLLERANCE)){
            current_emergency->in_loading = false;
            free_locks_rescuers(id_locks, count_id_locks);
            printf("[NON E' STATO POSSIBILE AVVIARE L'EMERGENZA (%d,%s) PER NON DISPONIBILITà DI ALCUNI SOCCORRITORI]\n", current_emergency->id, emergency->type->emergency_desc);
            if(current_emergency->emergency->status != PAUSED){
                printf("[EMERGENZA (%d,%s) AGGIUNTA IN CODA D'ATTESA]\n", current_emergency->id, emergency->type->emergency_desc);
                current_emergency->emergency->status = PAUSED;
                write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da ASSIGNED a PAUSED");
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

        } else {
            if(count_id_locks > 0){
                rescuers_t** tmp = realloc(id_locks, sizeof(rescuers_t*)*(count_id_locks+1));
                if(tmp == NULL) exit(0);
                id_locks = tmp;
            }
            id_locks[count_id_locks] = id_locks_tmp;
            count_id_locks++;
        }
    }

    if(current_emergency->was_in_waiting_queue){
        current_emergency->was_in_waiting_queue = false;
        mtx_lock(&lock_operation_on_waiting_queue);
        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
        int val;
        sem_getvalue(&sem_waiting_queue,&val);
        if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
        mtx_unlock(&lock_operation_on_waiting_queue);
    }  

    current_emergency->emergency->status = WAITING;
    write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da WAITING a TIMEOUT");
    current_emergency->tot_rescuers_required = tot_avalaible_rescuers;
    current_emergency->rescuers_finished = 0;
        

    if(get_priority_limit(emergency->type->priority) != 0){
        for(int i = 0; count_id_locks > i; i++){
            rescuers_t* tmp = id_locks[i];
            for(int j = 0; tmp->avalaible_resc > j; j++){
                int id = tmp->id_arr[j];
                if(distance_manhattan(rescuers_data[id].twin->x, emergency->x, rescuers_data[id].twin->y, emergency->y, rescuers_data[id].twin->rescuer->speed) > get_priority_limit(emergency->type->priority) - (time(NULL) - emergency->time)){
                    free_locks_rescuers(id_locks, count_id_locks);
                    printf("[L'EMERGENZA (%d,%s) NON E' STATA ESEGUITA A CAUSA DI DISTANZA/PROPRITA PER IL SOCCORRITORE: %s']\n",current_emergency->id ,emergency->type->emergency_desc, rescuers_data[id].twin->rescuer->rescuer_type_name);
                    mtx_lock(&lock_operation_on_waiting_queue);
                    if(emergency->status == PAUSED){
                        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                    }
                    mtx_unlock(&lock_operation_on_waiting_queue);
                    emergency->status = TIMEOUT;
                    write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da WAITING a TIMEOUT");
                    return -2;
                }
            }  
        }
    }


    for(int i = 0; count_id_locks > i; i++){
        rescuers_t* tmp = id_locks[i];
        for(int j = 0; tmp->avalaible_resc > j; j++){
            int id = tmp->id_arr[j];
            rescuers_data[id].current_emergency = (char*)malloc(sizeof(char)*LENGTH_LINE);
            if(rescuers_data[id].current_emergency == NULL){
                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                exit(MALLOC_ERROR);
            }
            strcpy(rescuers_data[id].current_emergency, emergency->type->emergency_desc);
            rescuers_data[id].id_current_emergency = current_emergency->id;
            rescuers_data[id].x_emergency = emergency->x;
            rescuers_data[id].y_emergency = emergency->y;
            rescuers_data[id].time_to_manage = tmp->tot_manage / tmp->avalaible_resc;
            rescuers_data[id].twin->status = EN_ROUTE_TO_SCENE;
            cnd_signal(&rescuers_data[id].cnd);
            mtx_unlock(&rescuers_data[id].lock);
        }
    }

    char rescuers[256];
    for(int i = 0; count_id_locks > i; i++){
        if(i == 0){ strcpy(rescuers,rescuers_data[id_locks[i]->id_arr[0]].twin->rescuer->rescuer_type_name);}
        else {
            strcat(rescuers, ";");
            strcat(rescuers, rescuers_data[id_locks[i]->id_arr[0]].twin->rescuer->rescuer_type_name);
        }
        free(id_locks[i]->id_arr);
        free(id_locks[i]);
    }
    free(id_locks);
    write_log_file(time(NULL), id_log, EMERGENCY_OPERATION, rescuers);

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
        char* desc_error = "emergenza non gestibile per mancanza di risorse";
        write_log_file(time_now, emergency->type->emergency_desc, EMERGENCY_STATUS, desc_error);
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

    char id_log[LENGTH_LINE];
    snprintf(id_log, sizeof(id_log), "(%d,%s) da %s a %s", current_emergency->id, current_emergency->emergency->type->emergency_desc, "WAITING", "IN_PROGRESS");
    write_log_file(time(NULL),id_log, EMERGENCY_STATUS, "Emergenza passata nello stato ASSIGNED");


    
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

        if(strcmp(request->emergency_name, "STOP") == 0){
            printf("[MESSAGE QUEUE TERMINATA]\n");
            break;
        }

        char* message = (char*)malloc(sizeof(char)*LENGTH_LINE);
        if(message == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
       
        int index_emergency = -1;
        if(!is_valid_request(request, params->environment->x, params->environment->y, message, &index_emergency, emergency_avalaible, num_emergency_avalaible)){
            printf("richiesta non valida!\n");
            strcpy(request->emergency_name, "N/A");
            write_log_file(time(NULL), request->emergency_name, MESSAGE_QUEUE, message);
            continue;
        }
        snprintf(message, LENGTH_LINE, "[%s (%d,%d) %ld]", request->emergency_name, request->x, request->y, request->delay);
        write_log_file(time(NULL), request->emergency_name, MESSAGE_QUEUE, message);

        params_handler_emergency_t* params_handler_emergency = (params_handler_emergency_t*)malloc(sizeof(params_handler_emergency_t));
        if(params_handler_emergency == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        params_handler_emergency->index_emergencies = index_emergency;
        params_handler_emergency->x = request->x;
        params_handler_emergency->y = request->y;
        params_handler_emergency->params = params;
        params_handler_emergency->timestamp = request->delay;
        
        thrd_t thread_handler_emergency;

        thrd_create(&thread_handler_emergency, handler_emergency, params_handler_emergency);
        
    }

    MESSAGE_QUEUE_ACTIVE = false;

    mq_close(message_queue);
    mq_unlink(mq_name);

    return 1;
}


int main(){

    mtx_init(&lock_operation_on_waiting_queue, mtx_plain);
    mtx_init(&lock_operation_on_queue_emergency, mtx_plain);
    sem_init(&sem_waiting_queue, 0, 0);

    thrd_t handler_queue, handler_waiting_queue_t,a;
    
    env_t* environment = parser_env(ENVIRONMENT_FILENAME);
    
    num_emergency_avalaible = 0;
    emergency_avalaible = parser_emergency(EMERGENCY_FILENAME, &num_emergency_avalaible);
    result_parser_rescuers* rp_rescuers = parse_rescuers(RESCUERS_FILENAME);
    
    int num_twins = rp_rescuers->num_twins;
    thrd_t rescuers_active[num_twins];

    params_handler_queue_t* params = (params_handler_queue_t*)malloc(sizeof(params_handler_queue_t));
    if(params == NULL) exit(0);

    params->environment = environment;
    params->rp_rescuers = rp_rescuers;

    rescuers_data = (rescuer_data_t*)malloc(sizeof(rescuer_data_t)*num_twins);
    if(rescuers_data == NULL) exit(0);

    for(int i = 0; num_twins > i; i++){
        rescuers_data[i].twin = rp_rescuers->rd_twins[i];
        mtx_init(&rescuers_data[i].lock, mtx_plain);
        cnd_init(&rescuers_data[i].cnd);
        rescuers_data[i].time_to_manage = 0;
        thrd_create(&rescuers_active[i], handle_rescuer, &rp_rescuers->rd_twins[i]->id);
    }

    if(thrd_create(&handler_waiting_queue_t, handler_waiting_queue, NULL) == thrd_success){
        thrd_detach(handler_waiting_queue_t);
    } else {
        //ERROR
    }

    thrd_create(&handler_queue, handler_queue_emergency, params);
    thrd_create(&a, print_state_digital_rescuer, rp_rescuers);
    thrd_join(handler_queue, NULL);
    
    for(int i = 0; num_twins> i; i++){
        if(rescuers_data[i].twin->status == IDLE){
            rescuers_data[i].twin->status = EN_ROUTE_TO_SCENE;
            cnd_signal(&rescuers_data[i].cnd);
            mtx_unlock(&rescuers_data[i].lock);
        }
    }

    for(int i = 0; num_twins> i; i++){
        thrd_join(rescuers_active[i], NULL);
    }
    
    mtx_destroy(&lock_operation_on_waiting_queue);
    mtx_destroy(&lock_operation_on_queue_emergency);
    sem_destroy(&sem_waiting_queue);

    free_rescuers(rp_rescuers->rescuers_type, rp_rescuers->num_rescuers);
    free_emergency_avalaible(emergency_avalaible, num_emergency_avalaible);
    free_rescuers_digital_twins(rp_rescuers->rd_twins, rp_rescuers->num_twins);
    free_queue_emergencies(queue_emergencies, id_emergencies);
    free_waiting_queue(waiting_queue, waiting_queue_len);
    free_rescuers_data(rescuers_data, num_twins);
    free(rp_rescuers);
    free(params->environment->queue_name);
    free(params->environment);
    free(params);

    

    return 0;
}
