#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define LENGTH_LINE 256
#define MALLOC_ERROR 0
#define FILE_ERROR 1
#define REALLOC_ERROR 2
#define RESCUERS_FILENAME "rescuers.conf"
#define EMERGENCY_FILENAME "emergency.conf"
#define ENVIRONMENT_FILENAME "env.conf"
#define LOG_FILENAME "events.log"

#define SCALL_ERROR -1
#define SCALL(r, c, e) do { if((r = c) == SCALL_ERROR) { perror(e); exit(EXIT_FAILURE); } } while(0)
#define SNCALL(r, c, e) do { if((r = c) == NULL) { perror(e); exit(EXIT_FAILURE); } } while(0)

void write_log_file(time_t timestamp, char* id, char* evento, char* desc);
char* trim(char* line);
int format_check_rescuers(char* line);
int format_check_emergency(char* line);

#endif
