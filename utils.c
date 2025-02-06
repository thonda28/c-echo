#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

SocketManager *new_socket_manager(int max_size)
{
    SocketManager *manager = (SocketManager *)malloc(sizeof(SocketManager));
    if (manager == NULL)
    {
        perror("malloc(sizeof(SocketManager))");
        return NULL;
    }

    manager->sockets = (SocketData *)malloc(max_size * sizeof(SocketData));
    if (manager->sockets == NULL)
    {
        perror("malloc(max_size * sizeof(SocketData))");
        free(manager);
        return NULL;
    }
    manager->free_indices = (int *)malloc(max_size * sizeof(int));
    if (manager->free_indices == NULL)
    {
        perror("malloc(max_size * sizeof(int))");
        free(manager->sockets);
        free(manager);
        return NULL;
    }
    manager->max_size = max_size;
    manager->top = max_size - 1;

    for (int i = 0; i < max_size; i++)
    {
        manager->sockets[i].socket_fd = -1;
        manager->free_indices[i] = i;
    }
    return manager;
}

SocketData *add_socket(SocketManager *manager, SocketType type, int socket_fd)
{
    if (manager == NULL)
    {
        return NULL;
    }

    if (manager->top < 0)
    {
        return NULL;
    }
    int index = manager->free_indices[manager->top--];
    manager->sockets[index].type = type;
    manager->sockets[index].socket_fd = socket_fd;
    manager->sockets[index].buffer_start = 0;
    manager->sockets[index].buffer_end = 0;
    return &manager->sockets[index];
}

int get_socket_count(SocketManager *manager)
{
    if (manager == NULL)
    {
        return -1;
    }
    return (manager->max_size - 1) - manager->top;
}

int remove_socket(SocketManager *manager, int socket_fd)
{
    if (manager == NULL)
    {
        return -1;
    }

    for (int i = 0; i < manager->max_size; i++)
    {
        if (manager->sockets[i].socket_fd == socket_fd)
        {
            manager->sockets[i].socket_fd = -1;
            manager->free_indices[++manager->top] = i;
            return 0;
        }
    }
    return -1;
}

void free_socket_manager(SocketManager *manager)
{
    if (manager == NULL)
    {
        return;
    }

    for (int i = 0; i < manager->max_size; i++)
    {
        if (manager->sockets[i].socket_fd != -1)
        {
            close_with_retry(manager->sockets[i].socket_fd);
        }
    }
    free(manager->sockets);
    free(manager->free_indices);
    free(manager);
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

int close_with_retry(int fd)
{
    while (close(fd) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("close()");
        return -1;
    }
    return 0;
}
