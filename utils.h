#include <stdbool.h>

#ifndef UTILS_H
#define UTILS_H

#define BUFFER_SIZE 256

typedef struct
{
    int socket_fd;
    char buffer[BUFFER_SIZE]; // TODO: Use dynamic memory allocation for the buffer
} SocketData;

typedef struct
{
    SocketData *sockets;
    int *free_indices;
    int max_size;
    int top;
} SocketManager;

void init_socket_manager(SocketManager *manager, int max_size);
SocketData *find_socket(SocketManager *manager, int socket_fd);
int add_socket(SocketManager *manager, int socket_fd);
int remove_socket(SocketManager *manager, int socket_fd);
int close_all_sockets(SocketManager *manager);

int parse_port(const char *port_str);
#endif
