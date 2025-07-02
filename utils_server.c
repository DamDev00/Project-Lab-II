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

    /*
        is_active verifica se la message queue è ancora operativa, se non lo è
        risveglia tutti i soccorritori che erano nello stato wait per poi terminare
        la loro esecuzione dato che dopo la funzione c'è 'if(!MESSAGE_QUEUE_ACTIVE) break;'
    */

    if(!is_active){
        cnd_broadcast(cnd);
		mtx_unlock(mtx);
        return 0;
    }

    // Aggiorno il numero di soccorritori che sono arrivati

    (*count)++;
	
    /*
        se sono arrivati tutti i soccorritori, l'ultimo che è arrivato risveglia tutti gli altri
        modificando lo stato dell'emergenza riportandolo sul file.log
    */

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

    /*
        verifico che tutti i tipi di soccorritori richiesti per l'emergenza ci siano
    */

    bool mancamenti = false;
    for(int i = 0; req_number > i; i++){
        if(requests[i].type == NULL){
            printf("non è possibile soddisfare l'emergenza: %s per mancanza di soccorritori necessari\n", desc);
            return -1;
        }
        int counter_resc = 0;

        /*
            verifico quanti soccorritori ci sono per un determinato tipo: se ci sono tutti mi fermo prima,
            se non c'è n'è nessuno ritorno -1 segnalando che l'emergenza non è fattibile
        */

        for(int j = 0; num_twins > j; j++){
            if(strcmp(rd_twins[j]->rescuer->rescuer_type_name, requests[i].type->rescuer_type_name) == 0) counter_resc++;
            if(counter_resc == requests[i].required_count) break;
        }
        if(counter_resc == 0) return -1;
        else if(counter_resc < requests[i].required_count){
            mancamenti = true;
        } 
    }

    return (mancamenti) ? 0 : 1;

}

int compare(const void *a, const void *b) {

    /*
        compare che utilizzo per ordinare la waiting queue in base alla priorità
    */

    waiting_queue_t* em1 = *(waiting_queue_t**)a;
    waiting_queue_t* em2 = *(waiting_queue_t**)b;

    if (em1->priority < em2->priority) return 1;
    else if (em1->priority > em2->priority) return -1;
    else return 0;
}

void add_waiting_queue(emergency_id_t* id, waiting_queue_t*** waiting_queue, int* waiting_queue_len){
    
    /*
        se la coda è vuota, alloco memoria. Se non è vuota, prima di ri-allocare memoria
        verifico che l'emergenza non sia in coda
    */
    
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

    // alloco memoria per l'emergenza corrente da aggiungere in coda

    (*waiting_queue)[*waiting_queue_len] = malloc(sizeof(waiting_queue_t));
    if ((*waiting_queue)[*waiting_queue_len] == NULL) {
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
        exit(MALLOC_ERROR);
    }

    // alloco memoria per la descrizione dell'emergenza

    (*waiting_queue)[*waiting_queue_len]->desc = malloc(sizeof(char) * LENGTH_LINE);
    if ((*waiting_queue)[*waiting_queue_len]->desc == NULL) {
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n", __LINE__, __FILE__);
        exit(MALLOC_ERROR);
    }

    // inizializzo i parametri dell'emergenza corrente aggiunta in coda

    strcpy((*waiting_queue)[*waiting_queue_len]->desc, id->emergency->type->emergency_desc);
    (*waiting_queue)[*waiting_queue_len]->id = id->id;
    (*waiting_queue)[*waiting_queue_len]->priority = id->emergency->type->priority;

    (*waiting_queue_len)++;

    // ordino la coda in base alla priorità

    qsort(*waiting_queue, *waiting_queue_len, sizeof(waiting_queue_t*), compare);

    return;
}

