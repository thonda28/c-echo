#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils.h"

#define MAX_CLIENTS 30

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

    fd_set read_fds;
    int client_sockets[MAX_CLIENTS] = {0};
    int max_fd;
    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(listen_sock, &read_fds);
        max_fd = listen_sock;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int fd = client_sockets[i];
            if (fd > 0)
                FD_SET(fd, &read_fds);
            if (fd > max_fd)
                max_fd = fd;
        }

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) // no timeout
        {
            perror("server: select()");
            close(listen_sock);
            exit(1);
        }

        if (FD_ISSET(listen_sock, &read_fds))
        {
            struct sockaddr_in6 client_addr6;
            socklen_t addr_len = sizeof(client_addr6);
            int conn_sock;
            if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr6, &addr_len)) == -1)
            {
                if (errno != EAGAIN)
                {
                    perror("server: accept()");
                    close(listen_sock);
                    exit(1);
                }
            }

            bool is_client_added = false;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = conn_sock;
                    is_client_added = true;
                    break;
                }
            }

            if (is_client_added)
            {
                char client_ip[INET6_ADDRSTRLEN];
                inet_ntop(PF_INET6, &client_addr6.sin6_addr, client_ip, sizeof(client_ip));
                printf("Connection from %s, %d\n", client_ip, ntohs(client_addr6.sin6_port));
            }
            else
            {
                printf("No more room for clients\n");
                close(conn_sock);
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int conn_sock = client_sockets[i];
            if (!FD_ISSET(conn_sock, &read_fds))
                continue;

            if (fcntl(conn_sock, F_SETFL, O_NONBLOCK) == -1)
            {
                perror("server: fcntl(conn_sock)");
                close(conn_sock);
                close(listen_sock);
                exit(1);
            }

            char buf[256];
            ssize_t received_bytes;
            while ((received_bytes = recv(conn_sock, buf, sizeof(buf) - 1, 0)) > 0)
            {
                printf("received_bytes: %ld\n", received_bytes);
                buf[received_bytes] = '\0'; // Null-terminate the string
                if (send(conn_sock, buf, received_bytes, 0) == -1)
                {
                    perror("server: send()");
                    close(conn_sock);
                    close(listen_sock);
                    exit(1);
                }
            }

            if (received_bytes == -1)
            {
                if (errno != EAGAIN)
                {
                    perror("server: recv()");
                    close(conn_sock);
                    close(listen_sock);
                    exit(1);
                }
            }
            if (received_bytes == 0)
            {
                printf("Connection closed\n");
                close(conn_sock);
                client_sockets[i] = 0;
            }
        }
    }

    // close(listen_sock);

    return 0;
}
