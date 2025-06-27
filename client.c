#include "client.h"

int send_queue(char* filename,  emergency_request_t* msg){

    env_t* env = parser_env(filename);

    char mq_name[LENGTH_LINE];
    mq_name[0] = '/';
    strncpy(&mq_name[1], env->queue_name, LENGTH_LINE-2);
    mq_name[LENGTH_LINE-1] = '\0';
    
    mqd_t mq;

    if((mq = mq_open(mq_name, O_WRONLY)) == -1){
        perror("mq_open");
        return 0;
    } 

    if (mq_send(mq ,(char*)msg, 1024, 0) == -1) {
			perror("mq_send");
			return 0;
	}

    mq_close(mq);

    return 1;

}

int send_with_parameters(char** parameters){

     emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));

    if(msg == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    strcpy(msg->emergency_name, parameters[1]);
    msg->x = atoi(parameters[2]);
    msg->y = atoi(parameters[3]);
    msg->timestamp = time(NULL);
    msg->timestamp = msg->timestamp + atoi(parameters[4]);

    return send_queue(ENVIRONMENT_FILENAME, msg);
}

int send_with_file(char* file_emergency){
    
    char* copy = strdup(file_emergency);
    char* tok = strtok(file_emergency, ".");
    if(strcmp(copy, tok) == 0){
        printf("ERRORE NELL'INSERIMETO DEI PARAMETRI\n");
        return -1;
    }
    tok = strtok(NULL, " ");
    
    if(strcmp(tok, "txt") != 0 && strcmp(tok, "conf") != 0){
        printf("il file deve avere estenzione {txt, conf}\n");
        return 0;
    }

    FILE* file = fopen(copy, "r");

    if(file == NULL){
        printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
        exit(MALLOC_ERROR);
    }

    char line[LENGTH_LINE];

    while(fgets(line, LENGTH_LINE, file)){

        emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));

        if(msg == NULL){
            printf("{type error: MALLOC_ERROR; line: %d; file: %s}\n",__LINE__,__FILE__);
            exit(MALLOC_ERROR);
        }

        int i = 1;
        for(char* t = strtok(line, " "); t != NULL; t = strtok(NULL, " "), i++){
            switch(i){
                case 1: strcpy(msg->emergency_name, t); break;
                case 2: msg->x = atoi(t); break;
                case 3: msg->y = atoi(t); break;
                case 4: {
                    msg->timestamp = time(NULL); 
                    msg->timestamp = msg->timestamp + atoi(t); 
                } break;
            }
        }

        if(send_queue(ENVIRONMENT_FILENAME, msg) != 1){
            printf("errore nell'invio delle emergenze!\n");
            exit(0);
        }

    }

    printf("invio delle emergenze andata a buon fine\n");
    return 1;
        
}


int main(int argc, char** argv){

    if(strcmp(argv[1], "STOP") == 0){
        char* stop = "STOP";
        emergency_request_t* msg = (emergency_request_t*)malloc(sizeof(emergency_request_t));
        strcpy(msg->emergency_name, stop);
        msg->x = -1;
        msg->y = -1;
        msg->timestamp = 0;
        if(send_queue(ENVIRONMENT_FILENAME, msg) != 1){
            printf("errore nell'invio delle emergenze!\n");
            exit(0);
        }
    }

    if(argc != 5 && argc != 3){
        printf("errore nell'inserimento dei parametri\n");
        exit(0);
    } else if(argc == 3){

        if(strcmp(argv[1], "-f") != 0){
            printf("errore nell'inserimento dei parametri\n");
            exit(0);
        } else {

            if(send_with_file(argv[2]) != 1){
                printf("{ERROR}\n");
                exit(0);
            } else printf("invio delle emergenze tramite file andata a buonfine!\n");
        }

    } else if(argc == 5){

        if(send_with_parameters(argv) != 1){
            printf("{ERROR}\n");
            exit(0);
        } else { printf("invio dell'emergenza andata a buon fine!\n"); }

    }


    return 0;
}
