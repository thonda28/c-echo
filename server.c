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
#include <netdb.h>

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
    int port;
    if ((port = parse_port(argv[1])) == -1)
    {
        printf("Invalid port number: %s\n", argv[1]);
        exit(1);
    }
    printf("Port: %d\n", port);

    struct addrinfo hints, *res, *res0;
    int listen_sock_v4 = -1;
    int listen_sock_v6 = -1;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    if (getaddrinfo(NULL, argv[1], &hints, &res0) != 0)
    {
        perror("server: getaddrinfo()");
        exit(1);
    }

    for (res = res0; res != NULL; res = res->ai_next)
    {
        int listen_sock;
        if ((listen_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("server: socket()");
            continue;
        }

        // Set the socket option to reuse the address
        setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (res->ai_family == PF_INET6)
        {
            // Set the socket option to allow only IPv6 connections
            if (setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(int)) == -1)
            {
                perror("server: setsockopt()");
                close(listen_sock);
                continue;
            };
        }

        if (bind(listen_sock, res->ai_addr, res->ai_addrlen) == -1)
        {
            perror("server: bind()");
            close(listen_sock);
            continue;
        }

        if (listen(listen_sock, 5) == -1)
        {
            perror("server: listen()");
            close(listen_sock);
            exit(1);
        }

        if (res->ai_family == PF_INET)
        {
            listen_sock_v4 = listen_sock;
        }
        else if (res->ai_family == PF_INET6)
        {
            listen_sock_v6 = listen_sock;
        }
    }

    freeaddrinfo(res0);

    if (listen_sock_v4 == -1 && listen_sock_v6 == -1)
    {
        printf("server: failed to bind any sockets\n");
        exit(1);
    }

    printf("listen_sock_v4: %d\n", listen_sock_v4);
    printf("listen_sock_v6: %d\n", listen_sock_v6);

    // Create an epoll instance
    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("server: epoll_create1()");
        if (listen_sock_v4 != -1)
            close(listen_sock_v4);
        if (listen_sock_v6 != -1)
            close(listen_sock_v6);
        exit(1);
    }

    struct epoll_event event;
    if (listen_sock_v4 != -1)
    {
        if (fcntl(listen_sock_v4, F_SETFL, O_NONBLOCK) == -1)
        {
            perror("server: fcntl(listen_sock_v4)");
            close(listen_sock_v4);
            if (listen_sock_v6 != -1)
                close(listen_sock_v6);
            close(epoll_fd);
            exit(1);
        }
        event.events = EPOLLIN;
        event.data.fd = listen_sock_v4;

        // Add the listen socket to the epoll instance
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock_v4, &event) == -1)
        {
            perror("server: epoll_ctl(listen_sock_v4)");
            close(listen_sock_v4);
            if (listen_sock_v6 != -1)
                close(listen_sock_v6);
            close(epoll_fd);
            exit(1);
        }
    }

    if (listen_sock_v6 != -1)
    {
        if (fcntl(listen_sock_v6, F_SETFL, O_NONBLOCK) == -1)
        {
            perror("server: fcntl(listen_sock_v6)");
            close(listen_sock_v6);
            if (listen_sock_v4 != -1)
                close(listen_sock_v4);
            close(epoll_fd);
            exit(1);
        }
        event.events = EPOLLIN;
        event.data.fd = listen_sock_v6;

        // Add the listen socket to the epoll instance
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock_v6, &event) == -1)
        {
            perror("server: epoll_ctl(listen_sock_v6)");
            close(listen_sock_v6);
            if (listen_sock_v4 != -1)
                close(listen_sock_v4);
            close(epoll_fd);
            exit(1);
        }
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
            if (listen_sock_v4 != -1)
                close(listen_sock_v4);
            if (listen_sock_v6 != -1)
                close(listen_sock_v6);
            close(epoll_fd);
            exit(1);
        }

        for (int i = 0; i < nfds; i++)
        {
            // Check if the event is for the listen socket
            if (events[i].data.fd == listen_sock_v4 || events[i].data.fd == listen_sock_v6)
            {
                handle_new_connection(events[i].data.fd, epoll_fd, client_sockets, free_indices, &free_index_top);
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

    if (listen_sock_v4 != -1)
        close(listen_sock_v4);
    if (listen_sock_v6 != -1)
        close(listen_sock_v6);
    close(epoll_fd);

    return 0;
}

void handle_new_connection(int listen_sock, int epoll_fd, int *client_sockets, int *free_indices, int *free_index_top)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int conn_sock;

    if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len)) == -1)
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
    if (client_addr.ss_family == PF_INET)
    {
        struct sockaddr_in *client_addr4 = (struct sockaddr_in *)&client_addr;
        inet_ntop(PF_INET, &client_addr4->sin_addr, client_ip, sizeof(client_ip));
        printf("Connection from %s, %d\n", client_ip, ntohs(client_addr4->sin_port));
    }
    else if (client_addr.ss_family == PF_INET6)
    {
        struct sockaddr_in6 *client_addr6 = (struct sockaddr_in6 *)&client_addr;
        inet_ntop(PF_INET6, &client_addr6->sin6_addr, client_ip, sizeof(client_ip));
        printf("Connection from %s, %d\n", client_ip, ntohs(client_addr6->sin6_port));
    }
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
