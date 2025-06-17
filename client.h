#include "parse_env.h"
#include <mqueue.h>
#include <unistd.h>

int send_queue(char* filename, emergency_request_t* msg);
int send_with_parameters(char** parameters);
int send_with_file(char* file_emergency);
