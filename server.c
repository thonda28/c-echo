#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

#define BUFFER_SIZE 256
#define MAX_CLIENTS 30
#define MAX_EVENTS 10

void handle_new_connection(int listen_sock, int epoll_fd, int *client_sockets, int *free_indices, int *free_index_top);
void handle_client(int client_sock, int *client_sockets, int index);

int main(int argc, char **argv)
{
    // Check if the port number is provided
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Initialize the error number
    errno = 0;

    // Parse the port number
    long port = parse_port(argv[1]);
    printf("Port: %ld\n", port);

    int listen_sock;
    if ((listen_sock = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
    {
        perror("server: socket()");
        exit(1);
    }

    if (fcntl(listen_sock, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("server: fcntl(listen_sock)");
        close(listen_sock);
        exit(1);
    }

    struct sockaddr_in6 server_addr6;
    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = PF_INET6;
    server_addr6.sin6_port = htons(port);
    server_addr6.sin6_addr = in6addr_any;
    if (bind(listen_sock, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) == -1)
    {
        perror("server: bind()");
        close(listen_sock);
        exit(1);
    }

    if (listen(listen_sock, 0) == -1)
    {
        perror("server: listen()");
        close(listen_sock);
        exit(1);
    }

    // Create an epoll instance
    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("server: epoll_create1()");
        close(listen_sock);
        exit(1);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = listen_sock;

    // Add the listen socket to the epoll instance
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1)
    {
        perror("server: epoll_ctl()");
        close(listen_sock);
        close(epoll_fd);
        exit(1);
    }

    struct epoll_event events[MAX_EVENTS];
    // Manage the client sockets using stack
    int client_sockets[MAX_CLIENTS];
    int free_indices[MAX_CLIENTS];
    int free_index_top = MAX_CLIENTS - 1;

    // Initialize the client sockets and free indices
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sockets[i] = -1;
        free_indices[i] = i;
    }

    while (1)
    {
        // Wait for events indefinitely
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("server: epoll_wait()");
            close(listen_sock);
            close(epoll_fd);
            exit(1);
        }

        for (int i = 0; i < nfds; i++)
        {
            // Check if the event is for the listen socket
            if (events[i].data.fd == listen_sock)
            {
                handle_new_connection(listen_sock, epoll_fd, client_sockets, free_indices, &free_index_top);
            }
            // Check if the event is for a client socket
            else
            {
                int conn_sock = events[i].data.fd;
                // TODO: Manage the client socket using hash table
                for (int j = 0; j < MAX_CLIENTS; j++)
                {
                    if (client_sockets[j] == conn_sock)
                    {
                        handle_client(conn_sock, client_sockets, j);
                        if (client_sockets[j] == -1)
                        {
                            free_indices[++free_index_top] = j;
                        }
                        break;
                    }
                }
            }
        }
    }

    close(listen_sock);
    close(epoll_fd);

    return 0;
}

void handle_new_connection(int listen_sock, int epoll_fd, int *client_sockets, int *free_indices, int *free_index_top)
{
    struct sockaddr_in6 client_addr6;
    socklen_t addr_len = sizeof(client_addr6);
    int conn_sock;

    if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr6, &addr_len)) == -1)
    {
        if (errno == EAGAIN)
        {
            return;
        }
        else
        {
            perror("server: accept()");
            close(listen_sock);
            close(epoll_fd);
            exit(1);
        }
    }

    // Set the connection socket to non-blocking mode
    if (fcntl(conn_sock, F_SETFL, O_NONBLOCK) == -1)
    {
        perror("server: fcntl(conn_sock)");
        close(conn_sock);
        close(listen_sock);
        close(epoll_fd);
        exit(1);
    }

    // Fulfilled the maximum number of clients
    if (*free_index_top == -1)
    {
        printf("No more room for clients\n");
        close(conn_sock);
        return;
    }

    int index = free_indices[(*free_index_top)--];
    client_sockets[index] = conn_sock;

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = conn_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &event) == -1)
    {
        perror("server: epoll_ctl()");
        close(conn_sock);
        close(listen_sock);
        close(epoll_fd);
        exit(1);
    }

    char client_ip[INET6_ADDRSTRLEN];
    inet_ntop(PF_INET6, &client_addr6.sin6_addr, client_ip, sizeof(client_ip));
    printf("Connection from %s, %d\n", client_ip, ntohs(client_addr6.sin6_port));
}

void handle_client(int client_sock, int *client_sockets, int index)
{
    char buf[BUFFER_SIZE];
    ssize_t received_bytes;
    while ((received_bytes = recv(client_sock, buf, sizeof(buf) - 1, 0)) > 0)
    {
        printf("received_bytes: %ld\n", received_bytes);
        buf[received_bytes] = '\0'; // Null-terminate the string
        if (send(client_sock, buf, received_bytes, 0) == -1)
        {
            perror("server: send()");
            close(client_sock);
            client_sockets[index] = -1;
            exit(1);
        }
    }

    if (received_bytes == -1)
    {
        if (errno != EAGAIN)
        {
            perror("server: recv()");
            close(client_sock);
            client_sockets[index] = -1;
            exit(1);
        }
    }
    else if (received_bytes == 0)
    {
        printf("Connection closed\n");
        close(client_sock);
        client_sockets[index] = -1;
    }
}
