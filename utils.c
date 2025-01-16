#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void init_socket_manager(SocketManager *manager)
{
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        manager->sockets[i] = -1;
        manager->free_indices[i] = i;
    }
    manager->top = MAX_SOCKETS - 1;
}

int add_socket(SocketManager *manager, int sock)
{
    if (manager->top < 0)
    {
        return -1;
    }
    int index = manager->free_indices[manager->top--];
    manager->sockets[index] = sock;
    return index;
}

int remove_socket(SocketManager *manager, int sock)
{
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        if (manager->sockets[i] == sock)
        {
            manager->sockets[i] = -1;
            manager->free_indices[++manager->top] = i;
            return 0;
        }
    }
    return -1;
}

int parse_port(const char *port_str)
{
    char *endptr;
    errno = 0;
    long port = strtol(port_str, &endptr, 10);
    if (endptr == port_str || *endptr != '\0' || errno == ERANGE || port <= 0 || port > 65535)
    {
        return -1;
    }
    return (int)port;
}
