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
int add_listen_sockets_to_epoll(int epoll_fd, SocketManager *listen_socket_manager);
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

    // Parse the port number
    if (parse_port(argv[1]) == -1)
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

    // Add the listen sockets to the epoll instance
    if (add_listen_sockets_to_epoll(epoll_fd, &listen_socket_manager) == -1)
    {
        close_all_sockets(&listen_socket_manager);
        close(epoll_fd);
        exit(1);
    }

    puts("Monitoring for events...");

    struct epoll_event events[MAX_EVENTS];
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
                int listen_sock = events[i].data.fd;
                if ((handle_new_connection(listen_sock, epoll_fd, &client_socket_manager)) == -1)
                {
                    close(listen_sock);
                    close(epoll_fd);
                }
            }
            // Check if the event is for a client socket
            else if (contains(client_socket_manager.sockets, MAX_SOCKETS, events[i].data.fd))
            {
                int client_sock = events[i].data.fd;
                int result = handle_client(client_sock);
                if (result == -1)
                {
                    remove_socket(&client_socket_manager, client_sock);
                    close(client_sock);
                    close(epoll_fd);
                    exit(1);
                }
                else if (result == 0)
                {
                    remove_socket(&client_socket_manager, client_sock);
                    close(client_sock);
                }
            }
            // Unreachable
            else
            {
                puts("Unknown socket\n");
                close(events[i].data.fd);
                close(epoll_fd);
                exit(1);
            }
        }
    }

    close_all_sockets(&listen_socket_manager);
    close(epoll_fd);

    return 0;
}

/**
 * @brief Create and bind listening sockets for the server.
 *
 * This function creates and binds listening sockets for both IPv4 and IPv6, sets the sockets
 * to reuse the address, and adds them to the provided socket manager.
 *
 * @param[in] port_str The port number as a string.
 * @param[in] listen_socket_manager The socket manager for listening sockets.
 * @return The status of the socket creation and binding process.
 * @retval 0 The sockets were successfully created and bound.
 * @retval -1 An error occurred during the process.
 */
int create_listen_sockets(const char *port_str, SocketManager *listen_socket_manager)
{
    struct addrinfo hints, *res, *res0;
    int yes = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // For wildcard IP address

    int getaddrinfo_result;
    if ((getaddrinfo_result = getaddrinfo(NULL, port_str, &hints, &res0)) != 0)
    {
        printf("server: getaddrinfo(): %s(%d)", gai_strerror(getaddrinfo_result), getaddrinfo_result);
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
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("server: setsockopt(SOL_SOCKET, SO_REUSEADDR)");
            close(listen_sock);
            continue;
        }

        if (res->ai_family == PF_INET6)
        {
            // Set the socket option to allow only IPv6 connections
            if (setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(int)) == -1)
            {
                perror("server: setsockopt(IPPROTO_IPV6, IPV6_V6ONLY)");
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

/**
 * @brief Add the listening sockets to the epoll instance.
 *
 * This function sets the listening sockets to non-blocking mode and adds them to the epoll instance.
 *
 * @param[in] epoll_fd The file descriptor of the epoll instance.
 * @param[in] listen_socket_manager The socket manager for listening sockets.
 * @return The status of the process.
 * @retval 0 The sockets were successfully added to the epoll instance.
 * @retval -1 An error occurred during the process.
 */
int add_listen_sockets_to_epoll(int epoll_fd, SocketManager *listen_socket_manager)
{
    struct epoll_event event;
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        int listen_sock = listen_socket_manager->sockets[i];
        if (listen_sock == -1)
        {
            continue;
        }

        // Set the listen socket to non-blocking mode
        int flags = fcntl(listen_sock, F_GETFL, 0);
        if (flags == -1)
        {
            perror("server: fcntl()");
            return -1;
        }
        if (fcntl(listen_sock, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            perror("server: fcntl()");
            return -1;
        }
        event.events = EPOLLIN;
        event.data.fd = listen_sock;

        // Add the listen socket to the epoll instance
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &event) == -1)
        {
            perror("server: epoll_ctl()");
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Handle a new connection on the listening socket.
 *
 * This function accepts a new connection on the listening socket, sets the new connection
 * socket to non-blocking mode, adds it to the epoll instance, and registers it with the
 * client socket manager.
 *
 * @param[in] listen_sock The file descriptor of the listening socket.
 * @param[in] epoll_fd The file descriptor of the epoll instance.
 * @param[in] client_socket_manager The socket manager for client sockets.
 * @return The status of the new connection.
 * @retval 0 The connection was successfully accepted and registered.
 * @retval -1 An error occurred during the process.
 */
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
        if (inet_ntop(PF_INET, &client_addr4->sin_addr, client_ip, sizeof(client_ip)) == NULL)
        {
            perror("server: inet_ntop(PF_INET)");
            return -1;
        }
        printf("Connection from %s, %d\n", client_ip, ntohs(client_addr4->sin_port));
    }
    else if (client_addr.ss_family == PF_INET6)
    {
        struct sockaddr_in6 *client_addr6 = (struct sockaddr_in6 *)&client_addr;
        if (inet_ntop(PF_INET6, &client_addr6->sin6_addr, client_ip, sizeof(client_ip)) == NULL)
        {
            perror("server: inet_ntop(PF_INET6)");
            return -1;
        }
        printf("Connection from %s, %d\n", client_ip, ntohs(client_addr6->sin6_port));
    }

    return 0;
}

/**
 * @brief Handle the client socket
 *
 * This function handles communication with a connected client socket. It reads data from the client,
 * processes it, and sends a response back to the client. The function continues to read and send data
 * until the client disconnects or an error occurs.
 *
 * @param[in] client_sock The file descriptor of the client socket.
 * @return The status of the client connection.
 * @retval 1 The client is still connected.
 * @retval 0 The client has disconnected.
 * @retval -1 An error occurred during communication.
 */
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
