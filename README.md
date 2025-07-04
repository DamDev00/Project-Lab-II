# Project-Lab-II

- Oltre ai 3 Parser richiesti sono presenti due file.c/.h per la gestione delle emergenze, **client.c** e **server.c**, insieme a **functions.c** che contiene tre funzioni riservate ai parser ed una per l'intero sistema,  **utils_server.c** creato con l'obiettivo principale di avere un organizzazione più pulita con le funzioni che servono al server, e **debug_server.c** che contiene funzioni non richieste/necessarie ma che ho usato per il debugging del programma (oltre al file **events.log**).
  
- ogni file.c ha ovviamente un relativo file.h in cui inizialmente definisco delle macro, se non sono presenti, con il nome del file in grassetto per non avere problemi di ridefinizioni multiple (ovviamente non a tutti i file.h)

- tutte le **typedef struct** richieste le ho messe in un unico file **data_structure.h**. Inoltre ho aggiunto un bel pò di strutture per aiutarmi con gli eventuali output, e funzionamenti del programma, che sono presenti in dei file.h separati.

- La memoria viene liberata in un colpo solo nel server.c, dato che tutti i riferimenti passeranno ad essa.

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
- funzioni che stampano tutti i campi dei soccorritori e dei gemelli digitali. In particolare la funzione che stampa i gemelli digitali viene usata nel server per debbugging tenendo traccia periodicamente dello stato de gemelli digitali, in particolare per la posizione e lo stato.
## result_parser_rescuers* parse_rescuers(char* filename)
- funzione principale per ottenere i soccorritori, in particolare inserisco in una struttura che ho aggiunto (resul_parser_rescuer) che contiene l'array dei soccorritori e gemelli digitali disponibili con le loro grandezze.

# parse_emergency_types.c

- emergency_type_t* parser_emergency(char* filename, int* num_emergency_type): esegue il parsing del file **emergency.conf** restituendo un array contenente tutte le emergenze disponibili.
- void print_emergencies(emergency_type_t* emergencies, int num): stampa l'array delle emergenze disponibili.

# parse_env.c

- env_t* parser_env(char* filename): restituisce un tipo di una struct aggiunta (**env_t**) che contiene il nome della message queue e la grandezza della griglia. Per farsì che il parsing avvenga correttamente, ogni riga del file **env.conf** deve essere del tipo: queue=<string><matricola>, width=<int>, height=<int>. Le chiavi devono essere solamente quelle, come accennato dal testo del progetto.

# server.c

