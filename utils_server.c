#include "server.h"


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

void free_queue_emergencies(emergency_id_t** queue_emergencies, int num){

    if(queue_emergencies == NULL) return;

    for(int i = 0; num > i; i++){
        cnd_destroy(&queue_emergencies[i]->cnd_emergency);
        mtx_destroy(&queue_emergencies[i]->lock_emergency);
        free(queue_emergencies[i]);
        queue_emergencies[i] = NULL;
    }

    free(queue_emergencies);
    printf("[GESTORE DELLA MEMORIA: QUEUE EMERGENCIES LIBERATA]\n");
    return;

}

int print_state_digital_rescuer(void* args){
    result_parser_rescuers* r = (result_parser_rescuers*)args;
    while(1){
        if(!MESSAGE_QUEUE) break;
        thrd_sleep(&(struct timespec){.tv_sec = 4}, NULL);
        print_digitals_twins(r->rd_twins, r->num_twins);
        printf("\n\n");
    }
}

void free_rescuers_data(rescuer_data_t* rd, int num){

    if(rd == NULL) return;

    for(int i = 0; num > i; i++){
        mtx_destroy(&rd[i].lock);
        cnd_destroy(&rd[i].cnd);
        free(rd[i].current_emergency);
        rd[i].current_emergency = NULL;
    }

    free(rd);
    rd = NULL;
    printf("[GESTORE DELLA MEMORIA: RESCUERS_DATA LIBERATI]\n");
    return;
}

void free_waiting_queue(waiting_queue_t** waiting_queue, int num){

    if(waiting_queue == NULL) return;

    for(int i = 0; num > i; i++){
        free(waiting_queue[i]->desc);
        free(waiting_queue[i]);
    }

    free(waiting_queue);
    printf("[GESTORE DELLA MEMORIA: WAITING QUEUE LIBERATA]\n");
    return;

}

int barrier_rescuers(emergency_id_t* current ,atomic_int* count, atomic_int* tot_rescuers_required, mtx_t* mtx, cnd_t* cnd, bool is_active) {
	mtx_lock(mtx);

    if(!is_active){
        cnd_broadcast(cnd);
		mtx_unlock(mtx);
        return 0;
    }

    (*count)++;
	
	if (*count == *tot_rescuers_required) {
		*count = 0;
		cnd_broadcast(cnd);
		mtx_unlock(mtx);
        current->emergency->status = IN_PROGRESS;
        char desc[LENGTH_LINE];
        snprintf(desc, sizeof(desc), "(%d,%s) da %s a %s", current->id, current->emergency->type->emergency_desc, "WAITING", "IN_PROGRESS");
        write_log_file(time(NULL), desc, EMERGENCY_STATUS, "Transizione da WAITING a IN_PROGRESS");
		return 1;
	} else {
		cnd_wait(cnd, mtx);
		mtx_unlock(mtx);
		return 0;
	}
}



int get_priority_limit(int priority){

    if(priority < 0 || priority > 2) return -1;
    
    int seconds = -1;

    switch(priority){
        case 0: seconds = 0; break;
        case 1: seconds = 30; break;
        case 2: seconds = 10; break;
    }

    return seconds;

}

int distance_manhattan(int x1, int x2, int y1, int y2, int speed){
    
    return (abs(x1 - x2) + abs(y1 - y2)) / speed;
    
}


int rescuers_is_avalaible(rescuer_digital_twin_t** rd_twins, int num_twins, rescuer_request_t* requests, int req_number, char* desc){

    bool whitout = false;
    for(int i = 0; req_number > i; i++){
        if(requests[i].type == NULL){
            printf("non è possibile soddisfare l'emergenza: %s per mancanza di soccorritori necessari\n", desc);
            return -1;
        }
        int counter_resc = 0;
        for(int j = 0; num_twins > j; j++){
            if(strcmp(rd_twins[j]->rescuer->rescuer_type_name, requests[i].type->rescuer_type_name) == 0) counter_resc++;
            if(counter_resc == requests[i].required_count) break;
        }
        if(counter_resc == 0) return -1;
        else if(counter_resc < requests[i].required_count){
            whitout = true;
        } 
    }

    return (whitout) ? 0 : 1;

}

int compare(const void *a, const void *b) {
    waiting_queue_t* em1 = *(waiting_queue_t**)a;
    waiting_queue_t* em2 = *(waiting_queue_t**)b;

    if (em1->priority < em2->priority) return 1;
    else if (em1->priority > em2->priority) return -1;
    else return 0;
}

