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
