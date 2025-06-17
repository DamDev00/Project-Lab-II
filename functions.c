#include "functions.h"

char* trim(char* line){
    int len = strlen(line);
    char* result = (char*)malloc(sizeof(char)*(len + 1)); 
    if (result == NULL) {
        printf("{MALLOC ERROR}\n");
        exit(MALLOC_ERROR); 
    }
    int c_result = 0;
    for (int i = 0; i < len; i++) {
        if (line[i] != ' ') {
            result[c_result++] = line[i];
        }
    }
    result[c_result] = '\0';
    return result;
}

void write_log_file(time_t timestamp, char* id, char* evento, char* desc){

    int fd;
    SCALL(fd, open(LOG_FILENAME, O_WRONLY | O_APPEND, 0644), "apertura file");

    struct tm* tm_info = localtime(&timestamp);

    char line_log[LENGTH_LINE];
    snprintf(line_log, sizeof(line_log), "[%d:%d:%d] [%s] [%s] [%s]\n", tm_info->tm_hour,tm_info->tm_min, tm_info->tm_sec, id, evento, desc);

    size_t bytes = write(fd, line_log, strlen(line_log));

    if(bytes == -1){
        perror("Errore nella scrittura del file");
        SCALL(fd, close(fd), "close");
        exit(FILE_ERROR);
    }

    SCALL(fd, close(fd), "close");
    return;

}
