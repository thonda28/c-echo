#include <stdbool.h>

#ifndef UTILS_H
#define UTILS_H

#define BUFFER_SIZE 256

typedef enum
{
    LISTEN_SOCKET,
    CLIENT_SOCKET,
    SIGNAL_PIPE,
} SocketType;

typedef struct
{
    SocketType type;
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

SocketManager *new_socket_manager(int max_size);
SocketData *find_socket(SocketManager *manager, int socket_fd); // NO USE
SocketData *add_socket(SocketManager *manager, SocketType type, int socket_fd);
int get_socket_count(SocketManager *manager);
int remove_socket(SocketManager *manager, int socket_fd);
void free_socket_manager(SocketManager *manager);

int parse_port(const char *port_str);
int close_with_retry(int fd);
#endif