## Funzionamento
  - Il server inizia prelevando i gemelli digitali disponibili, le possibili emergenze che si possono soddisfare e le variabili d'ambiente.
  - Vengono avviati un numero di thread pari al numero di gemelli digitali, che infatti li rappresentano, e restano in esecuzione fino a quando la message queue è ancora operativa. Questi thread restano bloccati quando ancora non arriva qualcuno a chiamarli per un emergenza.
  - Quando il server riceve un emergenza, verifica se è valida controllando se il nome dell'emergenza è tra quelle che si possono soddisfare e se le coordinate rientrano nella griglia.
  - Se l'emergenza è valida viene avviato un thread per gestirla, impostando il tipo **emergency_t** e verificando, dopo che gli sono stati asssegnati tutti i tipi di gemelli digitali che servono, se può essere gestita in base alla presenza dei soccorritori;
  - Un emergenza può essere gestita solamente se esiste almeno un istanza per ogni tipo di soccorritore (ad esempio, se ho [Incendio][<int>]Pompieri:2,3; e [Pompieri][1][<int>][//] l'emergenza può essere gestita lo stesso anche se non ci sono tutte le istanze) ma, ovviamente, ad ogni gemello, per un determinato tipo di soccorritore, gli deve essere distribuito del tempo in più per poter ricoprire il tempo che avrebbero dovuto impiegare i soccorritori mancanti.
  - L'emergenza viene scartata quindi solo quando:
    1) il tempo stimato di arrivo dalla base di almeno un soccorritore è maggiore dal tempo che ci si dovrebbe mettere in base alla priorità;
    2) non è disponibile nemmeno un istanza di un determinato tipo di soccorritore per l'emergenza. Ad esempio, se ho [Incendio][<int>]Pompieri:2,3; e [Pompieri][0][<int>][//] l'emergenza va scartata.
  - Una volta aver completato il settaggio dell'emergenza e aver verificato che si può soddisfare, viene avviata la funzione **start_emergency** in cui, attraverso un ciclo poco dopo l'inizio della funzione, vengono iterati tutte le istanze disponibili dei gemelli per ogni tipo id soccorritore necessario e, se per un tipo di soccorritore non c'è nemmeno un istanza disponibile al momento, l'emergenza va nella coda d'attesa.
  - per l'occupazione dei gemelli digitali ho aggiunto una struttura **rescuer_t** che serve per ricordare gli id dei gemelli che occupo al momento, ci sono due possibili casi:
    1) se ci sono 0 istanze disponibili per un soccorritore, oltre ad aggiungere l'emergenza nella coda di attesa, libero le lock che avevo occupato dei gemelli digitali precedenti, evitando cosi il deadlock con altre emergenze a venire;
    2) se per ogni soccorritore necessario esiste c'è almeno un istanza disponibile, allora l'emergenza può essere avviata dopo aver occupato tutti i lock dei gemelli digitali che servono. Se per un determinato tipo di soccorritore ci sono tutte le istanze richieste, allora per ogni gemello, sempre di quel tipo, verrà assegnato un tot tempo pari a quello specificato nel file **emergency.conf**, altrimenti ad ogni gemello di quel tipo verrà aggiunto del tempo in più per coprire il tempo dei soccorritori assenti;
  - per quanto riguarda le emergenze nella coda di attesa, c'è un thread avviato nel main che si occupa di gestire le emergenze in attesa, bloccato inizialmente da un semaforo globale, che può procedere quando c'è almeno un emergenza in attesa. In alcune parti del programma viene fatto un controllo per verificare che continui a sorvegliare la coda d'attesa quando c'è almeno un elemento. Il procedimento viene fatto controllando il valore attuale del counter del semaforo e, se è uguale a 0 mentre c'è almeno un elemento in coda, allora viene portato a 1.
  - Tutte le operazioni che si fanno sull'array delle emergenze in generale, e sull'array delle emergenze in attesa, vengono fatte in modo sicuro evitando fault e problemi vari per via della protezione da parte di due lock distinte: uno per la coda d'attesa e l'altro per le emergenze globali.
  - Ogni soccorritore, per essere chiamato per un emergenza, deve essere necessariamente alla sua base.
  - Infine, ho voluto creare due funzioni per l'ottimizzazione del sistema, che sono **control_waiting_queue** e **check_priority_waiting_queue** di cui se ne parlerà più avanti.

- Ho dovuto aggiungere delle strutture utili principalmente per la sincronizzazione e per l'organizzazione dell'intero programma.
## Descrizione delle struct aggiuntive
- emergency_id_t: ho voluto creare questa struttura per identificare le emergenze in modo univoco con un intero, creando un array dinamico di questo tipo includendo tutte le emergenze che sono state inviate (VALIDE) per avere una panoramica completa di quello che è successo (potendo vedere quali emergenze sono state soddisfatte e non). Oltre all'id ci sono le variabili: **int num_twins** intero che identifica tutti i gemelli digitali che sono stati salvati nell'array **rescuers_dt** nella variabile di tipo **emergency_t**;
**bool was_in_waiting_queue** variabile che, inizialmente falsa, mi tiene conto se l'emergenza in questione è passata nella coda di attesa, utile per il debbugging e per la rimozione dalla coda; **bool in_loading** altra booleana utilizzata nella funzione che gestisce la coda d'attesa per verificare che, l'emergenza prelevata dalla coda d'attesa, non sia già in esecuzione; **mtx_t lock_emergency e cnd_t cnd_emergency** servono per la barrier. Dato che nel testo è stato specificato "ovvero avere tutti i mezzi necessari collocati sul luogo dell’emergenza, che a quel punto passa nello stato IN_PROGRESS" ho voluto farsì che un emergenza per essere eseguita, i soccorritori devono cominciare a lavorare insieme e successivamente agiscono indipendentemente per conto loro. Per cui, grazie alla **struct rescuers_data_t** che possiede l'id dell'emergenza da soddisfare, può accedere alla lock e cv, condivisa con altri soccorritori della stessa emergenza, per sincronizzarsi con la barrier; la variabile atomica **rescuers_finished** indica il numero corrente dei soccorritori che sono arrivati alla barrier, mentre **tot_rescuers_finished** indica il numero di soccorritori che devono essere presenti in quell'istante (anche queste due ovviamente servono per la barrier);

- waiting_queue_t: struttura che possiede l'id, priorità e descrizione dell'emergenza che è attualmente in attesa. Il campo fondamentale è l'id per identificare l'emergenza nell'array globale per le emergenze, potendo identificare correttamente l'emergenza che mi serve;

