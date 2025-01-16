#include <stdbool.h>

#ifndef UTILS_H
#define UTILS_H

#define MAX_SOCKETS 30

typedef struct
{
    // TODO: dynamically allocate the array
    int sockets[MAX_SOCKETS];
    int free_indices[MAX_SOCKETS];
    int top;
} SocketManager;

void init_socket_manager(SocketManager *manager);
int add_socket(SocketManager *manager, int sock);
int remove_socket(SocketManager *manager, int sock);
int close_all_sockets(SocketManager *manager);

int parse_port(const char *port_str);
bool contains(int *array, int size, int value);
#endif
