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

int create_listen_sockets(const char *port_str, SocketManager *listen_socket_manager);
int handle_new_connection(int listen_sock, int epoll_fd, SocketManager *client_socket_manager);
int handle_client(int client_sock);

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

    // Create listen sockets
    SocketManager listen_socket_manager;
    init_socket_manager(&listen_socket_manager);
    if (create_listen_sockets(argv[1], &listen_socket_manager) == -1)
    {
        puts("Failed to create listen sockets\n");
        exit(1);
    }

    printf("Listening on port %s\n", argv[1]);

    // Create an epoll instance
    int epoll_fd;
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("server: epoll_create1()");
        close_all_sockets(&listen_socket_manager);
        exit(1);
    }

    struct epoll_event event;
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        int listen_sock = listen_socket_manager.sockets[i];
        if (listen_sock == -1)
        {
            continue;
        }

        // Set the listen socket to non-blocking mode
        int flags = fcntl(listen_sock, F_GETFL, 0);
        if (flags == -1)
        {
            perror("server: fcntl()");
            close_all_sockets(&listen_socket_manager);
            close(epoll_fd);
            exit(1);
        }
        if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            perror("server: fcntl()");
            close_all_sockets(&listen_socket_manager);
            close(epoll_fd);
            exit(1);
        }
        event.events = EPOLLIN;
        event.data.fd = listen_sock;

        // Add the listen socket to the epoll instance
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1)
        {
            perror("server: epoll_ctl()");
            close_all_sockets(&listen_socket_manager);
            close(epoll_fd);
            exit(1);
        }
    }

    struct epoll_event events[MAX_EVENTS];
    // Manage the client sockets using stack
    SocketManager client_socket_manager;
    init_socket_manager(&client_socket_manager);

    while (1)
    {
        // Wait for events indefinitely
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            perror("server: epoll_wait()");
            close_all_sockets(&listen_socket_manager);
            close(epoll_fd);
            exit(1);
        }

        for (int i = 0; i < nfds; i++)
        {
            // Check if the event is for the listen socket
            if (contains(listen_socket_manager.sockets, MAX_SOCKETS, events[i].data.fd))
            {
                int res = handle_new_connection(events[i].data.fd, epoll_fd, &client_socket_manager);
                if (res == -1)
                {
                    close(events[i].data.fd);
                    close(epoll_fd);
                }
            }
            // Check if the event is for a client socket
            else
            {
                int conn_sock = events[i].data.fd;
                // TODO: Manage the client socket using hash table
                if (contains(client_socket_manager.sockets, MAX_CLIENTS, conn_sock))
                {
                    int res = handle_client(conn_sock);
                    if (res == -1)
                    {
                        remove_socket(&client_socket_manager, conn_sock);
                        close(conn_sock);
                        close(epoll_fd);
                        exit(1);
                    }
                    else if (res == 0)
                    {
                        remove_socket(&client_socket_manager, conn_sock);
                        close(conn_sock);
                    }
                }
            }
        }
    }

    close_all_sockets(&listen_socket_manager);
    close(epoll_fd);

    return 0;
}

int create_listen_sockets(const char *port_str, SocketManager *listen_socket_manager)
{
    struct addrinfo hints, *res, *res0;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    if (getaddrinfo(NULL, port_str, &hints, &res0) != 0)
    {
        perror("server: getaddrinfo()");
        return -1;
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
            return -1;
        }

        add_socket(listen_socket_manager, listen_sock);
    }

    freeaddrinfo(res0);

    if (listen_socket_manager->top == MAX_SOCKETS - 1)
    {
        puts("server: failed to bind any sockets\n");
        return -1;
    }

    return 0;
}

int handle_new_connection(int listen_sock, int epoll_fd, SocketManager *client_socket_manager)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int conn_sock;

    if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len)) == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        else
        {
            perror("server: accept()");
            return -1;
        }
    }

    // Set the connection socket to non-blocking mode
    int flags = fcntl(conn_sock, F_GETFL, 0);
    if (flags == -1)
    {
        perror("server: fcntl(conn_sock)");
        close(conn_sock);
        return -1;
    }
    if (fcntl(conn_sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        perror("server: fcntl(conn_sock)");
        close(conn_sock);
        return -1;
    }

    // Fulfilled the maximum number of clients
    if (add_socket(client_socket_manager, conn_sock) == -1)
    {
        puts("No more room for clients\n");
        close(conn_sock);
        return 0;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = conn_sock;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_sock, &event) == -1)
    {
        perror("server: epoll_ctl()");
        close(conn_sock);
        return -1;
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

    return 0;
}

int handle_client(int client_sock)
{
    char buf[BUFFER_SIZE];
    ssize_t received_bytes;
    while ((received_bytes = recv(client_sock, buf, BUFFER_SIZE - 1, 0)) > 0)
    {
        printf("received_bytes: %ld\n", received_bytes);
        buf[received_bytes] = '\0'; // Null-terminate the string
        if (send(client_sock, buf, received_bytes, 0) == -1)
        {
            perror("server: send()");
            return -1;
        }
    }

    if (received_bytes == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 1;
        }
        else
        {
            perror("server: recv()");
            return -1;
        }
    }
    else if (received_bytes == 0)
    {
        puts("Connection closed\n");
        return 0;
    }

    return 1;
}
