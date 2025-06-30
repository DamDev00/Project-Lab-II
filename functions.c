#include "functions.h"
#include <stdbool.h>

char* trim(char* line){

    if(line == NULL) return NULL;
    
    // Prelevo la lunghezza della stringa

    int len = strlen(line);

    // Alloco memoria per la stringa 'trimmata'

    char* result = (char*)malloc(sizeof(char)*(len)); 
    if (result == NULL) {
        printf("{MALLOC ERROR}\n");
        exit(MALLOC_ERROR); 
    }

    /*
        Scorro la stringa eliminando gli spazi vuoti
    */

    int c_result = 0;
    for (int i = 0; i < len; i++) {
        if(line[i] != ' ')
            result[c_result++] = line[i];
    }

    // Imposto la fine della stringa 

    result[c_result] = '\0';

    return result;
}

void write_log_file(time_t timestamp, char* id, char* evento, char* desc){

    /*
        Apro il descrittore del file per scriverci all'interno
        utilizzando una MACRO
    */

    int fd;
    SCALL(fd, open(LOG_FILENAME, O_WRONLY | O_APPEND, 0644), "apertura file");

    /*
        Questa è una struct che ho trovato in rete che mi aiuta
        a scrivere il timestamp in un formato ore:minuti:secondi.
        La funzione localtime setta i parametri della struct:

        struct tm {
            int tm_sec;			   Seconds.	[0-60] (1 leap second) 
            int tm_min;			   Minutes.	[0-59] 
            int tm_hour;		   Hours.	[0-23] 
            int tm_mday;		   Day.		[1-31] 
            int tm_mon;			   Month.	[0-11] 
            int tm_year;		   Year	- 1900.  
            int tm_wday;		   Day of week.	[0-6] 
            int tm_yday;		   Days in year.[0-365]	
            int tm_isdst;		   DST.		[-1/0/1] 
        }

        (in time.h)

        fonte: https://cplusplus.com/reference/ctime/localtime/
    */

    struct tm* tm_info = localtime(&timestamp);

    // Inizializzo la stringa 'line_log' con tutti i parametri necessari

    char line_log[LENGTH_LINE];
    snprintf(line_log, sizeof(line_log), "[%d:%d:%d] [%s] [%s] [%s] \n", tm_info->tm_hour,tm_info->tm_min, tm_info->tm_sec, id, evento, desc);
    
    // Riporto tutto sul file.log con la syscall write

    ssize_t bytes;
    SCALL(bytes, write(fd, line_log, strlen(line_log)), "Scrittura nel file.log");

    if(bytes == -1){
        perror("Errore nella scrittura del file");
        SCALL(fd, close(fd), "close");
        exit(FILE_ERROR);
    }

    SCALL(fd, close(fd), "close");
    return;

}

