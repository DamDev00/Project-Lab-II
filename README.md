# Project-Lab-II

- Per ogni parser ho creato delle funzioni esterne per il loro corretto funzionamento. Oltre ai parser ho realizzato un client.c, per inviare le richieste da terminale, e un server.c per gestirle. Inoltre ci sono altri 2 file per il server tra cui utils_server.c, per avere un organizzazione più pulita tra i file, e debug_server.c che contiene delle funzioni che ho usato per il debug del programma (oltre al file events.log). Per ultimo c'è anche un file **functions.c** che contiene delle funzioni di supporto.
- Oltre alle strutture già presenti, ne ho aggiunte un paio per l'output, potendo avere a disposizione una struttura che mi consente di usare più strutture lavorate dai parser.

# Parse_rescuer.c

## result_parser_rescuers* parse_rescuers(char* filename) 
- Ho utilizzato una **fopen** per leggere le righe del file **rescuers.conf**, inizializzando un paio di variabili/strutture per il file **events.log** e per l'eventuale output della funzione.
Ho utilizzato la funzione **trim()** all'inizio per facilitare il parsing per poi verificare se la sintassi è corretta. Per ogni giro utilizzo le strutture **rescuer_type_t** e **rescuer_digital_twin** creando un nuovo soccorritore utilizzando la funzione **strtok()**, per poi aggiornare l'array dei gemelli digitali. L'output è il tipo di una struttura che ho aggiunto che contiene sia l'array dei gemelli digitali che dei soccorritori, con le loro grandezze.


# Parse_emergency_types.c

## emergency_type_t* parser_emergency(char* filename, int* num_emergency_type)
- Utilizzo **fopen** per leggere le righe del file **emergency.conf**, inizializzando un paio di variabili/strutture per il file **events.log** e per l'eventuale output della funzione. Ho utilizzato la funzione **trim()** all'inizio per facilitare il parsing per poi verificare se la sintassi è corretta. Utilizzo l'array dinamico **emergency_type_t* emergency_type** per tener conto delle possibili emergenze che possono  essere soddisfatte. All'inizio del parsing utilizzo due volte la funzione **strtok()** per il nome e la priorità. Successivamente utilizzo un for per prelevare i soccorritori nella forma <nome soccorritore><richiesti><tempo di gestione> per poi usarli per realizzare l'array dei soccorritori richiesti. Per questo ho utilizzato un'altra volta la funzione **strtok()** delimitando la stringa prima per **;** e poi per **,**, per un numero pari di volte al numero di soccorritori richiesti.
