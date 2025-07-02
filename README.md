# Project-Lab-II

- Oltre ai 3 Parser richiesti sono presenti due file.c/.h per la gestione delle emergenze, **client.c** e **server.c**, insieme a **functions.c** che contiene tre funzioni riservate ai parser ed una per l'intero sistema,  **utils_server.c** creato con l'obiettivo principale di avere un organizzazione più pulita con le funzioni che servono al server, e **debug_server.c** che contiene funzioni non richieste/necessarie ma che ho usato per il debugging del programma (oltre al file **events.log**).
  
- ogni file.c ha ovviamente un relativo file.h in cui inizialmente definisco delle macro, se non sono presenti, con il nome del file in grassetto per non avere problemi di ridefinizioni multiple (ovviamente non a tutti i file.h)

- tutte le **typedef struct** richieste le ho messe in un unico file **data_structure.h**. Inoltre ho aggiunto un bel pò di strutture per aiutarmi con gli eventuali output, e funzionamenti del programma, che sono presenti in dei file.h separati.

## Per ciascun parser

- Inizializzo l'id, l'event, e un array di caratteri utili alla gestione della scrittura nel file .log;
- Accedo a ciascun file con la **fopen()** verificando sempre se è andata a buon fine;
- Inizializzo degli array dinamici per la gestione della funzione, aiutandomi con l'eventuale output;
- Ogni riga del file la ottengo con la **fgets()** dove successivamente elimino gli spazi tra i caratteri della riga, semplificandomi il lavoro con la verifica della sintassi;
- Ad ogni ciclo utilizzo una **strtok()** con determinati delimitatori;
- Inifine l'output, in particolare per il **parse_rescuers.c** e **parse_env.c**, hanno un tipo **struct** aggiunto da me per avere a disposizione più di un elemento.

# functions.c
 - Contiene 4 funzioni tra cui:
   ## char* trim(char* line)
   - utilizzato per rimuovere gli spazi tra i caratteri semplificando il lavoro per la correzione della sintassi.
   ## void write_log_file(time_t timestamp, char* id, char* evento, char* desc)
   - utilizzato da ogni file.c per aggiornare il file.log
   ## int format_check_rescuers(char* line)
   - verifico la sintassi della BNF del file **rescuers.conf**
   ## int format_check_Emergency(char* line)
   - verifico la sintassi della BNF del file **emergency.conf**

# parse_rescuer.c

 ## rescuer_type_t* find_rescuer(rescuer_type_t** rescuers, char* name, int num_rescuers)
 - Verifico se, tra i soccorritori disponibili in **rescuers.conf**, esiste un soccorritore utilizzando il parametro **char* name**
 ## void free_rescuers_digital_twins(rescuer_digital_twin_t** rd_twin, int n_twins) e void free_rescuers(rescuer_type_t** rescuers, int num)
 - Libero dall'heap l'array di gemelli digitali e l'array dei soccorritori disponibili
## void print_rescuers(rescuer_type_t** rescuers, int num) e void print_digitals_twins(rescuer_digital_twin_t** rd_twins, int num)
- funzioni che stampano tutti i campi dei soccorritori e dei gemelli digitali
## result_parser_rescuers* parse_rescuers(char* filename)
- funzione principale per ottenere i soccorritori, in particolare inserisco in una struttura che ho aggiunto (resul_parser_rescuer) che contiene l'array dei soccorritori e gemelli digitali disponibili con le loro grandezze. 

# server.c

- Per gestire il server ho aggiunto delle strutture utili per la concorrenza e l'organizzazione del sistema.
Nel main inizializzo tutto il necessario per il server tra cui: inizializzare le variabili globali; eseguire il parsing dei 3 file passandoli per parametri al thread che gestisce la message queue; mandare in esecuzione un numero di thread pari al numero dei gemelli digitali disponibili che restano in esecuzione fino a quando la message queue è operativa; liberare la memoria dopo l'arresto della message queue.
- Ho dovuto aggiungere delle strutture utili principalmente per la sincronizzazione e per l'organizzazione dell'intero programma.
## Descrizione delle struct aggiuntive
- emergency_id_t: ho voluto creare questa struttura per identificare le emergenze in modo univoco con un intero, creando un array dinamico di questo tipo includendo tutte le emergenze che sono state inviate (VALIDE) per avere una panoramica completa di quello che è successo (potendo vedere quali emergenze sono state soddisfatte e non). Oltre all'id ci sono le variabili: **int num_twins** intero che identifica tutti i gemelli digitali che sono stati salvati nell'array **rescuers_dt** nella variabile di tipo **emergency_t**;
**bool was_in_waiting_queue** variabile che, inizialmente falsa, mi tiene conto se l'emergenza in questione è passata nella coda di attesa, utile per il debbugging e per la rimozione dalla coda; **bool in_loading** altra booleana utilizzata nella funzione che gestisce la coda d'attesa per verificare che, l'emergenza prelevata dalla coda d'attesa, non sia già in esecuzione; **mtx_t lock_emergency e cnd_t cnd_emergency** servono per la barrier. Dato che nel testo è stato specificato "ovvero avere tutti i mezzi necessari collocati sul luogo dell’emergenza, che a quel punto passa nello stato IN_PROGRESS" ho voluto farsì che un emergenza per essere eseguita, i soccorritori devono cominciare a lavorare insieme e successivamente agiscono indipendentemente per conto loro. Per cui, grazie alla **struct rescuers_data_t** che possiede l'id dell'emergenza da soddisfare, può accedere alla lock e cv, condivisa con altri soccorritori della stessa emergenza, per sincronizzarsi con la barrier; la variabile atomica **rescuers_finished** indica il numero corrente dei soccorritori che sono arrivati alla barrier, mentre **tot_rescuers_finished** indica il numero di soccorritori che devono essere presenti in quell'istante (anche queste due ovviamente servono per la barrier);

- waiting_queue_t: struttura che possiede l'id, priorità e descrizione dell'emergenza che è attualmente in attesa. Il campo fondamentale è l'id per identificare l'emergenza nell'array globale per le emergenze, potendo identificare correttamente l'emergenza che mi serve;

- params_handler_queue_t: struttura che contiene le variabili d'ambiente e l'array dei gemelli digitale e dei soccorritori, che verrà passata al thread che gestisce la message queue;

- rescuer_data_t: struttura da supporto per ogni gemello digitale, contiene: la lock e la cv per la sincronizzazione; le coordinate del punto d'emergenza (x,y); la descrizione dell'emergenza (aggiunta per comodità); il tempo che deve dedicare il soccorritore per il suo lavoro (time_to_manage); id_current_emergency è l'id dell'emergenza di cui si sta occupando, utilissima per poter prelevare informazioni di questa emergenza tramite l'array globale delle emergenze; infine **twin** in cui sono salvate le informazioni del gemello digitale, inizializzate nel main;

- 