int format_check_rescuers(char* line){

    // [<string>][<int>][<int>][<int>;<int>]

    // Verifico che la stringa non sia vuota
    if(line == NULL || !(strlen(line) > 0)) return -1;

    /*
        Inizializzo due counter per le parentesi quadre chiuse a destra/sinistra
        che servono per capire quale campo, tra i 4 campi del soccorritori, ci si trova
    */

    short int left_square_brackets = 0;
    short int right_square_brackets = 0;

    // Contatore per i caratteri dell'array

    int count = 0;

    // La uso per l'ultimo campo (le coordinate)

    bool semicolon = false;

    while(line[count] != '\0'){

        /*
            Ad ogni inizio del ciclo verifico che il carattere corrente sia '[',
            e che successivamente non sia ']' perchè altrimenti ci si ritroverebbe
            a fare il parsing con '[]'.
        */

        if(line[count++] != '[') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
            return -1;
        }
        if(line[count] == ']') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
            return -1;
        }

        left_square_brackets++;

        /*
            Continuo fino a quando la stringa non è al termine e che
            il carattere corrente è diverso da ']'
        */
        
        while(line[count] != ']' && line[count] != '\0'){

            /*
                case 1: controllo il nome del soccorritore. Devono essere presenti 
                solamente caratteri e '_' per non lasciare lo spazio vuoto.
                Ad esempio, se nel rescuers.conf c'è 'vigili del fuoco' per le meccaniche
                del parser verrebbe convalidato ma senza spazi (vigilidelfuoco, poco leggibile)
                e quindi è possibile scriverlo mettendo '_' tra gli spazi vuoti per 
                renderlo più leggibile (vigili_del_fuoco)

                case 2 e 3: verifico che ogni carattere sia un numero

                case 4: controllo la sintassi delle coordinate [x;y] facendo tutti i possibili
                controlli affinchè la sintassi sia giusta
            */

            switch(left_square_brackets){
                case 1: if(!(line[count] >= 'a' && line[count] <= 'z') && !(line[count] >= 'A' && line[count] <= 'Z') && line[count] != '_') {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [x][<int>][<int>][<int>;<int>]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                } break;
                case 2:
                case 3: if(!(line[count] >= '0' && line[count] <= '9')) {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [<string>][x][x][<int>;<int>]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                } break;
                case 4: if(((line[count-1] == '[' && line[count] == ';') || 
                (line[count] == ';' && line[count+1] == ']')) && !(line[count] >= '0' && line[count] <= '9')) {
                    printf("[ERRORE NEL PARSING DEL FILE: %s PER UN CARATTERE NON APPROPRIATO [<string>][<int>][<int>][x]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
                    return -1;
                }
                if(line[count] == ';') semicolon = true;
            }

            count++;
        }

        /*
            Verifico che, una volta arrivato alla fine del campo, è presente
            ']', incrementando ovviamente 'count' dato che il prossimo controllo
            consiste nel verificare che il carattere corrente sia '[' 
        */

        if(line[count] == ']'){
            right_square_brackets++;
            count++;
        } else return -1;


    }

    /*
        Se non era presente il ; nell'ultimo campo è sbagliato
    */

    if(!semicolon){
        printf("[ERRORE NEL PARSING DEL FILE: %s PER LA MANCANZA DEL ';' [<string>][<int>][<int>][x]]; LINEA ERRORE: %d\n", RESCUERS_FILENAME,__LINE__);
        return -1;
    }

    return 1;
  

}

int format_check_emergency(char* line){

    // [<string>][<int>]<string>:<int>,<int>;

    // Verifico che l'array di caratteri non sia vuoto

    if(line == NULL || !(strlen(line) > 0)) return -1;

    /*
        Inizializzo il contatore dell'array e la variabile successiva 
        che serve per i primi due campi [<string>][<int>] per capire quando fermarsi
    */

    int count = 0;
    int close_square_brackets = 0;

    while(close_square_brackets < 2){

        /*
            Qui solito discorso che ho fatto nel parser per i soccorritori
        */

        if(line[count++] != '[') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        if(line[count] == ']'){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER PARENTESI]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }
        while(line[count] != ']'){

            /*
                case 0: solito discorso del primo case nel parser dei soccorritori;
                case 1: verifico che la priorità sia un intero
            */
            switch(close_square_brackets){
                case 0: if(!(line[count] >= 'a' && line[count] <= 'z') && !(line[count] >= 'A' && line[count] <= 'Z') && line[count] != '_'){
                            printf("[ERRORE NEL PARSING DEL FILE: %s PER CARATTERE NON APPROPRIATO]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                            return -1;
                        } break;
                case 1:  if(!(line[count] >= '0' && line[count] <= '9')) {
                            printf("[ERRORE NEL PARSING DEL FILE: %s PER CARATTERE NON APPROPRIATO]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                            return -1;
                        } break;
            }
            count++;
        }
        close_square_brackets++;
        count++;
    }

    /*
        Passo alla parte successiva controllando <string>:<int>,<int>;
    */

    /*
        copio alla variabile 'rest' tutto quello che viene dopo [<string>][<int>]
        ovvero la lista dei soccorritori necessari
    */

    char rest[LENGTH_LINE];
    strncpy(rest, line+count, sizeof(rest));

    /*
        creo una copia dell'array 'rest' perchè nel caso dovesse mancare ;
        rimarrebbe uguale rest, ed uso 'copy' come supporto per non spostare 
        il puntatore alla variabile 'rest'
    */

    char* copy = strdup(rest);
    char* tok = strtok(copy, ";");

    /*
        se tok = rest significa che non c'è nessun ; ed è sbagliato
    */

    if(strcmp(tok, rest) == 0){
        printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
        return -1;
    }
    

    while(tok != NULL){

        // Inizializzo il counter del resto della riga

       int count_tok = 0;

        /*
            1° fase: verifico che il nome abbia solo caratteri dalla a alla z oppure '_'
            per coprire gli spazi vuoti
        */

       while(tok[count_tok] != ':'){
            if(!(tok[count_tok] >= 'a' && tok[count_tok] <= 'z') && !(tok[count_tok] >= 'A' && tok[count_tok] <= 'Z') && tok[count_tok] != '_'){
                printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
                return -1;
            }
            count_tok++;
        }

        // Incremento un'altra volta per andare oltre il ;

        count_tok++;

        // Verifico subito che sia un numero il prossimo carattere

        if(!(tok[count_tok] >= '0' && tok[count_tok] <= '9')) {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }

        count_tok++;

        /*
            vado avanti finchè il prossimo carattere sia diverso da un numero
            (sperando sia una virgola)
        */
        
        while (tok[count_tok] >= '0' && tok[count_tok] <= '9') count_tok++;

        // Controllo che sia una virgola

        if(tok[count_tok++] != ',') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }

        // Faccio la stessa cosa di quella precedente per il tempo dedicato 

        if(!(tok[count_tok] >= '0' && tok[count_tok] <= 'z')) {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }

        while (tok[count_tok] >= '0' && tok[count_tok] <= '9') count_tok++;
        if(tok[count_tok] != '\0') {
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", EMERGENCY_FILENAME,__LINE__);
            return -1;
        }

        // Vado a tokenizzare il prossimo soccorritore (se esiste)
        tok = strtok(NULL, ";");
    }


    return 1;
}

