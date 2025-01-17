#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

void init_socket_manager(SocketManager *manager, int max_size)
{
    manager->sockets = (int *)malloc(max_size * sizeof(int));
    manager->free_indices = (int *)malloc(max_size * sizeof(int));
    manager->max_size = max_size;
    manager->top = max_size - 1;

    for (int i = 0; i < max_size; i++)
    {
        manager->sockets[i] = -1;
        manager->free_indices[i] = i;
    }
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
    for (int i = 0; i < manager->max_size; i++)
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

int close_all_sockets(SocketManager *manager)
{
    for (int i = 0; i < manager->max_size; i++)
    {
        if (manager->sockets[i] != -1)
        {
            close(manager->sockets[i]);
        }
    }
    free(manager->sockets);
    free(manager->free_indices);
    return 0;
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

bool contains(int *array, int size, int value)
{
    for (int i = 0; i < size; i++)
    {
        if (array[i] == value)
        {
            return true;
        }
    }
    return false;
}
