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
    int sock;
    if ((sock = create_connected_socket(argv[1], argv[2])) == -1)
    {
        puts("Failed to create a connected socket\n");
        exit(1);
    }

    while (1)
    {
        char buf[BUFFER_SIZE];
        fgets(buf, BUFFER_SIZE, stdin); // Null-terminate the string

        size_t len = strlen(buf);
        ssize_t sent_bytes = send(sock, buf, len, 0);
        if (sent_bytes == -1)
        {
            perror("client: send()");
            close(sock);
            exit(1);
        }
        else if (sent_bytes == 0)
        {
            printf("Connection closed by server\n");
            close(sock);
            exit(0);
        }

        ssize_t received_bytes = recv(sock, buf, BUFFER_SIZE - 1, 0);
        if (received_bytes == -1)
        {
            perror("client: recv()");
            close(sock);
            exit(1);
        }
        else if (received_bytes == 0)
        {
            printf("Connection closed by server\n");
            close(sock);
            exit(0);
        }

        printf("%s", buf);
    }

    close(sock);

    return 0;
}

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
    int sock;
    if (is_ipv4 == 1)
    {
        if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
        {
            perror("client: socket()");
            return -1;
        }

        server_addr4.sin_family = PF_INET;
        server_addr4.sin_port = htons(port);

        if (connect(sock, (struct sockaddr *)&server_addr4, sizeof(server_addr4)) == -1)
        {
            perror("client: connect() using IPv4");
            close(sock);
            return -1;
        }
    }
    else if (is_ipv6 == 1)
    {
        if ((sock = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
        {
            perror("client: socket() using IPv6");
            return -1;
        }

        server_addr6.sin6_family = PF_INET6;
        server_addr6.sin6_port = htons(port);

        if (connect(sock, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) == -1)
        {
            perror("client: connect() using IPv6");
            close(sock);
            return -1;
        }
    }
    else
    {
        puts("Reached the unreachable");
        return -1;
    }

    printf("Connected to %s, %d\n", ip, port);
    return sock;
}
