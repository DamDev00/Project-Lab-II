#include "functions.h"
#include "parse_rescuers.h"

void print_emergencies(emergency_type_t* emergencies, int num);
emergency_type_t* parser_emergency(char* filename, int* num_emergency_type);
