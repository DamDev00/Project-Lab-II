#include "data_structure.h"
#include "functions.h"
#ifndef PARSE_RESCUERS_H
#define PARSE_RESCUERS_H

typedef struct {
    rescuer_digital_twin_t** rd_twins;
    rescuer_type_t** rescuers_type;
    int num_twins;
    int num_rescuers;
} result_parser_rescuers;

void free_rescuers_digital_twins(rescuer_digital_twin_t** rd_twin, int n_twins);
void print_rescuers(rescuer_type_t** rescuers, int num);
result_parser_rescuers* parse_rescuers(char* filename);
void print_digitals_twins(rescuer_digital_twin_t** rd_twins, int num);
rescuer_type_t* find_rescuer(rescuer_type_t** rescuers, char* name, int num_rescuers);
void free_rescuers(rescuer_type_t** rescuers, int num);
#endif
