#include "parse_env.h"
#include <mqueue.h>
#include <unistd.h>
#include <threads.h>
#include <stdatomic.h>
#include <semaphore.h>
#include <stdbool.h>

#define MAX_SIZE 1024
#define MAX_MSG 10
#define TOLLERANCE 20

#define MESSAGE_QUEUE "message queue"
#define EMERGENCY_STATUS "EMERGENCY_STATUS"
#define EMERGENCY_OPERATION "EMERGENCY_OPERATION"
#define RESCUERS_STATUS "RESCUER_STATUS"

typedef struct {
    env_t* environment;
    result_parser_rescuers* rp_rescuers;
} params_handler_queue_t;

typedef struct {
    int id_current_emergency;
    int time_to_manage;
    int x_emergency;
    int y_emergency;
    rescuer_digital_twin_t* twin;
    mtx_t lock;
    cnd_t cnd;
    char* current_emergency;
    bool is_been_called;
} rescuer_data_t;

typedef struct {
    int* id_arr;
    int tot_manage;
    int avalaible_resc;
} rescuers_t;

typedef struct {
    int id;
    int priority;
    char* desc;
} waiting_queue_t;

typedef struct {
    int id;
    int num_twins;
    bool miss_rescuers;
    bool was_in_waiting_queue;
    bool in_loading;
    cnd_t cnd_emergency;
    mtx_t lock_emergency;
    emergency_t* emergency;
    atomic_int rescuers_finished;
    atomic_int tot_rescuers_required;
} emergency_id_t;

typedef struct {
    params_handler_queue_t* params;
    int x;
    int y;
    int index_emergencies;
    emergency_type_t* emergency_avalaible;
    time_t timestamp;
} params_handler_emergency_t;

typedef struct {
    mtx_t* lock_operation_on_waiting_queue;
    int* waiting_queue_len;
    emergency_id_t** queue_emergencies;
    waiting_queue_t** waiting_queue;
} params_control_waiting_queue_t;

// RESCUER
int handle_rescuer(void* args);
int rescuers_is_avalaible(rescuer_digital_twin_t** rd_twins, int num_twins, rescuer_request_t* requests, int req_number, char* desc);
int barrier_rescuers(emergency_id_t* current ,atomic_int* count, atomic_int* tot_rescuers_required, mtx_t* mtx, cnd_t* cnd, bool is_active);
void free_rescuers_data(rescuer_data_t* rd, int num);
char* get_state_rescuer(rescuer_status_t status);
void free_locks_rescuers(rescuers_t** id_locks, int count);
int print_dt(void* args);

// EMERGENCY
emergency_t* set_new_emergency(params_handler_emergency_t* params_emergency);
emergency_id_t* add_emergency(int* id, emergency_t* emergency, emergency_id_t*** queue_emergencies);
int start_emergency(emergency_id_t* current_emergency);
int handler_queue_emergency(void* args);
void print_requests_emergencies(emergency_id_t** queue, int num);
int handler_emergency(void* args);
void free_emergency_avalaible(emergency_type_t* emergencies, int num);
void free_queue_emergencies(emergency_id_t** queue_emergencies, int num);
int status_emergency(void* args);
void print_waiting_emergencies(waiting_queue_t** queue, int len);

// WAITING QUEUE

void add_waiting_queue(emergency_id_t* id, waiting_queue_t*** waiting_queue, int* waiting_queue_len);
void remove_from_waiting_queue(emergency_id_t* emergency, waiting_queue_t*** waiting_queue, int* waiting_queue_len);
int control_waiting_queue(void* args);
int check_priority_waiting_queue(int current_index, waiting_queue_t*** waiting_queue, int waiting_queue_len, emergency_id_t*** queue_emergencies);
int handler_waiting_queue(void* args);
void print_waiting_emergencies(waiting_queue_t** queue, int len);
void free_waiting_queue(waiting_queue_t** waiting_queue, int num);

// UTILS

int get_priority_limit(int priority);
int compare(const void *a, const void *b);
int distance_manhattan(int x1, int x2, int y1, int y2, int speed);
int is_valid_request(emergency_request_t* request, int width, int height, char* problem, int* index_type_emergency, emergency_type_t* emergency_avalaible, int num_emergency_avalaible);
