#include "client.h"

int send_queue(char* filename,  emergency_request_t* msg){

    // Prelevo i parametri d'ambiente (nome della coda, lunghezza e larghezza della griglia)

    env_t* env = parser_env(filename);

    char* mq_name = (char*)malloc(sizeof(char)*(strlen(env->queue_name)+1));
    if(mq_name == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    /* 
       Faccio questi passaggi per il corretto funzionamento della message queue
       inserendo '/' prima del nome della coda 
    */

    strcpy(mq_name, "/");
    strcat(mq_name, env->queue_name);

    /*
        inizializzo la message queue in modalità solo scrittura
    */
    
    mqd_t mq;

    if((mq = mq_open(mq_name, O_WRONLY)) == -1){
        perror("mq_open");
        return 0;
    } 

    if (mq_send(mq ,(char*)msg, 1024, 0) == -1) {
		perror("mq_send");
		return 0;
	}

    mq_close(mq);
    free(mq_name);
    free(env);

    return 1;

}

void stop_message_queue(){

    /*
        Inizializzo il nome dell'emergenza con 'STOP'
        per far terminare l'esecuzione del server.
        L'inizializzazione o non degli altri parametri
        è indifferente visto che per prima cosa viene controllato
        il nome della message queue dall'altra parte
    */

    char* stop = "STOP";
    emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));
    strcpy(msg->emergency_name, stop);
    msg->x = -1;
    msg->y = -1;
    msg->timestamp = 0;
    if(send_queue(ENVIRONMENT_FILENAME, msg) != 1){
        printf("[ERRORE NELL'INVIO DI 'STOP']\n");
        exit(0);
    }

    return;

}

int send_with_parameters(char** parameters){

    /*
        Inizializzo la struttura per inviare l'emergenza
    */

    emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));

    if(msg == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    strcpy(msg->emergency_name, parameters[1]);
    msg->x = atoi(parameters[2]);
    msg->y = atoi(parameters[3]);
    msg->timestamp = time(NULL) + atoi(parameters[4]);

    return send_queue(ENVIRONMENT_FILENAME, msg);
}

int send_with_file(char* file_emergency){

    /*
        Creo una copia nel nome del file per: 
        - verificare che sia ben formata (ovvero  nome_file.estensione) 
          confrontando prima e dopo la strtok();
        - Per aprire correttamente il file con l'altra copia/o versione originale 
          dato che la strtok() sposta il puntatore della stringa
    */
    
    char* copy = strdup(file_emergency);
    char* tok = strtok(file_emergency, ".");

    /*
        Faccio questo confronto per verificare che sia presente
        il punto di separazione con l'estensione.
    */
    if(strcmp(copy, tok) == 0){
        printf("[ERRORE NELL'INSERIMETO DEI PARAMETRI]\n");
        return -1;
    }
    tok = strtok(NULL, " ");
    
    // Verifico se il file ha un estensione ammissibile

    if(strcmp(tok, "txt") != 0 && strcmp(tok, "conf") != 0){
        printf("[IL FILE DEVE AVERE UN ESTENSIONE DEL TIPO {txt, conf}]\n");
        return 0;
    }

    FILE* file = fopen(copy, "r");

    if(file == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    char line[LENGTH_LINE];

    /*
        Prelevo del righe del file delle emergenze
    */

    while(fgets(line, LENGTH_LINE, file)){

        emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));

        if(msg == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        /*
            Tokenizzo la riga i-esima del file <nome emergenza><x><y><delay> 
            utilizzando strtok()
        */

        int i = 1;
        for(char* t = strtok(line, " "); t != NULL; t = strtok(NULL, " "), i++){
            switch(i){
                case 1: strcpy(msg->emergency_name, t); break;
                case 2: msg->x = atoi(t); break;
                case 3: msg->y = atoi(t); break;
                case 4: msg->timestamp = time(NULL) + atoi(t); break;
            }
        }

        // Invio la richiesta

        if(send_queue(ENVIRONMENT_FILENAME, msg) != 1){
            printf("[SI E' VERIFICATO UN ERRORE NELL'INVIO DELLE EMERGENZE; (FILE;LINE)=(%s,%d)]\n", __FILE__, __LINE__);
            exit(0);
        }

    }

    // Libero la memoria

    fclose(file);

    printf("[INVIO DELLE EMERGENZE ANDATA A BUON FINE ATTRAVERSO IL FILE: %s]\n", file_emergency);
    return 1;
        
}


