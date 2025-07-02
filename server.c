#include "server.h"
#include <assert.h>
#include <math.h>

// inizializzo a NULL la griglia degli id delle emergenze

int** field_emergency = NULL;

// variabile che indica se la message queue è ancora operativa

bool MESSAGE_QUEUE_ACTIVE = true;

// array delle emergenze disponibili prelevate dal file emergency.conf 

int num_emergency_avalaible;
emergency_type_t* emergency_avalaible = NULL;

// inizializzo la coda d'attesa con la rispettiva variabile intera che indica la lunghezza

int waiting_queue_len = 0;
waiting_queue_t** waiting_queue = NULL;

// coda delle emergenze totali, che siano valide, non valide, e non soddisfacibili

int id_emergencies = 0;
emergency_id_t** queue_emergencies = NULL;

// struttura aggiuntiva di sopporto per l'esecuzione dei gemelli digitali

rescuer_data_t* rescuers_data = NULL;

/*
    locks che permettono di fare operazioni in modo sicuro nella coda d'attesa,
    nell'array delle emergenze totali e sulla griglia 
*/

mtx_t lock_operation_on_waiting_queue;
mtx_t lock_operation_on_queue_emergency;
mtx_t lock_field;

// semaforo locale utilizzato per le emergenze in attesa

sem_t sem_waiting_queue;


