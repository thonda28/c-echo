#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

#define BUFFER_SIZE 256

int create_connected_socket(const char *ip, const char *port);

int main(int argc, char **argv)
{
    // Check if the IP address and port number are provided
    if (argc != 3)
    {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    // Create a connected socket
    int socket_fd;
    if ((socket_fd = create_connected_socket(argv[1], argv[2])) == -1)
    {
        puts("Failed to create a connected socket\n");
        exit(1);
    }

    while (1)
    {
        char buf[BUFFER_SIZE];
        if (fgets(buf, BUFFER_SIZE, stdin) == NULL)
        {
            // Check if fgets() reached EOF
            if (feof(stdin))
            {
                puts("EOF detected\n");
                break;
            }
            else
            {
                perror("client: fgets()");
                close(socket_fd);
                exit(1);
            }
        }

        // Send the input to the server
        size_t bytes_read = strlen(buf);
        size_t total_sent = 0;
        while (total_sent < bytes_read)
        {
            ssize_t sent_bytes = send(socket_fd, buf + total_sent, bytes_read - total_sent, 0);
            if (sent_bytes == -1)
            {
                perror("client: send()");
                close(socket_fd);
                exit(1);
            }
            total_sent += sent_bytes;
        }

        // Receive the response from the server
        size_t total_received = 0;
        while (total_received < bytes_read)
        {
            ssize_t received_bytes = recv(socket_fd, buf + total_received, bytes_read - total_received, 0);
            if (received_bytes == -1)
            {
                perror("client: recv()");
                close(socket_fd);
                exit(1);
            }
            else if (received_bytes == 0)
            {
                puts("Connection closed by server\n");
                close(socket_fd);
                exit(1);
            }
            total_received += received_bytes;
        }

        // Print the received data
        fwrite(buf, 1, total_received, stdout);
    }

    close(socket_fd);

    return 0;
}

/**
 * @brief Create a connected socket to the specified IP address and port.
 *
 * This function creates a socket, connects it to the specified IP address and port,
 * and returns the connected socket file descriptor.
 *
 * @param[in] ip The IP address to connect to.
 * @param[in] port The port number to connect to.
 * @return The file descriptor of the connected socket.
 * @retval -1 An error occurred during the process.
 */
int create_connected_socket(const char *ip, const char *port_str)
{
    // Parse the IP address
    struct sockaddr_in server_addr4;
    struct sockaddr_in6 server_addr6;
    memset(&server_addr4, 0, sizeof(server_addr4));
    memset(&server_addr6, 0, sizeof(server_addr6));
    int is_ipv4 = inet_pton(PF_INET, ip, &server_addr4.sin_addr);
    int is_ipv6 = inet_pton(PF_INET6, ip, &server_addr6.sin6_addr);
    if (is_ipv4 <= 0 && is_ipv6 <= 0)
    {
        printf("Invalid IP address: %s\n", ip);
        return -1;
    }

    // Parse the port number
    int port;
    if ((port = parse_port(port_str)) == -1)
    {
        printf("Invalid port number: %s\n", port_str);
        return -1;
    }

    // Create a socket and connect to the server
    int socket_fd;
    if (is_ipv4 == 1)
    {
        if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        {
            perror("client: socket()");
            return -1;
        }

        server_addr4.sin_family = PF_INET;
        server_addr4.sin_port = htons(port);

        if (connect(socket_fd, (struct sockaddr *)&server_addr4, sizeof(server_addr4)) == -1)
        {
            perror("client: connect() using IPv4");
            close(socket_fd);
            return -1;
        }
    }
    else if (is_ipv6 == 1)
    {
        if ((socket_fd = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
        {
            perror("client: socket() using IPv6");
            return -1;
        }

        server_addr6.sin6_family = PF_INET6;
        server_addr6.sin6_port = htons(port);

        if (connect(socket_fd, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) == -1)
        {
            perror("client: connect() using IPv6");
            close(socket_fd);
            return -1;
        }
    }
    else
    {
        puts("Reached the unreachable");
        return -1;
    }

    printf("Connected to %s, %d\n", ip, port);
    return socket_fd;
}
