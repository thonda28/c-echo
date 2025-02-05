#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
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
#define MAX_LISTENS 20
#define MAX_CLIENTS 30
#define MAX_EVENTS 10

int pipe_fds[2];

int create_listen_sockets(const char *port_str, SocketManager *listen_socket_manager);
int add_listen_sockets_to_epoll(int epoll_fd, SocketManager *listen_socket_manager);
int add_pipe_to_epoll(int epoll_fd);
int handle_new_connection(int listen_socket_fd, int epoll_fd, SocketManager *client_socket_manager);
int handle_client(SocketData *client_socket_data, struct epoll_event event);
void handle_sigint(int sig);

int main(int argc, char **argv)
{
    // Declare variables that need to be cleaned up later
    int epoll_fd = -1;
    SocketManager *listen_socket_manager = NULL;
    SocketManager *client_socket_manager = NULL;
    int exit_code = 0;

    // Check if the port number is provided
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit_code = 1;
        goto cleanup;
    }

    // Parse the port number
    if (parse_port(argv[1]) == -1)
    {
        printf("Invalid port number: %s\n", argv[1]);
        exit_code = 1;
        goto cleanup;
    }

    // Create listen sockets
    listen_socket_manager = new_socket_manager(MAX_LISTENS);
    if (create_listen_sockets(argv[1], listen_socket_manager) == -1)
    {
        fputs("Failed to create listen sockets\n", stderr);
        exit_code = 1;
        goto cleanup;
    }

    printf("Listening on port %s\n", argv[1]);

    // Create an epoll instance
    if ((epoll_fd = epoll_create1(0)) == -1)
    {
        perror("server: epoll_create1()");
        exit_code = 1;
        goto cleanup;
    }

    // Add the listen sockets to the epoll instance
    if (add_listen_sockets_to_epoll(epoll_fd, listen_socket_manager) == -1)
    {
        fputs("Failed to add listen sockets to epoll\n", stderr);
        exit_code = 1;
        goto cleanup;
    }

    // Create a pipe for signal handling
    if (pipe(pipe_fds) == -1)
    {
        perror("server: pipe()");
        exit_code = 1;
        goto cleanup;
    }

    // Add the pipe to the epoll instance
    if (add_pipe_to_epoll(epoll_fd) == -1)
    {
        fputs("Failed to add pipe to epoll\n", stderr);
        exit_code = 1;
        goto cleanup;
    }

    puts("Monitoring for events...");

    struct epoll_event events[MAX_EVENTS];
    client_socket_manager = new_socket_manager(MAX_CLIENTS);
    while (1)
    {
        // Wait for events indefinitely
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("server: epoll_wait()");
            exit_code = 1;
            goto cleanup;
        }

        for (int i = 0; i < nfds; i++)
        {
            SocketData *socket_data;
            // Check if the event is for the listen socket
            if ((socket_data = find_socket(listen_socket_manager, events[i].data.fd)) != NULL)
            {
                if ((handle_new_connection(socket_data->socket_fd, epoll_fd, client_socket_manager)) == -1)
                {
                    exit_code = 1;
                    goto cleanup;
                }
            }
            // Check if the event is for a client socket
            else if ((socket_data = find_socket(client_socket_manager, events[i].data.fd)) != NULL)
            {
                if (handle_client(socket_data, events[i]) <= 0)
                {
                    // The server itself does not exit even if the processing with the client ends abnormally.
                    close_with_retry(socket_data->socket_fd);
                    remove_socket(client_socket_manager, socket_data->socket_fd);
                }
            }
            // Check if the event is for the pipe
            else if (events[i].data.fd == pipe_fds[0])
            {
                int sig;
                while (read(pipe_fds[0], &sig, sizeof(sig)) == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    {
                        continue;
                    }
                    perror("server: read()");
                    exit_code = 1;
                    goto cleanup;
                }

                if (sig == SIGINT)
                {
                    puts("Received SIGINT, exiting...");
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (epoll_fd != -1)
    {
        close_with_retry(epoll_fd);
    }
    free_socket_manager(client_socket_manager);
    free_socket_manager(listen_socket_manager);
    close_with_retry(pipe_fds[0]);
    close_with_retry(pipe_fds[1]);

    if (exit_code != 0)
    {
        exit(exit_code);
    }
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
        int listen_socket_fd;
        if ((listen_socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)
        {
            perror("server: socket()");
            continue;
        }

        // Set the socket option to reuse the address
        if (setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("server: setsockopt(SOL_SOCKET, SO_REUSEADDR)");
            close_with_retry(listen_socket_fd);
            continue;
        }

        if (res->ai_family == PF_INET6)
        {
            // Set the socket option to allow only IPv6 connections
            if (setsockopt(listen_socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(int)) == -1)
            {
                perror("server: setsockopt(IPPROTO_IPV6, IPV6_V6ONLY)");
                close_with_retry(listen_socket_fd);
                continue;
            };
        }

        if (bind(listen_socket_fd, res->ai_addr, res->ai_addrlen) == -1)
        {
            perror("server: bind()");
            close_with_retry(listen_socket_fd);
            continue;
        }

        if (listen(listen_socket_fd, 5) == -1)
        {
            perror("server: listen()");
            close_with_retry(listen_socket_fd);
            return -1;
        }

        add_socket(listen_socket_manager, listen_socket_fd);
    }

    freeaddrinfo(res0);

    if (listen_socket_manager->top == listen_socket_manager->max_size - 1)
    {
        fputs("server: failed to bind any sockets\n", stderr);
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
    for (int i = 0; i < listen_socket_manager->max_size; i++)
    {
        int listen_socket_fd = listen_socket_manager->sockets[i].socket_fd;
        if (listen_socket_fd == -1)
        {
            continue;
        }

        // Get the current flags for the listen socket
        int flags;
        while ((flags = fcntl(listen_socket_fd, F_GETFL, 0)) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("server: fcntl()");
            return -1;
        }

        // Set the listen socket to non-blocking mode
        while (fcntl(listen_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("server: fcntl()");
            return -1;
        }

        // Add the listen socket to the epoll instance
        event.events = EPOLLIN;
        event.data.fd = listen_socket_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket_fd, &event) == -1)
        {
            perror("server: epoll_ctl()");
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Add the pipe to the epoll instance.
 *
 * This function sets the pipe read end to non-blocking mode and adds it to the epoll instance.
 *
 * @param[in] epoll_fd The file descriptor of the epoll instance.
 * @return The status of the process.
 * @retval 0 The pipe was successfully added to the epoll instance.
 * @retval -1 An error occurred during the process.
 */
int add_pipe_to_epoll(int epoll_fd)
{
    // Get the current flags for
    int flags;
    while ((flags = fcntl(pipe_fds[0], F_GETFL, 0)) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("server: fcntl()");
        return -1;
    }

    // Set the pipe read end to non-blocking mode
    while (fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("server: fcntl()");
        return -1;
    }

    // Set up a signal handler for SIGINT
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("server: sigaction()");
        return -1;
    }

    // Add the pipe read end to the epoll instance
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = pipe_fds[0];
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_fds[0], &event) == -1)
    {
        perror("server: epoll_ctl()");
        return -1;
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
 * @param[in] listen_socket_fd The file descriptor of the listening socket.
 * @param[in] epoll_fd The file descriptor of the epoll instance.
 * @param[in] client_socket_manager The socket manager for client sockets.
 * @return The status of the new connection.
 * @retval 0 The connection was successfully accepted and registered.
 * @retval -1 An error occurred during the process.
 */
int handle_new_connection(int listen_socket_fd, int epoll_fd, SocketManager *client_socket_manager)
{
    struct sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int conn_socket_fd;

    if ((conn_socket_fd = accept(listen_socket_fd, (struct sockaddr *)&client_addr, &addr_len)) == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            return 0;
        }
        else
        {
            perror("server: accept()");
            return -1;
        }
    }

    // Get the current flags for the connection socket
    int flags;
    while ((flags = fcntl(conn_socket_fd, F_GETFL, 0)) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("server: fcntl(conn_sock)");
        close_with_retry(conn_socket_fd);
        return -1;
    }

    // Set the connection socket to non-blocking mode
    while (fcntl(conn_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        perror("server: fcntl(conn_sock)");
        close_with_retry(conn_socket_fd);
        return -1;
    }

    // Fulfilled the maximum number of clients
    if (add_socket(client_socket_manager, conn_socket_fd) == -1)
    {
        puts("No more room for clients");
        close_with_retry(conn_socket_fd);
        return 0;
    }

    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT;
    event.data.fd = conn_socket_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_socket_fd, &event) == -1)
    {
        perror("server: epoll_ctl()");
        close_with_retry(conn_socket_fd);
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
 * @param[in] client_socket_data The socket data for the client socket.
 * @param[in] event The epoll event for the client socket.
 * @return The status of the client connection.
 * @retval 1 The client is still connected.
 * @retval 0 The client has disconnected.
 * @retval -1 An error occurred during communication.
 */
int handle_client(SocketData *client_socket_data, struct epoll_event event)
{
    // Check if the client socket is ready to read
    if (event.events & EPOLLIN)
    {
        ssize_t received_bytes;
        if ((received_bytes = recv(client_socket_data->socket_fd, client_socket_data->buffer, BUFFER_SIZE - 1, 0)) > 0)
        {
            printf("received_bytes: %ld (fd: %d)\n", received_bytes, client_socket_data->socket_fd);
            client_socket_data->buffer[received_bytes] = '\0'; // Null-terminate the string
        }
        else if (received_bytes == 0)
        {
            puts("Connection closed");
            return 0;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return 1;
            }
            perror("server: recv()");
            return -1;
        }
    }

    // Check if the client socket is ready to write
    if (event.events & EPOLLOUT)
    {
        ssize_t sent_bytes;
        if ((sent_bytes = send(client_socket_data->socket_fd, client_socket_data->buffer, strlen(client_socket_data->buffer), 0)) >= 0)
        {
            memmove(client_socket_data->buffer, client_socket_data->buffer + sent_bytes, strlen(client_socket_data->buffer) - sent_bytes);
            client_socket_data->buffer[strlen(client_socket_data->buffer) - sent_bytes] = '\0';
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            {
                return 1;
            }
            perror("server: send()");
            return -1;
        }
    }

    return 1;
}

/**
 * @brief Handle the SIGINT signal
 *
 * This function writes the signal number to the pipe to notify the main loop to exit.
 *
 * @param[in] sig The signal number.
 */
void handle_sigint(int sig)
{
    while (write(pipe_fds[1], &sig, sizeof(sig)) == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            continue;
        }
        perror("server: write()");
        exit(1);
    }
}
