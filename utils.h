#include <stdbool.h>

#ifndef UTILS_H
#define UTILS_H

typedef struct
{
    int *sockets;
    int *free_indices;
    int max_size;
    int top;
} SocketManager;

void init_socket_manager(SocketManager *manager, int max_size);
int add_socket(SocketManager *manager, int sock);
int remove_socket(SocketManager *manager, int sock);
int close_all_sockets(SocketManager *manager);

int parse_port(const char *port_str);
bool contains(int *array, int size, int value);
#endif
