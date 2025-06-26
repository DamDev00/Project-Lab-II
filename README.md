# Project-Lab-II

- Oltre ai 3 Parser richiesti sono presenti due file.c/.h per la gestione delle emergenze, **client.c** e **server.c**, insieme a **functions.c** che contiene tre funzioni riservate ai parser ed una per l'intero sistema,  **utils_server.c** creato con l'obiettivo principale di avere un organizzazione più pulita con le funzioni che servono al server, e **debug_server.c** che contiene funzioni non richieste/necessarie ma che ho usato per il debugging del programma (oltre al file **events.log**).
  
- ogni file.c ha ovviamente un relativo file.h in cui inizialmente definisco delle macro, se non sono presenti, con il nome del file in grassetto per non avere problemi di ridefinizioni multiple (ovviamente non a tutti i file.h)

- tutte le **typedef struct** richieste le ho messe in un unico file **data_structure.h**. Inoltre ho aggiunto un bel pò di strutture per aiutarmi con gli eventuali output, e funzionamenti del programma, che sono presenti in dei file.h separati.