int format_check_env(char* line, char* matricola){

    // Verifico che l'array di caratteri non sia vuoto

    if(line == NULL || !(strlen(line) > 0)) return -1;

    /*
        Uso 'copy' come diversivo per controllare che, dopo strtok(...,"="), 
        abbia tokenizzato correttamente la stringa senza restituire la stessa
    */

    char* copy = strdup(line);
    char* tok = strtok(copy, "=");
    if(strcmp(tok, line) == 0) {
        printf("error\n");
        exit(0);
    }

    //contatore per il valore dopo l'uguale

    int count = 0;
    
    if(strcmp(tok, "queue") == 0){

        /*
            Se è uguale a queue controllo che abbia solo caratteri fino a un certo punto
            e che, successivamente, abbia il numero della mia matricola
        */

        tok = strtok(NULL, " ");
        if(tok == NULL){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
            exit(0);
        }   
        
        /*
            Vado avanti finchè il prossimo elemento sia diverso da un carattere
        */
        
        while(((tok[count] >= 'a' && tok[count] <= 'z') || (tok[count] >= 'A' && tok[count] <= 'Z'))) count++;
            
        //Se si è arrivati alla fine non va bene perchè dev'essere presente la matricola

        if(tok[count] == '\0' || tok[count] == '\n'){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
            return -1;
        }

        /*
            count_matricola: counter per la MACRO definita all'inizio;
            init_count: ricordo da dove mi sono fermato con il counter;
        */
        
        int count_matricola = 0;
        int init_count = count;
        int len_matricola = strlen(matricola);


        while(tok[count] != '\0' && tok[count] != '\n'){
            /*
                Verifico che il numero corrente sia valido
            */
            if(tok[count++] != matricola[count_matricola++]) {
                printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
                return -1;
            }
        }

        /*
            Arrivato qui controllo che il numero di elementi che ho 
            comparato con la matricola sia effettivamente uguale
            alla lunghezza della matricola
        */

        if(count - init_count != len_matricola){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
            return -1;
        }

    } else if((strcmp(tok, "width") == 0 || strcmp(tok, "height") == 0)){

        /*
            Qui molto più semplificato controllo semplicemente che 
            il valore dopo l'uguale sia intero
        */

        tok = strtok(NULL, " ");
        if(tok == NULL){
            printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
            exit(0);
        }  

        while(tok[count] != '\0' && tok[count] != '\n'){
            if(!(tok[count] >= '0' && tok[count] <= '9')) {
                printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
                return -1;
            }
            count++;
        }        
    } else {
        /*
            Il valore prima dell'uguale non va bene. 
            Deve essere obbligatoriamente uno tra {queue, width, height}
        */
        printf("[ERRORE NEL PARSING DEL FILE: %s PER SINTASSI NON SUPPORTATA]; LINEA ERRORE: %d\n", ENVIRONMENT_FILENAME,__LINE__);
        exit(0);
    }
    
    

  
    return 1;

}

int digits(int x){

    /*
        Se è solo di una cifra restituisco subito
    */

    if(x >= 0 && x <= 9) return 1;

    short int res = 0;

    /*
        divido per 10 ad ogni ciclo per ogni cifra del numero
    */

    while(x > 0){
        res++;
        x /= 10;
    }

    return res;

}