- params_handler_queue_t: struttura che contiene le variabili d'ambiente e l'array dei gemelli digitale e dei soccorritori, che verrà passata al thread che gestisce la message queue;

- rescuer_data_t: struttura da supporto per ogni gemello digitale, contiene: la lock e la cv per la sincronizzazione; le coordinate del punto d'emergenza (x,y); la descrizione dell'emergenza (aggiunta per comodità); il tempo che deve dedicare il soccorritore per il suo lavoro (time_to_manage); id_current_emergency è l'id dell'emergenza di cui si sta occupando, utilissima per poter prelevare informazioni di questa emergenza tramite l'array globale delle emergenze; infine **twin** in cui sono salvate le informazioni del gemello digitale, inizializzate nel main;

- params_handler_emergency_t e params_control_waiting_queue_t: strutture d'appoggio per il passaggio dei parametri a due funzioni eseguite da thread.

## Funzioni

### handler_queue_emergency(void* args): Si occupa della gestione della message queue ricevendo le emergenze attraverso la struct **emergency_request_t**, verifica se è valida in termini di nome e coordinate e, se cosi fosse, viene mandato in esecuzione un thread che gestisce il seguito.
### int handler_emergency(void* args): si occupa del settaggio dell'emergenza ed esegue una funzione (rescuers_is_avalaible) che verifica se è possibile gestire l'emergenza in termini di soccorritori, se non esiste nessuna istanza di un determinato soccorritore, non può essere gestita.
### int start_emergency(emergency_id_t* current_emergency): si occupa di provare ad avviare l'emergenza. Inizialmente vengono iterati i gemelli digitali richiesti per quell'emergenza e se c'è almeno un istanza disponibile per ogni soccorritore allora si può avviare. Prima di avviarla viene verificata se esiste un soccorritore che non può arrivare in tempo per gestire l'emergenza, in quel caso viene scartata passando allo stato TIMEOUT (ovviamente se la priorità è diversa da 0), se sono in grado di arrivare in tempo l'emergenza viene avviata risvegliando ogni gemello digitale assegnato per l'emergenza settando tutti i campi del tipo **rescuers_data_t** per ogni gemello.
### void free_locks_rescuers(rescuers_t** id_locks, int count): questa funzione viene chiamata per rilasciare i lock attualmente acquisiti dei gemelli digitali quando un emergenza non può essere avviata a causa di distanze del soccorritore, oppure per mancanza di tutte le istanze di un soccorritore. Funzione fondamentale per evitare il deadlock.
### int handle_rescuer(void* args): permette di gestire indipendentemente ogni gemello digitale, resta in esecuzione fino a quando la message queue è operativa.
### int handler_waiting_queue(void* args): funzione che viene avviata da un thread nel main che si occupa di gestire le emergenze in attesa. E' gestita da un semaforo definito globalmente dove in alcune zone del programma viene verificato il valore attuale del counter del semaforo e, se è uguale a 0 mentre ci sono delle emergenze in attesa, allora viene incrementato ad 1. Quest'operazione viene protetta dalla lock **lock_operation_on_waiting_queue** in modo da non far andare ad un valore maggiore di 1 il valore del counter.
### int control_waiting_queue(void* args): funzione opzionale di ottimizzazione. Resta in esecuzione fino a quando la message queue è operativa e controlla periodicamente, dopo un tot di secondi, se le emergenze in attesa possono essere soddisfatte in termini di distanza di qualche soccorritore, in caso contrario vengono eliminate dalla coda d'attesa prima ancora che vengano prelevate. L'idea è quella di mantenere pulita e leggera la coda d'attesa senza tenere emergenze inutili che non possono essere più soddisfatte.