int main(int argc, char** argv){

    // Modalità 1
    
    if(argc == 1){

        /*
            Ottengo i parametri dell'ambiente e inizializzo array di caratteri
            che possono servire nel caso in cui venga scelta l'opzione 0.
            I parametri dell'ambiente ovviamente li prelevo prima del while
            per minimizzare l'overhead, non dovendo ogni volta richiamare parser_env()
        */

        env_t* env = parser_env(ENVIRONMENT_FILENAME);
    
        /*
            con digits capisco di quanto devo far grande l'array di caratteri
            in base alle coordinate
        */

        char x[digits(env->x)];
        char y[digits(env->y)];
        char delay[8];

        while(1){

            /*
                Menù 
            */

            printf("***********************************************************************************\n");
            printf("*                   -- ISTRUZIONI PER L'INVIO DELLE EMERGENZE --                  *\n");
            printf("***********************************************************************************\n");
            printf("* INSERISCI 0 PER INVIARE UN EMERGENZA CON SINTASSI <nome_emergenza><x><y><delay> *\n");
            printf("* INSERISCI 1 PER INVIARE EMERGENZE ALL'INTERNO DI UN FILE: <nome file>           *\n");
            printf("* INSERISCI 2 PER TERMINARE LA MESSAGE QUEUE                                      *\n");
            printf("* INSERISCI 3 PER USCIRE DAL PROGRAMMA                                            *\n");
            printf("***********************************************************************************\n");
            
            short int option = -1;

            /*
                Fino a quando non si inserisce un opzione valida
                il programma richiede di nuovo l'inserimento

            */

            while(option < 0 || option > 3){
                printf("\n[OPZIONE] >: ");
                scanf("%hd", &option);
                if(option < 0 || option > 3){
                    printf("Opzione non esistente!\n");
                }
            }

            char emergency_or_file[LENGTH_LINE];
            
            if(option == 0){

                printf("[NOME EMERGENZA]: ");
                scanf("%s", emergency_or_file);
                printf("[x;y]: ");
                scanf("%s %s", x,y);
                printf("[DELAY]: ");
                scanf("%s", delay);

                char* params[] = {"\0", emergency_or_file,x,y,delay};
                if(send_with_parameters(params) != 1){
                    printf("{ERROR}\n");
                    exit(0);
                } else { printf("invio dell'emergenza andata a buon fine!\n"); }

            } else if(option == 1){

                printf("[NOME FILE]: ");
                scanf("%s", emergency_or_file);

                if(send_with_file(emergency_or_file) != 1){

                    exit(FILE_ERROR);
                } 

            } else if(option == 2){ stop_message_queue(); }
            else if(option == 3) return 0;
        }


    } else if(strcmp(argv[1], "STOP") == 0){
        stop_message_queue();
    }

    // Modalità 2

    if(argc != 5 && argc != 3){

        // Numero dei parametri inseriti non idoneo

        printf("[PARAMETRI NON INSERITI CORRETTAMENTE]\n");
        exit(0);

    } else if(argc == 3){

        /*
            Invio delle emergenze tramite file
            specificato da terminale
        */

        if(strcmp(argv[1], "-f") != 0){

            // Se il secondo parametro argv[1] != -f non va bene

            printf("[ERRORE NELL'INSERIMENTO DEL FLAG]\n");
            exit(0);

        } else {
            
            /*
                Invio delle emergenze tramite file specificato da terminale
            */

            if(send_with_file(argv[2]) != 1){
                exit(FILE_ERROR);
            } 
        }

    } else if(argc == 5){

        // Invio di una singola emergenza da terminale

        if(send_with_parameters(argv) != 1){
            printf("[ERRORE NELL'INSERIMENTO DEI PARAMETRI]\n");
            exit(0);
        } else { printf("[INVIO DELL'EMERGENZA ANDATA A BUON FINE]\n"); }

    }


    return 0;
}
