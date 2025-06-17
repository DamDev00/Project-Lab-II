#include "parse_emergency_types.h"

typedef struct {
    char* queue_name;
    int x;
    int y;
} env_t;

env_t* parser_env(char* filename);
void print_env(env_t* env);