void remove_from_waiting_queue(emergency_id_t* emergency, waiting_queue_t*** waiting_queue, int* waiting_queue_len) {

    // verifico che la coda non sia vuota

    if (*waiting_queue == NULL || *waiting_queue_len <= 0) return;

    // controllo subito se la coda ha solamente un emergenza ed è quella passata per parametro

    if (*waiting_queue_len == 1 && emergency->id == (*waiting_queue)[0]->id) {
        printf("[(%d,%s) ELIMINATA DALLA CODA DI ATTESA]\n", emergency->id, (*waiting_queue)[0]->desc);
        free((*waiting_queue)[0]->desc);
        free((*waiting_queue)[0]);
        free(*waiting_queue);
        *waiting_queue = NULL;
        (*waiting_queue_len)--;
        return;
    }

    /*
        scorro la coda per trovare l'id dell'emergenza da eliminare
        salvando l'indice
    */

    int index = -1;
    for (int i = 0; i < *waiting_queue_len; i++) {
        if ((*waiting_queue)[i]->id == emergency->id) {
            index = i;
            break;
        }
    }

    // se rimane -1, l'emergenza non è in coda d'attesa

    if (index == -1) {
        printf("[L'EMERGENZA (%d,%s) NON ERA IN CODA D'ATTESA]\n", emergency->id, emergency->emergency->type->emergency_desc);
        return;
    }

    /*
        mi salvo il nome dell'emergenza per segnalare, alla fine della funzione, che l'emergenza è
        stata rimossa dalla coda d'attesa
    */

    char temp_desc[LENGTH_LINE];
    strcpy(temp_desc, (*waiting_queue)[index]->desc);

    // libero la memoria

    free((*waiting_queue)[index]->desc);
    free((*waiting_queue)[index]);

    /*
        sovrascrivo l'emergenza i-esima con la i+1-esima per poter eliminare l'ultimo elemento
        diminuendo la grandezza della coda
    */

    for (int i = index; i < *waiting_queue_len - 1; i++) {
        (*waiting_queue)[i] = (*waiting_queue)[i + 1];
    }

    (*waiting_queue)[*waiting_queue_len - 1] = NULL;
    (*waiting_queue_len)--;

    // ridimensiono la coda d'attesa di -1

    if (*waiting_queue_len > 0) {
        waiting_queue_t** tmp = realloc(*waiting_queue, sizeof(waiting_queue_t*)*(*waiting_queue_len));
        if (tmp != NULL) {
            *waiting_queue = tmp;
        }
    } else {
        free(*waiting_queue);
        *waiting_queue = NULL;
    }

    printf("[(%d,%s) ELIMINATA DALLA CODA DI ATTESA]\n", emergency->id, temp_desc);
    return;

}


emergency_t* set_new_emergency(params_handler_emergency_t* params_emergency, int* twins){

    // alloco memoria per l'output (emergency)

    emergency_t* emergency = (emergency_t*)malloc(sizeof(emergency_t));
    if(emergency == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /*
        inizializzo tutti i campi per il tipo emergency_t
    */

    int index = params_emergency->index_emergencies;
    emergency->x = params_emergency->x;
    emergency->y = params_emergency->y;
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

    /*
        imposto i soccorritori necessari richiesti
    */

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        if(params_emergency->emergency_avalaible[index].rescuers[i].type != NULL){
            emergency->type->rescuers[i].required_count = params_emergency->emergency_avalaible[index].rescuers[i].required_count;
            emergency->type->rescuers[i].time_to_manage = params_emergency->emergency_avalaible[index].rescuers[i].time_to_manage;
            emergency->type->rescuers[i].type = params_emergency->emergency_avalaible[index].rescuers[i].type;
        }
    }

    // alloco memoria per i gemelli digitali

    emergency->rescuers_dt = (rescuer_digital_twin_t**)malloc(sizeof(rescuer_digital_twin_t*));
    if(emergency->rescuers_dt == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /*
        in questo ciclo in sostanza, nell'array emergency->rescuers_dt, vado ad aggiungere più possibili
        soccorritori necessari per ogni tipo, cosa da avere una panoramica al 100% per capire chi posso avviare
        e chi no. Ad esempio, se ho [Allagamento][<int>]Pompieri: 4,5 e nel file.rescuers ho [Pompieri][10] non vado
        ad assergnare solamente 4 istanze, ma tutte e 10 le disponibili.
    */
    

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        if(params_emergency->emergency_avalaible[index].rescuers[i].type != NULL){

            // i-esimo soccorritore richiesto

            rescuer_request_t req = params_emergency->emergency_avalaible[index].rescuers[i];

            for(int j = 0; params_emergency->params->rp_rescuers->num_twins > j; j++){
                // verifico se è lui attraverso il nome

                if(strcmp(req.type->rescuer_type_name, params_emergency->params->rp_rescuers->rd_twins[j]->rescuer->rescuer_type_name) == 0){
                    
                    // lo aggiungo nell'array e ri-alloco memoria di +1
                    
                    emergency->rescuers_dt[*twins] = params_emergency->params->rp_rescuers->rd_twins[j];
                    (*twins)++;
                    rescuer_digital_twin_t** tmp = realloc(emergency->rescuers_dt, sizeof(rescuer_digital_twin_t*)*((*twins)+1));
                    if(tmp == NULL){
                        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                        exit(MALLOC_ERROR);
                    }
                    emergency->rescuers_dt = tmp;
                }
            }
        }
    }

    emergency->status = ASSIGNED;
    
    return emergency;

}