int control_waiting_queue(void* args){

    emergency_id_t* current_emergency = NULL;

    /*
        array di caratteri che utilizzo nel ciclo più interno per non dover
        fare lo stesso tipo di soccorritore
    */

    char curr_twin[LENGTH_LINE];
    printf("[ATTIVATA LA FUNZIONE <control_waiting_queue>]\n");

    while(MESSAGE_QUEUE_ACTIVE){
        /*
            faccio un doppio controllo per verificare che la message queue sia
            ancora attiva.
            Qui e dentro al ciclo
        */
        if(!MESSAGE_QUEUE_ACTIVE) break;
        thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
        printf("[CONTROLLO EMERGENZE IN WAITING_QUEUE]\n");
        mtx_lock(&lock_operation_on_waiting_queue);

        /*
            faccio quest'operazione solo se c'è più di un elemento nella message queue
        */

        if(waiting_queue_len > 1){

            for(int i = 0; waiting_queue_len > i; i++){
                current_emergency = queue_emergencies[waiting_queue[i]->id];

                /*
                    le emergenze con priorità 0 non le considero
                */

                if(current_emergency->emergency->type->priority == 0) continue;

                for(int j = 0; current_emergency->num_twins > j; j++){

                    if(!MESSAGE_QUEUE_ACTIVE) break;

                    // prendo il soccorritore corrente

                    rescuer_digital_twin_t* twin = current_emergency->emergency->rescuers_dt[j];
                    
                    /* 
                        se il soccorritore corrente è uguale al soccorritore precedente vado al ciclo successivo,
                        inutile controllare la distanza dello stesso soccorritore in quanto 
                        produrrebbe soltanto overhead 
                    */

                    if(j > 0 && strcmp(twin->rescuer->rescuer_type_name, curr_twin) == 0) continue;
                    
                    // copio il nome del soccorritore corrente

                    strcpy(curr_twin, twin->rescuer->rescuer_type_name);
                    
                    // se il soccorritore non fa in tempo, la elimino dall coda d'attesa, riportando tutto sul file.log
                    if(distance_manhattan(twin->x, current_emergency->emergency->x, twin->y, current_emergency->emergency->y, twin->rescuer->speed) > get_priority_limit(current_emergency->emergency->type->priority) - (time(NULL) - current_emergency->emergency->time)) {
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

    /*
        avvio un thread per la funzione control_waiting_queue
    */

    thrd_t control_waiting_queue_t;
    if(thrd_create(&control_waiting_queue_t, control_waiting_queue, NULL) == thrd_success){
        thrd_detach(control_waiting_queue_t);
    } else {
        //ERROR
    }

    while(MESSAGE_QUEUE_ACTIVE){
        
        // stampo il numero di elementi nella coda di attesa
        printf("[N° ELEMENTI NELLA CODA D'ATTESA: %d]\n", waiting_queue_len);
        sem_wait(&sem_waiting_queue);

        /*
            controllo che la message queue sia ancora attiva
        */

        if(!MESSAGE_QUEUE_ACTIVE) break;

        // se il flusso arriva qui significa che c'è almeno un'emergenza in attesa

        printf("[HANDLER WAITING QUEUE: 1 SECONDO DI ATTESA]\n");
        thrd_sleep(&(struct timespec){.tv_sec = 1.2}, NULL);

        /*
            index_current è un indice della coda di attesa che indica un emergenza
            con una diversa priorità che può essere avviata comunque.
            il resto lo spiego nella documentazione e all'interno della funzione
        */

        mtx_lock(&lock_operation_on_waiting_queue);
        if(waiting_queue_len > 0){
            index_current = check_priority_waiting_queue(index_current, &waiting_queue, waiting_queue_len, &queue_emergencies);
            
            // se l'indice è -1 significa che torno al punto di partenza
            if(index_current == -1) index_current = 0;
            
            // prelevo l'emergenza in attesa

            emergency_id_t* e = queue_emergencies[waiting_queue[index_current]->id];
            mtx_unlock(&lock_operation_on_waiting_queue);
            printf("[PRELEVATA DALLA CODA D'ATTESA (%d,%s)]\n", e->id, e->emergency->type->emergency_desc);
            
            // se non è in caricamento prova ad avviarla
            if(!e->in_loading) res = start_emergency(e);
        } else mtx_unlock(&lock_operation_on_waiting_queue);
        mtx_lock(&lock_operation_on_waiting_queue);
        if(((res == thrd_success || res == -2) || res == -1 || res == 0) && waiting_queue_len > 0){
            int val;
            sem_getvalue(&sem_waiting_queue, &val);
            
            // se la coda d'attesa ha almeno un elemento allora ripeto la procedura

            if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
        }
        mtx_unlock(&lock_operation_on_waiting_queue);
    }

    printf("[HANDLER WAITING QUEUE TERMINATO]\n");
    return thrd_success;

}

int handle_rescuer(void* args){

    /*
        prelevo subito l'id decrementandolo di 1 dato che inizia da 0
    */

    int id = *(int*)args;
    id--;
    printf("[SOCCORRITORE: %s;N°:%d] E' STATO ATTIVATO!\n", rescuers_data[id].twin->rescuer->rescuer_type_name, rescuers_data[id].twin->id);

    while(MESSAGE_QUEUE_ACTIVE){

        /*
            in questo ciclo viene ripetuto più volte la condizione if(!MESSAGE_QUEUE_ACTIVE) break;
            in modo tale che se è stata chiusa la message queue, allora i gemelli digitali terminano 
        */
        
        if(!MESSAGE_QUEUE_ACTIVE) break;

        // prendo il lock

        mtx_lock(&rescuers_data[id].lock);

        /*
            resto in attesa fino a quando non arriva una richiesta
        */
          
        while(rescuers_data[id].twin->status == IDLE){
            cnd_wait(&rescuers_data[id].cnd, &rescuers_data[id].lock);
            printf("risvegliato (%d,%s)\n",rescuers_data[id].twin->id,rescuers_data[id].twin->rescuer->rescuer_type_name);
        }

        if(!MESSAGE_QUEUE_ACTIVE) break;

        /*
            id_log lo uso come campo ID per scrivere nel file.log. In particolare gli assegno l'id del gemello digitale
            e il nome dell'emergenza
        */

        char id_log[64];
        snprintf(id_log, sizeof(id_log), "(%d,%s)", id+1, rescuers_data[id].twin->rescuer->rescuer_type_name);
        write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da IDLE a EN_ROUTE_TO_SCENE");
        printf("[SOCCORRITORE:%s;N°:%d] ATTIVATO PER L'EMERGENZA (%d,%s)\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,rescuers_data[id].id_current_emergency,rescuers_data[id].current_emergency);
            
        // partenza e destinazione coordinata x
        
        int from_x = rescuers_data[id].twin->x;
        int to_x = rescuers_data[id].x_emergency;

        // partenza e destinazione coordinata y
        
        int from_y = rescuers_data[id].twin->y;
        int to_y = rescuers_data[id].y_emergency;

        // calcola la stima del tempo di arrivo

        int distance = distance_manhattan(from_x, to_x, from_y,to_y, rescuers_data[id].twin->rescuer->speed);

        printf("[(%s, %d) TEMPO STIMATO DI ARRIVO IN SECONDI: %d]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1,distance);
        
        /*
            ripeto questo ciclo fino a quando non arrivo a destinazione.
            L'idea principale di questo ciclo è che ad ogni iterazione, dato che la velocità
            del soccorritore rappresentano le caselle al secondo, faccio tot_passi_x e tot_passi_y
            dove tot_passi_x + tot_passi_y = caselle al secondo. 
        */
        
        while((rescuers_data[id].twin->x != to_x || rescuers_data[id].twin->y != to_y)){
            if(!MESSAGE_QUEUE_ACTIVE) break;

            // aspetto un secondo

            thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);

            // dx e dy sono i passi che mancano per ogni coordinata

            int dx = to_x - rescuers_data[id].twin->x;
            int dy = to_y - rescuers_data[id].twin->y;
            
            // tot caselle che bisogna fare a iterazione

            double tot_steps = rescuers_data[id].twin->rescuer->speed;

            /*
                In sostanza, sia per x che per y:
                1) controllo il verso positivo o negativo
                2) se il numero di passi che devo fare nella corrente iterazione è maggiore
                del numero di passi che mancano, andrei in overflow, per cui a step 
                (che è il numero di passi che faccio per quella coordinata) gli assegno
                direttamente il numero mancante di passi per arrivare, altrimenti assegno la parte 
                superiore diviso due di tot_steps, per poi assegnare l'altra metà all'altra coordinata.
                3) il numero di passi viene moltiplicato per 1 o -1 in base alla località 
                del soccorritore
            */

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

        /*
            Mi preparo queste 4 variabili da passare alla barrier utilizzando
            l'array delle emergenze, identificando l'emergenza su cui sto lavorando con 
            il proprio id salvato nella struttura di rescuers_data.
            Ovviamente passo tutto per riferimento perchè ci sono più emergenze e ogni lock e cv è
            diversa per ogni emergenza
        */

        atomic_int* rescuers_finished = &queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished;
        atomic_int* tot_rescuers_required = &queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required;
        mtx_t* lock = &queue_emergencies[rescuers_data[id].id_current_emergency]->lock_emergency;
        cnd_t* cnd = &queue_emergencies[rescuers_data[id].id_current_emergency]->cnd_emergency;
           
        /*
            uso una barrier per far cominciare a lavorare i soccorritori allo stesso momento.
            L'ultimo soccorritore che arriva modifica lo stato dell'emergenza IN_PROGRESS
            e risveglia gli altri soccorritori
        */
        barrier_rescuers(queue_emergencies[rescuers_data[id].id_current_emergency],rescuers_finished, tot_rescuers_required, lock, cnd, MESSAGE_QUEUE_ACTIVE); 
        if(!MESSAGE_QUEUE_ACTIVE) break;
        
        // modifico lo stato del soccorritore e lo riporto nel file.log
        
        rescuers_data[id].twin->status = ON_SCENE;
        write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da EN_ROUTE_TO_SCENE a ON_SCENE");
           
        //aspetto tot secondi richiesti per l'emergenza
            
        printf("[(%s,%d) ARRIVATO, TEMPO PER IL LAVORO RICHIESTO: %d SEC]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, rescuers_data[id].time_to_manage);
        thrd_sleep(&(struct timespec){.tv_sec = rescuers_data[id].time_to_manage}, NULL);
            
        // incremento atomicamente la variabile rescuers_finished
        
        atomic_fetch_add(&queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished, 1);
        
        // Se hanno finito tutti modifico lo stato dell'emergenza e lo riporto ne file.log
        
        if(queue_emergencies[rescuers_data[id].id_current_emergency]->rescuers_finished == queue_emergencies[rescuers_data[id].id_current_emergency]->tot_rescuers_required){
            mtx_lock(&lock_field);
            field_emergency[to_x][to_y] = -1;
            mtx_unlock(&lock_field);
            printf("[EMERGENZA: (%d,%s)] TERMINATA\n", queue_emergencies[rescuers_data[id].id_current_emergency]->id, queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->type->emergency_desc);
            queue_emergencies[rescuers_data[id].id_current_emergency]->emergency->status = COMPLETED;
            char id_log[LENGTH_LINE];
            snprintf(id_log, sizeof(id_log), "(%d,%s)",queue_emergencies[rescuers_data[id].id_current_emergency]->id , rescuers_data[id].current_emergency);
            write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da IN_PROGESS a COMPLETED");
        }

        // modifico lo stato del soccorritore e lo riporto nel file.log
            
        rescuers_data[id].twin->status = RETURNING_TO_BASE;
        write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da ON_SCENE a RETURNING_TO_BASE");
        
        printf("[(%s,%d) LAVORO TERMINATO, TEMPO: %d SEC PER TORNARE ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1, distance);
            
        // stessa logica del ciclo in alto, cambio solo la destinazione

        while((rescuers_data[id].twin->x != from_x || rescuers_data[id].twin->y != from_y)){
            if(!MESSAGE_QUEUE_ACTIVE) break;
            thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
            int dx = from_x - rescuers_data[id].twin->x;
            int dy = from_y - rescuers_data[id].twin->y;
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

        // modifico lo stato del soccorritore e lo riporto nel file.log
  
        printf("[(%s,%d) TORNATO ALLA BASE]\n", rescuers_data[id].twin->rescuer->rescuer_type_name, id+1);
        rescuers_data[id].twin->status = IDLE;
        write_log_file(time(NULL), id_log, RESCUERS_STATUS, "Transizione da RETURNING_TO_BASE a IDLE");
        mtx_unlock(&rescuers_data[id].lock);
    }

    printf("[RESCUER: (%d,%s) TERMINATO]\n", rescuers_data[id].twin->id+1, rescuers_data[id].twin->rescuer->rescuer_type_name);
    return thrd_success;
}

void free_locks_rescuers(rescuers_t** id_locks, int count){

    /*
        lascio i lock dei gemelli digitali e libero la memoria
    */

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

    /*
        Inizializz l'array id_log con l'id e la descrizione dell'emergenza
        per il seguito di questa funzione durante le scritture nel file.log
    */
    char id_log[256];
    snprintf(id_log, sizeof(id_log), "(%d,%s)", current_emergency->id, current_emergency->emergency->type->emergency_desc);

    /*
        Mi tengo la variabile 'emergency' per comodità
    */

    emergency_t* emergency = current_emergency->emergency;

    // Variabile utile per la gestione della coda d'attesa

    current_emergency->in_loading = true;

    /*
        id_locks è una struttura che serve per il corretto uso delle lock dei soccorritori
        tenendo una buona organizzazione.
        In sostanza ogni emergenza arrivata a questo punto può essere soddisfatta sicuramente, il 
        problema è scegliere quali soccorritori scegliere in base al loro stato attuale.
        é stato pensato per ricordarsi su quali soccorritori fare la signal e, oltre a questo,
        anche per capire quanto tempo distribuire ad ognuno dei soccorritori dato che non ci devono
        essere, obbligatoriamente, tutte le istanze necessarie. Infatti, se manca una o più istanze
        (non tutte altrimenti andrebbe in attesa) per un tipo di soccorritore, gli viene assegnato
        più tempo del dovuto per coprire il tempo di gestione dei soccorritori mancanti.
        L'i-esimo id_locks rappresenta la situazione di un determinato soccorritore, ovvero:
        id dei soccorritori disponibili, per lo stesso tipo di soccorritore, e il tempo di gestione.
    */

    rescuers_t** id_locks = (rescuers_t**)malloc(sizeof(rescuers_t*));

    // Inizializzo il suo contatore
    int count_id_locks = 0;
    
    if(id_locks == NULL) {
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /*
        tot_request_rescuers è la somma del numero di istanze dei soccorritori richiesti
    */
    
    int tot_request_rescuers = 0;

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){
        tot_request_rescuers += emergency->type->rescuers[i].required_count;
    }

    /*
        tot_avalaible_rescuers sono invece quelli presenti
    */

    int tot_avalaible_rescuers = 0;

    /*
        ciclo che si ripete per il numero di soccorritori necessari
    */

    for(int i = 0; emergency->type->rescuers_req_number > i; i++){

        // Inizializzo la richiesta i-esima dell'emergenza

        rescuer_request_t req = emergency->type->rescuers[i];

        // num_request indica il numero di soccorritori che sono disponibili al momento

        short int num_request = 0;
        
        /*
            alloco memoria per l'i-esima struttura da appoggio per l'i-esimo soccorritore
            necessario
        */
        
        rescuers_t* id_locks_tmp = (rescuers_t*)malloc(sizeof(rescuers_t));

        if(id_locks_tmp == NULL) {
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        /*
            alloco memoria per l'array di interi che rappresentano gli id dei soccorritori
            disponibili, con il suo rispettivo contatore
        */

        short int count_id_arr = 0;
        id_locks_tmp->id_arr = (int*)malloc(sizeof(int));

        if(id_locks_tmp->id_arr == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        /*
            Inizializzo la somma dei tempi di gestione dei soccorritori di questo tipo.
            Ad esempio, Incendio: 2,3 -> servono due istanze dove ognuna deve prestarci tre
            secondi, id_locks_tmp->tot_manage = 2*3 = 6 
        */

        id_locks_tmp->tot_manage = req.required_count * req.time_to_manage;

        /*
            In questo ciclo controllo l'array dei gemelli digitali assegnati all'emergenza
            per verificare il loro stato
        */

        for(int j = 0; current_emergency->num_twins > j; j++){

            /*
                controllo se il tipo richiesto è uguale al j-esimo gemello digitale
            */
            if(req.type != NULL && strcmp(req.type->rescuer_type_name, emergency->rescuers_dt[j]->rescuer->rescuer_type_name) == 0){
                
                /*
                    Qui provo a prendere il lock del soccorritre e verifico successivamente che
                    sia IDLE, altrimenti lascio il lock (metodo per evitare il deadlock)
                */
                
                if(mtx_trylock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock) == thrd_success){
                    if(rescuers_data[emergency->rescuers_dt[j]->id-1].twin->status == IDLE){
                        
                        /*
                            Se è IDLE allora me lo salvo. Successivamente controllo se 
                            count_id_arr > 0, se è cosi allora rialloco memoria aumentando la
                            grandezza dell'array di count_id_arr+1
                        */
                        
                        if(count_id_arr > 0){
                           int* tmp = realloc(id_locks_tmp->id_arr, sizeof(int)*(count_id_arr+1));
                           if(tmp == NULL){
                                printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                                exit(MALLOC_ERROR);
                           }
                           id_locks_tmp->id_arr = tmp;
                        }

                        /*
                            aggiorno il numero di soccorritori disponibili sia per il 
                            singolo soccorritore (num_request) che per tutti i soccorritori 
                            (tot_avalaible_rescuers)
                        */ 

                        num_request++;
                        tot_avalaible_rescuers++;

                        /*
                            Aggiungo l'id del nuovo soccorritore e aggiorno il numero
                            di soccorritori disponibili di quell'istanza
                        */

                        id_locks_tmp->id_arr[count_id_arr] = emergency->rescuers_dt[j]->id-1;
                        id_locks_tmp->avalaible_resc = num_request;
                        count_id_arr++;
                    } else mtx_unlock(&rescuers_data[emergency->rescuers_dt[j]->id-1].lock);
                }
            }

            /*
                Controllo se ho tutte le istanze richieste per l'emergenza, se è cosi posso 
                andare a fare lo stesso gioco con il l'i+1-esimo soccorritore che mi serve
            */

            if(num_request == req.required_count) break;
        }

        /*
            se il flusso entra in questa condizione significa che l'emergenza, al momento, non può
            essere soddisfatta. Sicuramente mancano una o più istanze necessarie di un tipo di 
            soccorritore, e l'altro problema può essere che: o ci sono 0 istanze; oppure,
            siccome mancano 1 o più istanze di un soccorritore dovendo distribuire più tempo 
            a ciascuno di essi, devono restare per un tot tempo maggiore della tolleranza
        */
        
        if(!(num_request == req.required_count) && (num_request == 0 || (id_locks_tmp->tot_manage / num_request) > TOLLERANCE)){
            
            // Aggiorno la flag in loading
            
            current_emergency->in_loading = false;

            /*
                rilascio tutti i lock dei gemelli digitali evitando il deadlock
            */

            free_locks_rescuers(id_locks, count_id_locks);
            
            // Debugging
            
            printf("[NON E' STATO POSSIBILE AVVIARE L'EMERGENZA (%d,%s) PER NON DISPONIBILITà DI ALCUNI SOCCORRITORI]\n", current_emergency->id, emergency->type->emergency_desc);
            
            if(current_emergency->emergency->status != PAUSED){
                printf("[EMERGENZA (%d,%s) AGGIUNTA IN CODA D'ATTESA]\n", current_emergency->id, emergency->type->emergency_desc);
                
                /*
                se l'emergenza non era già in attesa, allora cambio il suo stato in PAUSED
                */
                
                current_emergency->emergency->status = PAUSED;

                // Riporto la transizione di stato dell'emergenza nel file.log

                write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da ASSIGNED a PAUSED");
                
                // Dico ora che è nella coda d'attesa
                
                current_emergency->was_in_waiting_queue = true;

                /*
                    La aggiungo nella coda d'attesa in modo sicuro 
                */

                mtx_lock(&lock_operation_on_waiting_queue);
                add_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                int val;
                sem_getvalue(&sem_waiting_queue, &val);

                /*
                    verifico che il semaforo non abbia valore 0 e se cosi fosse allora
                    faccio una sem_post per sbloccarlo
                */

                if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
                mtx_unlock(&lock_operation_on_waiting_queue);
                
            } else if(current_emergency->emergency->status == PAUSED ){
                printf("[ANCORA NON è POSSIBILE SODDISFARE L'EMERGENZA (%d,%s) IN CODA]\n", current_emergency->id,current_emergency->emergency->type->emergency_desc);
            }
                
            return -1;

        } else {

            /*
                Modifico la grandezza dell'id_locks di +1
            */

            if(count_id_locks > 0){
                rescuers_t** tmp = realloc(id_locks, sizeof(rescuers_t*)*(count_id_locks+1));
                if(tmp == NULL){
                    printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
                    exit(MALLOC_ERROR);
                }
                id_locks = tmp;
            }

            id_locks[count_id_locks] = id_locks_tmp;
            count_id_locks++;
        }
    }

    /*
        Se sono arrivato in questa fase significa che posso avviare l'esecuzione dell'emergenza
        al 100% dato che ho occupato tutti i lock dei gemelli digitali che mi servivano
    */

    if(current_emergency->was_in_waiting_queue){

        /*
            Se l'emergenza corrente era in coda d'attesa allora la rimuovo in modo sicuro
        */

        current_emergency->was_in_waiting_queue = false;
        mtx_lock(&lock_operation_on_waiting_queue);
        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
        int val;
        sem_getvalue(&sem_waiting_queue,&val);

        /*
            controllo sempre il semaforo che non abbia valore 0 se ci sono altri elementi
            nella coda
        */

        if(val == 0 && waiting_queue_len > 0) sem_post(&sem_waiting_queue);
        mtx_unlock(&lock_operation_on_waiting_queue);
    }  

    /*
        modifico lo stato dell'emergenza e lo riporto sul file.log
    */

    current_emergency->emergency->status = WAITING;
    write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da WAITING a TIMEOUT");

    /*
        current_emergency->tot_rescuers_required fondamentale per la barrier nella funzione 
        handle_rescuer per capire quando iniziare l'esecuzione dell'emergenza tutti insieme
    */

    current_emergency->tot_rescuers_required = tot_avalaible_rescuers;
   
    /*
        current_emergency->rescuers_finished serve per capire quanti soccorritori sono 
        arrivati all'emergenza per poi sbloccarli tutti quando
        rescuers_finished = tot_rescuers_required 
    */
   
    current_emergency->rescuers_finished = 0;

    /*
        se la priorità è diversa da 0 devo controllare se è possibile eseguirla
    */

    if(get_priority_limit(emergency->type->priority) != 0){

        /*
            Uso l'array id_locks per capire quale gemello digitale coinvolgere
        */

        for(int i = 0; count_id_locks > i; i++){

            /*
                tmp è l'elemento corrente
            */

            rescuers_t* tmp = id_locks[i];

            for(int j = 0; tmp->avalaible_resc > j; j++){

                // id corrente del gemello digitale

                int id = tmp->id_arr[j];

                /*
                  get_priority_limit(emergency->type->priority) - (time(NULL) - emergency->time) 
                  rappresenta il tempo massimo per arrivare al punto dell'emergenza per soddisfarla.
                  se un soccorritore ci mette più di quel tempo per arrivarci, allora non è possibile
                  eseguirla
                */
                
                if(distance_manhattan(rescuers_data[id].twin->x, emergency->x, rescuers_data[id].twin->y, emergency->y, rescuers_data[id].twin->rescuer->speed) > get_priority_limit(emergency->type->priority) - (time(NULL) - emergency->time)){
                    
                    // Lascio tutti i lock dei gemelli digitali
                    
                    free_locks_rescuers(id_locks, count_id_locks);
                    printf("[L'EMERGENZA (%d,%s) NON E' STATA ESEGUITA A CAUSA DI DISTANZA/PROPRITA PER IL SOCCORRITORE: %s']\n",current_emergency->id ,emergency->type->emergency_desc, rescuers_data[id].twin->rescuer->rescuer_type_name);
                    
                    // rimuovo l'emergenza dalla coda d'attesa in modo sicuro

                    mtx_lock(&lock_operation_on_waiting_queue);
                    if(emergency->status == PAUSED){
                        remove_from_waiting_queue(current_emergency, &waiting_queue, &waiting_queue_len);
                    }
                    mtx_unlock(&lock_operation_on_waiting_queue);
                    
                    // modifico lo stato dell'emergenza e lo riporto nel file.log

                    emergency->status = TIMEOUT;
                    write_log_file(time(NULL), id_log, EMERGENCY_STATUS, "Transizione da WAITING a TIMEOUT");
                    return -2;
                }
            }  
        }
    }

    // imposto l'id dell'emergenza nella griglia

    mtx_lock(&lock_field);
    field_emergency[emergency->x][emergency->y] = current_emergency->id;
    mtx_unlock(&lock_field);

    /*
        Se sono arrivato qui posso avviare l'emergenza
    */

    for(int i = 0; count_id_locks > i; i++){
        rescuers_t* tmp = id_locks[i];

        /*
            scorro tutti gli id del j-esimo soccorritore
            necessario
        */

        for(int j = 0; tmp->avalaible_resc > j; j++){

            // prendo l'id del gemello digitale

            int id = tmp->id_arr[j];
            
            /* 
                Inizializzo i vari parametri alla struttura che sorveglia il gemello digitale
            */

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

            /*
                risveglio il gemello dormiente
            */

            cnd_signal(&rescuers_data[id].cnd);
            mtx_unlock(&rescuers_data[id].lock);
        }
    }

    /*
        riporto nel file.log il successo dell'avvio dell'emergenza riportando
        tutti i soccorritori che ho mandato in esecuzione
    */

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

    /*
        Casto il parametro della funzione per ottenere quello che serve
        per settare l'emergenza
    */

    params_handler_emergency_t* p = (params_handler_emergency_t*)args;
    rescuer_digital_twin_t** twins = p->params->rp_rescuers->rd_twins;
    int num_twins = p->params->rp_rescuers->num_twins;
    p->emergency_avalaible = emergency_avalaible;
    
    // Imposto i parametri dell'emergenza corrente

    int current_twins = 0;
    emergency_t* emergency = set_new_emergency(p, &current_twins);
    
    /*
        verifico con rescuers_is_avalaible se sono disponibili i soccorritori
        necessari per l'emergenza corrente. Se restituisce -1 mancano 1 o più soccorritori
    */
    
    int rescuers_avalaible = rescuers_is_avalaible(twins, num_twins, emergency->type->rescuers, emergency->type->rescuers_req_number, emergency->type->emergency_desc);
    if(rescuers_avalaible == -1){
        /*
            Riporto sul file.log che non è possibile soddisfare l'emergenza
        */
        printf("l'emergenza: %s; non è possibile soddisfarla\n", emergency->type->emergency_desc);
        char* desc_error = "emergenza non gestibile per mancanza di risorse";
        write_log_file(time(NULL), emergency->type->emergency_desc, EMERGENCY_STATUS, desc_error);

        // modifico lo stato dell'emergenza

        emergency->status = CANCELED;
        
        /*
            La aggiungo comunque all'array delle emergenze, valide e non, per avere una panoramica
            totale di tutte le emergenze (non necessario)
        */
        
        mtx_lock(&lock_operation_on_queue_emergency);
        add_emergency(&id_emergencies, emergency, &queue_emergencies);
        mtx_unlock(&lock_operation_on_queue_emergency);
        return -1;

    } 

    /*
        Aggiungo l'emergenza valida nell'array delle emergenze  
    */
   
    mtx_lock(&lock_operation_on_queue_emergency);
    emergency_id_t* current_emergency = add_emergency(&id_emergencies, emergency, &queue_emergencies);
    mtx_unlock(&lock_operation_on_queue_emergency);

    /*
        Inizializzo altri parametri necessari, della struttura aggiunta, per 
        il corretto funzionamento del programma
    */

    mtx_init(&current_emergency->lock_emergency, mtx_plain);
    cnd_init(&current_emergency->cnd_emergency);
    current_emergency->num_twins = current_twins;
    current_emergency->was_in_waiting_queue = false;
    current_emergency->in_loading = false;
    if(rescuers_avalaible == 0){
        current_emergency->miss_rescuers = true;
    } else current_emergency->miss_rescuers = false;

    /*
        Riporto nel file.log che l'emergenza corrente è stata assegnata
    */

    char id_log[LENGTH_LINE];
    snprintf(id_log, sizeof(id_log), "(%d,%s) da %s a %s", current_emergency->id, current_emergency->emergency->type->emergency_desc, "WAITING", "IN_PROGRESS");
    write_log_file(time(NULL),id_log, EMERGENCY_STATUS, "Emergenza passata nello stato ASSIGNED");

    // Avvio l'emergenza

    start_emergency(current_emergency);

    // Opzionale (debugging)

    mtx_lock(&lock_operation_on_waiting_queue);
    print_waiting_emergencies(waiting_queue, waiting_queue_len);
    mtx_unlock(&lock_operation_on_waiting_queue);

    return thrd_success;
  
}


int handler_queue_emergency(void* args){

    /*
        Casting dell'argomento con il tipo della struttura che rappresenta
        i parametri di questa funzione
    */

    params_handler_queue_t* params = (params_handler_queue_t*)args;

    /*assert(params->environment != NULL);
    assert(params->environment->queue_name != NULL);
    assert(strlen(params->environment->queue_name) < LENGTH_LINE-2);*/
   
    /*
        Inizializzo la message queue con i rispettivi parametri
    */

    mqd_t message_queue;
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MSG;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    /*
        Vado a modificare il nome della message queue 
        aggiungendo '/' davanti al nome
    */

    char mq_name[LENGTH_LINE];
    mq_name[0] = '/';
    strncpy(&mq_name[1], params->environment->queue_name, LENGTH_LINE-2);
    mq_name[LENGTH_LINE-1] = '\0';

    /*
        Apro la message queue
    */

    if((message_queue = mq_open(mq_name, O_CREAT | O_RDONLY, 0644, &attr)) == -1){
        perror("mq_open");
        return 0;
    } else printf("[MESSAGE QUEUE: %s AVVIATA CON SUCCESSO!]\n", mq_name);
    thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
    
    printf("[SERVER IN ASCOLTO...]\n\n");
    
    /*
        msg è l'emergenza che riceve la message queue che verrà castata
        nel tipo emergency_request_t ogni volta che arriva una nuova richiesta
    */
    
    void* msg = malloc(MAX_SIZE);

    if(msg == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }
    
    for(;;){

        // In attesa dell'emergenza

        size_t bytes_read = mq_receive(message_queue, (char*)msg, MAX_SIZE, NULL);
        
        // Se non ha letto più di 0 byte si arresta per errore
        
        if(!(bytes_read > 0)){
            perror("mq_receive bytes read");
            exit(0);
        }

        // Procedo con il casting se non ci sono errori
        
        emergency_request_t* request = (emergency_request_t*)msg;

        /*
            Se il nome della message queue è STOP termina la message queue
            e l'esecuzione del programma
        */

        if(strcmp(request->emergency_name, "STOP") == 0){
            printf("[MESSAGE QUEUE TERMINATA]\n");
            break;
        }

        /*
            Inizializzo message che uso per riportare informazioni nel file.log
            - prima lo passo alla funzione is_invalid_request che, nel caso ci 
              dovesse essere un errore con i parametri dell'emergenza inviata, 
              message sarà diverso in base al tipo di problema che c'è stato.
            - se i parametri dell'emergenza inviata sono buoni, allora viene usata per
              scrivere nel file.log che è andato tutto bene
        */

        char* message = (char*)malloc(sizeof(char)*LENGTH_LINE);
        if(message == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
        
        /*
            index_emergency è una variabile che uso per identificare il tipo di emergenza
            da soddisfare che si trova nell'array globale emergency_avalaible.
            viene passata per riferimento alla funzione is_valid_request e in caso
            di successo, gli viene passato l'indice dell'emergenza
        */

        int index_emergency = -1;

        /*
            Se l'emergenza non è valida non la eseguo e passo direttamente alla prossima 
            iterazione
        */

        if(!is_valid_request(request, params->environment->x, params->environment->y, message, &index_emergency, emergency_avalaible, num_emergency_avalaible)){
            printf("richiesta non valida!\n");
            strcpy(request->emergency_name, "N/A");
            write_log_file(time(NULL), request->emergency_name, MESSAGE_QUEUE, message);
            continue;
        }

        if(field_emergency[request->x][request->y] != -1){
            printf("[L'EMERGENZA %s (%d,%d) NON PUO' ESSERE SODDISFATTA PER UN'ALTRA EMERGENZA IN CORSO IN QUELLA POSIZIONE]\n",request->emergency_name, request->x, request->y);
            snprintf(message, LENGTH_LINE, "[l'emergenza %s (%d,%d) non può essere soddisfatta per un 'altra emergenza in corso in quella posizione]", request->emergency_name, request->x, request->y);
            write_log_file(time(NULL), request->emergency_name, MESSAGE_QUEUE, message);
            continue;
        }

        /*
            riporto sul file.log che l'emergenza si può soddisfare
        */

        snprintf(message, LENGTH_LINE, "[%s (%d,%d) %ld]", request->emergency_name, request->x, request->y, request->delay);
        write_log_file(time(NULL), request->emergency_name, MESSAGE_QUEUE, message);

        /*
            Inizializzo una struttura che serve per settare e startare l'emergenza
            che verrà passata come parametro alla funzione che gestirà un altro thread
        */

        params_handler_emergency_t* params_handler_emergency = (params_handler_emergency_t*)malloc(sizeof(params_handler_emergency_t));
        if(params_handler_emergency == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        // Inizializzo i suoi parametri

        params_handler_emergency->index_emergencies = index_emergency;
        params_handler_emergency->x = request->x;
        params_handler_emergency->y = request->y;
        params_handler_emergency->params = params;
        params_handler_emergency->timestamp = request->delay;
        
        thrd_t thread_handler_emergency;

        thrd_create(&thread_handler_emergency, handler_emergency, params_handler_emergency);
        
    }

    // segnalo che la message queue è terminata

    MESSAGE_QUEUE_ACTIVE = false;

    // Elimino e libero la memoria

    mq_close(message_queue);
    mq_unlink(mq_name);

    return 1;
}

int main(){

    /*
        faccio la init di una lock che protegge le operazioni 
        sulla coda di attesa, una che protegge le operazioni 
        sull'array delle emergenze, e il semaforo inizializzato a 0
        in modalità locale (sulla stessa memoria)
    */

    mtx_init(&lock_operation_on_waiting_queue, mtx_plain);
    mtx_init(&lock_field, mtx_plain);
    mtx_init(&lock_operation_on_queue_emergency, mtx_plain);
    sem_init(&sem_waiting_queue, 0, 0);

    /*
        - handler_queue: si occupa di ricevere le emergenze dalla message
          queue e capisce se eseguirla o meno;
        - handler_waiting_queue_t: si occupa delle emergenze nella coda
          d'attesa
        - news_on_digital_twins: stampa lo stato attuale dei gemelli
          digitali dopo tot secondi (utile per il debugging)
    */

    thrd_t handler_queue, handler_waiting_queue_t, news_on_digital_twins;

    // Ottengo i valori dell'ambiente
    
    env_t* environment = parser_env(ENVIRONMENT_FILENAME);

    // posso a questo punto impostare la griglia

    field_emergency = (int**)malloc(sizeof(int*)*environment->y);
    if(field_emergency == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /*
        imposto tutti i valori della griglia a -1 che indica emergenza in quel punto assente
    */

    for(int i = 0; environment->y > i; i++){
        field_emergency[i] = (int*)malloc(sizeof(int)*environment->x);
        if(field_emergency[i] == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }
        for(int j = 0; environment->x > j; j++){
            field_emergency[i][j] = -1;
        }
    }


    /*
        Ottengo le emergenze che il programma è in grado di 
        soddisfare
    */

    num_emergency_avalaible = 0;
    emergency_avalaible = parser_emergency(EMERGENCY_FILENAME, &num_emergency_avalaible);
    
    /*
        Ottengo i soccorritori e i gemelli digitali con le loro
        grandezze
    */
    result_parser_rescuers* rp_rescuers = parse_rescuers(RESCUERS_FILENAME);
    int num_twins = rp_rescuers->num_twins;
    
    /*
        per ogni rescuer active rappresenta un gemello digitale in esecuzione
        pronto a ricevere ordini
    */

    thrd_t rescuers_active[num_twins];

    /*
        Inizializzo la struttura che uso come parametro della funzione handler_queue_emergency
        che gestisce la message queue, contenente le variabili d'ambiente e i soccoritori
    */

    params_handler_queue_t* params = (params_handler_queue_t*)malloc(sizeof(params_handler_queue_t));
    if(params == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    params->environment = environment;
    params->rp_rescuers = rp_rescuers;

    /*
        Inizializzo la struttura rescuer_data_t per ogni gemello digitale
        utile in primo luogo per la sincronizzazione. Per il resto è 
        riportato sulla documentazione
    */

    rescuers_data = (rescuer_data_t*)malloc(sizeof(rescuer_data_t)*num_twins);
    if(rescuers_data == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /*
        ciclo che starta i gemelli digitali con i propri lock, cv, ecc..
    */

    for(int i = 0; num_twins > i; i++){
        rescuers_data[i].twin = rp_rescuers->rd_twins[i];
        mtx_init(&rescuers_data[i].lock, mtx_plain);
        cnd_init(&rescuers_data[i].cnd);
        rescuers_data[i].time_to_manage = 0;
        thrd_create(&rescuers_active[i], handle_rescuer, &rp_rescuers->rd_twins[i]->id);
    }

    // avvio handler_waiting_queue in modalità detach

    if(thrd_create(&handler_waiting_queue_t, handler_waiting_queue, NULL) == thrd_success){
        thrd_detach(handler_waiting_queue_t);
    } else {
        //ERROR
    }

    // avvio il thread che gestisce la message queue

    thrd_create(&handler_queue, handler_queue_emergency, params);

    // avvio il thread che stampa la situazione dei gemelli digitali (opzionale)

    if(thrd_create(&news_on_digital_twins, print_state_digital_rescuer, rp_rescuers) == thrd_success){
        thrd_detach(news_on_digital_twins);
    } else {
        //ERROR
    }
    
    thrd_join(handler_queue, NULL);

    /*
        per ogni rescuers_data se è nello stato idle lo porto in en_route_to_scene
        per farlo terminare (subito dopo che si sveglia verifica se la message queue è 
        ancora operativa) per poi fare la join per ognuno di essi, dato che poi viene
        liberata la memoria, evitando segmantation fault
    */
    
    for(int i = 0; num_twins > i; i++){
        if(rescuers_data[i].twin->status == IDLE){
            rescuers_data[i].twin->status = EN_ROUTE_TO_SCENE;
            cnd_signal(&rescuers_data[i].cnd);
            mtx_unlock(&rescuers_data[i].lock);
        }
    }

    for(int i = 0; num_twins> i; i++){
        thrd_join(rescuers_active[i], NULL);
    }

    /*
        controllo che il thread controllore della message queue non sia
        ancora in attesa del semaforo. Se è in attesa, lo sveglio per far 
        terminare la funzione
    */

    int val;
    sem_getvalue(&sem_waiting_queue, &val);
    if(val == 0) sem_post(&sem_waiting_queue);

    /*
        distruggo tutte le cv globali con il semaforo e libero
        la memoria completamente
    */
    
    mtx_destroy(&lock_operation_on_waiting_queue);
    mtx_destroy(&lock_operation_on_queue_emergency);
    mtx_destroy(&lock_field);
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
