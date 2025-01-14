#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

int main(int argc, char **argv)
{
    // Check if the IP address and port number are provided
    if (argc != 3)
    {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    // Initialize the error number
    errno = 0;

    // Parse the port number
    long port = parse_port(argv[2]);
    printf("Port: %ld\n", port);

    int sock;
    if ((sock = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
    {
        perror("client: socket()");
        exit(1);
    }

    struct sockaddr_in6 server_addr6;
    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = PF_INET6;
    server_addr6.sin6_port = htons(port);
    if (inet_pton(PF_INET6, "::1", &server_addr6.sin6_addr) <= 0)
    {
        perror("client: inet_pton()");
        close(sock);
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&server_addr6, sizeof(server_addr6)) == -1)
    {
        perror("client: connect()");
        close(sock);
        exit(1);
    }

    while (1)
    {
        char buf[256];
        fgets(buf, sizeof(buf), stdin); // Null-terminate the string

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

        ssize_t received_bytes = recv(sock, buf, sizeof(buf) - 1, 0);
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