# Utils_server.c
- file.c pensato per tenere un organizzazione più pulita che contiene delle funzioni necessarie per il funzionamento del server.

  ## Funzioni
  - int barrier_rescuers(emergency_id_t* current ,atomic_int* count, atomic_int* tot_rescuers_required, mtx_t* mtx, cnd_t* cnd, bool is_active): barrier che serve per far cominciare a lavorare i gemelli nello stesso momento. Inizialmente, il programma, era implementato in modo che ogni gemello appena arrivato al posto d'emergenza, indipendentemente dagli altri gemelli, iniziava a lavorare, ma siccome nel testo viene accennato che lo stato dell'emergenza passa nello stato IN_PROGRESS nel momento in cui tutti sono collocati nella zona d'emergenza, allora ho implementato questa barrier. Dopo la barrier i gemelli si comportano in modo indipendente.
  - int get_priority_limit(int priority): restituisce il tempo massimo entro cui bisogna cominciare a gestire l'emergenza in base alla priorità;
  - int distance_manhattan(int x1, int x2, int y1, int y2, int speed): tempo di arrivo stimato di un soccorritore;
  - int rescuers_is_avalaible(rescuer_digital_twin_t** rd_twins, int num_twins, rescuer_request_t* requests, int req_number, char* desc): verifica se sono disponibili i soccorritori per gestire l'emergenza. Può restituire 1,0 e -1 a seconda dei casi:
    1) (-1) se non esiste nessuna istanza per un determinato tipo di
          soccorritore, in questo caso l'emergenza va scartata.
          (ESEMPIO: [Incedio][...]Pompieri:4,2 - [Pompieri][0][...][...;...])

    2) 0 se esiste almeno un istanza per ogni soccorritore necessario,
       ma non tutte quelle richieste per uno o più soccorritori
       (ESEMPIO: [Incedio][...]Pompieri:4,2 - [Pompieri][2][...][...;...])
       in questo caso, ai soli

    3) 1 se esistono tutte le istanze richieste per ogni tipo di soccorritore
       (ESEMPIO: [Incedio][...]Pompieri:4,2 - [Pompieri][4][...][...;...])
       in questo caso, ai soli
  - int compare(const void *a, const void *b): funzione utilizzata per riordinare la coda d'attesa, dopo l'inserimento di una nuova emergenza, in base alla priorità
  - void add_waiting_queue(emergency_id_t* id, waiting_queue_t*** waiting_queue, int* waiting_queue_len): funzione per aggiungere, in fondo, un emergenza nella coda d'attesa.
  - void remove_from_waiting_queue(emergency_id_t* emergency, waiting_queue_t*** waiting_queue, int* waiting_queue_len): funzione per rimuovere un'emergenza dalla coda d'attesa.
  - emergency_t* set_new_emergency(params_handler_emergency_t* params_emergency, int* twins): settaggio dell'emergenza corrente, in particolare imposto il tipo **emergency_t**.
  - emergency_id_t* add_emergency(int* id, emergency_t* emergency, emergency_id_t*** queue_emergencies): aggiungo un emergenza nell'array delle emergenze totali (valide).
  - int is_valid_request(emergency_request_t* request, int width, int height, char* problem, int* index_type_emergency, emergency_type_t* emergency_avalaible, int num_emergency_avalaible): verifico che l'emergenza appena arrivata sia valida in termini di posizione e di disponibilità dell'emergenza. Nel caso in cui non sia valida, viene restituito 0 scrivendo il problema nella variabile **char* problem**. Se è valida restituisce 1 impostando il valore dell'indice delle emergenze potendo identificare che tipo di emergenza devo gestire nell'array delle emergenze.
  - int check_priority_waiting_queue(int current_index, waiting_queue_t*** waiting_queue, int waiting_queue_len, emergency_id_t*** queue_emergencies): questa è un'altra funzione di ottimizzazione (opzionale). Durante l'esecuzione, se ci sono più elementi nella coda d'attesa è probabile che ci siano delle emergenze diverse ma con la stessa priorità che potrebbero essere eseguite. Quindi questa funzione è pensata per visionare delle emergenze diverse in attesa, con la stessa priorità di quella al primo posto, che potrebbero essere avviate restituendo l'indice dell'emergenza nell'array globale. Permette di alleggerire la coda.
  - Il resto delle funzioni serve per liberare la memoria.
 
# debug_server.c
- file opzionale che contiene delle funzioni che utilizzo per il debbugging del programma (oltre al file.log). In particolare, contiene una funzione per stampare le emergenze in attesa e un'altra per stampare lo stato attuale dei gemelli digitali, principalmente per tenere traccia della loro posizione e del loro status.

# client.c

- E' possibile inviare da terminale una singola emergenza o più emergenze contenute in un file di estensione .txt o .conf.
- Alternativamente, il programma può essere avviato senza alcun tipo di parametro, semplicemente finisce in un ciclo in cui viene fornito un menù che permette di fare le stesse cose che ho citato in precedenza, con la differenza che il client rimane in esecuzione fino a quando non viene digitato un certo valore.
- E' dal client che è possibile far terminare la message queue.

## Istruzioni
- Sintassi inviare una singola emergenza da terminale: ./<nome file client> <nome emergenza> <x> <y> <delay>.
- Sintassi per inviare più emergenze contenute in un file: ./<nome file client> -f <nome file>.{txt o conf}.
- Se si vuole inviare le emergenze facendo rimanere in esecuzione il client, basta avviare il programma digitando solamente ./<nome file client> seguendo le istruzioni del menù.