void add_waiting_queue(emergency_id_t* id, waiting_queue_t*** waiting_queue, int* waiting_queue_len){
    
    if (*waiting_queue == NULL){
        *waiting_queue = (waiting_queue_t**)malloc(sizeof(waiting_queue_t*));
        if (*waiting_queue == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
            exit(MALLOC_ERROR);
        }
    } else {
        for (int i = 0; i < *waiting_queue_len; i++) {
            if ((*waiting_queue)[i]->id == id->id &&
                strcmp((*waiting_queue)[i]->desc, id->emergency->type->emergency_desc) == 0) {
                return;  
            }
        }
        waiting_queue_t** tmp = realloc(*waiting_queue, sizeof(waiting_queue_t*) * (*waiting_queue_len + 1));
        if (tmp == NULL) {
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
            exit(MALLOC_ERROR);
        }
        *waiting_queue = tmp;
    }

    (*waiting_queue)[*waiting_queue_len] = malloc(sizeof(waiting_queue_t));
    if ((*waiting_queue)[*waiting_queue_len] == NULL) {
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
        exit(MALLOC_ERROR);
    }

    (*waiting_queue)[*waiting_queue_len]->desc = malloc(sizeof(char) * LENGTH_LINE);
    if ((*waiting_queue)[*waiting_queue_len]->desc == NULL) {
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
        exit(MALLOC_ERROR);
    }

    strcpy((*waiting_queue)[*waiting_queue_len]->desc, id->emergency->type->emergency_desc);
    (*waiting_queue)[*waiting_queue_len]->id = id->id;
    (*waiting_queue)[*waiting_queue_len]->priority = id->emergency->type->priority;

    (*waiting_queue_len)++;

    qsort(*waiting_queue, *waiting_queue_len, sizeof(waiting_queue_t*), compare);
}