emergency_id_t* add_emergency(int* id, emergency_t* emergency, emergency_id_t*** queue_emergencies){

    /*
        inizializzo il campo 'emergency', l'id, e lo aggiungo nell'array delle emergenze
    */

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

    /*
        qui verifico che l'emergenza sia valida per: nome e coordinate;
        per ogni tipo di problema, assegno alla variabile 'problem' una stringa
        che descrive il tipo di problema.

        La variabile is_valid determina il ritorno della funzione
    */
    
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

    if(!is_valid) return 0;
    
    for(int i = 0; num_emergency_avalaible > i; i++){

        // se l'emergenza ci sta, allora fermo il ciclo e mi salvo l'indice
        if(strcmp(request->emergency_name, emergency_avalaible[i].emergency_desc) == 0){
            *index_type_emergency = i;
            break;
        }
        /*
            se sono arrivato qui, nell'ultima iterazione del ciclo, significa che non è stata trovata
            l'emergenza da soddisfare, per cui salvo il tipo di problema e imposto la variabile a false
        */
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

    /*
        verifico che la coda d'attesa abbia più di un elemento e che l'indice corrente non 
        sia maggiore o uguale alla lunghezza della coda d'attesa
    */

    if(current_index >= waiting_queue_len || !(waiting_queue_len > 1)) return -1;
    
    // verifico se esiste l'emergenza nell'indice corrente
    
    if((*waiting_queue)[current_index] == NULL) return -1;
    
    // mi salvo l'id dell'emergenza in attesa nell'indice corrente

    int id = (*waiting_queue)[current_index]->id;

    // verifico che esista questo id nell'array globale delle emergenze

    if (!(*queue_emergencies)[id] || !(*queue_emergencies)[id]->emergency || !(*queue_emergencies)[id]->emergency->type) return -1;

    // mi salvo la priorità dell'emergenza e il nome 
    int current_priority = (*queue_emergencies)[id]->emergency->type->priority;

    /*
        se ha priorità 0 significa che si trova in fondo alla coda, per cui non importa
        preoccuparsi di altre emergenze con priorità 0, riparto direttamente dalla prima nella coda d'attesa
    */

    if(current_priority == 0) return -1;

    char curr_name[LENGTH_LINE];
    strcpy(curr_name, (*queue_emergencies)[id]->emergency->type->emergency_desc);
   
    /*
        verifico che ci siaun emergenza diversa con priorità uguale
    */
   
    for(int i = current_index; waiting_queue_len - 1 > i; i++){
        if(current_priority == (*queue_emergencies)[(*waiting_queue)[i+1]->id]->emergency->type->priority &&
        strcmp(curr_name, (*queue_emergencies)[(*waiting_queue)[i+1]->id]->emergency->type->emergency_desc) != 0){
            printf("[L'EMERGENZA (%d,%s) CON PRIORITA' UGUALE A QUELLA CORRENTE POTREBBE ESSERE AVVIATA]\n",(*waiting_queue)[i+1]->id, (*waiting_queue)[i+1]->desc);
            return i+1;
        }
    }
    return -1;
    
}
