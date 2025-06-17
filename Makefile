CC = gcc
CFLAGS = -Wall -g -pthread
OBJ = parse_env.o parse_emergency_types.o parse_rescuers.o utils_server.o functions.o

SERVER_OBJ = server.o $(OBJ) 
CLIENT_OBJ = client.o $(OBJ)

all: server client

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJ)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJ)

client.o: client.c client.h
	$(CC) $(CFLAGS) -c client.c

server.o: server.c server.h
	$(CC) $(CFLAGS) -c server.c

utils_server.o: utils_server.c server.h
	$(CC) $(CFLAGS) -c utils_server.c

parse_env.o: parse_env.c parse_env.h 
	$(CC) $(CFLAGS) -c parse_env.c

parse_emergency_types.o: parse_emergency_types.c parse_emergency_types.h
	$(CC) $(CFLAGS) -c parse_emergency_types.c

parse_rescuers.o: parse_rescuers.c parse_rescuers.h functions.h
	$(CC) $(CFLAGS) -c parse_rescuers.c

functions.o: functions.c functions.h
	$(CC) $(CFLAGS) -c functions.c

clean:
	rm -f *.o client server

.PHONY: all clean