void remove_from_waiting_queue(emergency_id_t* emergency, waiting_queue_t*** waiting_queue, int* waiting_queue_len) {
    if (*waiting_queue == NULL || *waiting_queue_len <= 0) return;

    if (*waiting_queue_len == 1 && emergency->id == (*waiting_queue)[0]->id) {
        printf("[(%d,%s) ELIMINATA DALLA CODA DI ATTESA]\n", emergency->id, (*waiting_queue)[0]->desc);
        free((*waiting_queue)[0]->desc);
        free((*waiting_queue)[0]);
        free(*waiting_queue);
        *waiting_queue = NULL;
        (*waiting_queue_len)--;
        return;
    }

    int index = -1;
    for (int i = 0; i < *waiting_queue_len; i++) {
        if ((*waiting_queue)[i]->id == emergency->id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        printf("[L'EMERGENZA (%d,%s) NON ERA IN CODA D'ATTESA]\n", emergency->id, emergency->emergency->type->emergency_desc);
        return;
    }

    char temp_desc[LENGTH_LINE];
    strcpy(temp_desc, (*waiting_queue)[index]->desc);

    free((*waiting_queue)[index]->desc);
    free((*waiting_queue)[index]);

    for (int i = index; i < *waiting_queue_len - 1; i++) {
        (*waiting_queue)[i] = (*waiting_queue)[i + 1];
    }

    (*waiting_queue)[*waiting_queue_len - 1] = NULL;
    (*waiting_queue_len)--;

    // Ridimensiona la coda
    if (*waiting_queue_len > 0) {
        waiting_queue_t** tmp = realloc(*waiting_queue, sizeof(waiting_queue_t*) * (*waiting_queue_len));
        if (tmp != NULL) {
            *waiting_queue = tmp;
        }
    } else {
        free(*waiting_queue);
        *waiting_queue = NULL;
    }

    printf("[(%d,%s) ELIMINATA DALLA CODA DI ATTESA]\n", emergency->id, temp_desc);
}


emergency_t* set_new_emergency(params_handler_emergency_t* params_emergency){

    emergency_t* emergency = (emergency_t*)malloc(sizeof(emergency_t));
    if(emergency == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    int index = params_emergency->index_emergencies;
    int x = params_emergency->x;
    int y = params_emergency->y;

    emergency->time = params_emergency->timestamp;
    emergency->type = (emergency_type_t*)malloc(sizeof(emergency_type_t));

    if(emergency->type == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }
    emergency->type->emergency_desc = (char*)malloc(sizeof(char)*LENGTH_LINE);
    if(emergency->type->emergency_desc == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    emergency->type->emergency_desc = strdup(params_emergency->emergency_avalaible[index].emergency_desc);
    emergency->type->priority = params_emergency->emergency_avalaible[index].priority;
    
    emergency->type->rescuers_req_number = params_emergency->emergency_avalaible[index].rescuers_req_number;
    emergency->type->rescuers = (rescuer_request_t*)malloc(sizeof(rescuer_request_t)*emergency->type->rescuers_req_number);
    
    if(emergency->type->rescuers == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        if(params_emergency->emergency_avalaible[index].rescuers[i].type != NULL){
            emergency->type->rescuers[i].required_count = params_emergency->emergency_avalaible[index].rescuers[i].required_count;
            emergency->type->rescuers[i].time_to_manage = params_emergency->emergency_avalaible[index].rescuers[i].time_to_manage;
            emergency->type->rescuers[i].type = params_emergency->emergency_avalaible[index].rescuers[i].type;
        }
    }

    emergency->x = x;
    emergency->y = y;

    emergency->status = ASSIGNED;
    
    return emergency;

}


emergency_id_t* add_emergency(int* id, emergency_t* emergency, emergency_id_t*** queue_emergencies){

    emergency_id_t* new_emergency = (emergency_id_t*)malloc(sizeof(emergency_id_t));
    if(new_emergency == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    new_emergency->id = *id;
    new_emergency->emergency = emergency;

    if(*queue_emergencies == NULL) {
        *queue_emergencies = (emergency_id_t**)malloc(sizeof(emergency_id_t*));
    } else {
        emergency_id_t** tmp = realloc(*queue_emergencies, sizeof(emergency_id_t*)*((*id)+1));
        if(tmp == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
            exit(MALLOC_ERROR);
        }
        *queue_emergencies = tmp;
    }

    (*queue_emergencies)[*id] = new_emergency;
    (*id)++; 

    return new_emergency;

}

int is_valid_request(emergency_request_t* request, int width, int height, char* problem, int* index_type_emergency, emergency_type_t* emergency_avalaible, int num_emergency_avalaible){

    bool is_valid = true;

    if(request == NULL) {
        strcpy(problem, "request = NULL;");
        is_valid = false;
    } else if((request->x < 0 || request->x >= width) || (request->y < 0 || request->y >= height)){
        if(is_valid) {
            strcpy(problem, "overflow coordinate;");
            is_valid = false;
        } else strcat(problem, "overflow coordinate;");
    }


    
    for(int i = 0; num_emergency_avalaible > i; i++){
        if(strcmp(request->emergency_name, emergency_avalaible[i].emergency_desc) == 0){
            *index_type_emergency = i;
            break;
        }
        else if(i == num_emergency_avalaible - 1){
            if(is_valid){
                strcpy(problem, "questa emergenza non è presente nel file emergency.conf;");
                is_valid = false;
            } else strcat(problem, "questa emergenza non è presente nel file emergency.conf");
        }
    }

    return (is_valid == true) ? 1 : 0;
    
}


int check_priority_waiting_queue(int current_index, waiting_queue_t*** waiting_queue, int waiting_queue_len, emergency_id_t*** queue_emergencies){

    if(current_index >= waiting_queue_len || !(waiting_queue_len > 1)) return -1;
    if((*waiting_queue)[current_index] == NULL) return -1;
    int id = (*waiting_queue)[current_index]->id;
    if (!(*queue_emergencies)[id] || !(*queue_emergencies)[id]->emergency || !(*queue_emergencies)[id]->emergency->type) return -1;

    int current_priority = (*queue_emergencies)[id]->emergency->type->priority;
    char curr_name[LENGTH_LINE];
    strcpy(curr_name, (*queue_emergencies)[id]->emergency->type->emergency_desc);
   
    for(int i = current_index; waiting_queue_len - 1 > i; i++){
        if(current_priority == (*queue_emergencies)[(*waiting_queue)[i+1]->id]->emergency->type->priority &&
        strcmp(curr_name, (*queue_emergencies)[(*waiting_queue)[i+1]->id]->emergency->type->emergency_desc) != 0){
            printf("c'è un emergenza (%d,%s) che può essere avviata con priorità uguale\n",(*waiting_queue)[i+1]->id, (*waiting_queue)[i+1]->desc);
            return i+1;
        }
    }
    return -1;
    
}
