#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int listen_sock;
    if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("server: socket()");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
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
        unsigned int client_len;
        struct sockaddr_in client_addr;
        int conn_sock;
        if ((conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len)) == -1)
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
