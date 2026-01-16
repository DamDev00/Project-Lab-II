#include "parse_emergency_types.h"

// struttura aggiuntiva per il tipo di ritorno del parser_env

typedef struct {
    char* queue_name;
    int x;
    int y;
} env_t;

env_t* parser_env(char* filename);
void print_env(env_t* env);
