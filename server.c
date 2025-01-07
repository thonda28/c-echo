#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int listen_sock;
    if ((listen_sock = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
    {
        perror("server: socket()");
        exit(1);
    }

    struct sockaddr_in6 server_addr6;
    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = PF_INET6;
    server_addr6.sin6_port = htons(8080);
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

    while (1)
    {
        struct sockaddr_in6 client_addr6;
        socklen_t addr_len = sizeof(client_addr6);
        int conn_sock;
        if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr6, &addr_len)) == -1)
        {
            perror("server: accept()");
            close(listen_sock);
            exit(1);
        }

        char buf[256];
        ssize_t received_bytes;
        while ((received_bytes = recv(conn_sock, buf, sizeof(buf) - 1, 0)) > 0)
        {
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
            perror("server: recv()");
            close(conn_sock);
            close(listen_sock);
            exit(1);
        }

        close(conn_sock);
    }

    close(listen_sock);

    return 0;
}
