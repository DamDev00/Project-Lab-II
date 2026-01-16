#ifndef DATA_STRUCTURE_H
#define DATA_STRUCTURE_H

#include <time.h>

#define EMERGENCY_NAME_LENGTH 64

typedef enum {
    IDLE, EN_ROUTE_TO_SCENE, ON_SCENE, RETURNING_TO_BASE
} rescuer_status_t;

typedef struct {
    char* rescuer_type_name;
    int speed;
    int x;
    int y;
} rescuer_type_t;

typedef struct {
    int id;
    int x;
    int y;
    rescuer_type_t* rescuer;
    rescuer_status_t status;
} rescuer_digital_twin_t;

typedef struct {
    rescuer_type_t* type;
    int required_count;
    int time_to_manage;
} rescuer_request_t;

typedef struct {
   short priority;
   char* emergency_desc;
   rescuer_request_t* rescuers;
   int rescuers_req_number;
} emergency_type_t;

typedef enum {
    WAITING, ASSIGNED, IN_PROGRESS, PAUSED, COMPLETED, CANCELED, TIMEOUT
} emergency_status_t;

typedef struct {
    char emergency_name[EMERGENCY_NAME_LENGTH];
    int y;
    int x;
    time_t timestamp;
} emergency_request_t;

typedef struct {
    emergency_type_t* type;
    emergency_status_t status;
    int x;
    int y;
    time_t time;
    int rescuer_count;
    rescuer_digital_twin_t** rescuers_dt;
} emergency_t;

#endif
