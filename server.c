#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // TODO: Refactor redundant error handling
    int result = 0;

    int listen_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1)
    {
        perror("server: socket()");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    result = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (result == -1)
    {
        perror("server: bind()");
        close(listen_sock);
        exit(1);
    }

    result = listen(listen_sock, 0);
    if (result == -1)
    {
        perror("server: listen()");
        close(listen_sock);
        exit(1);
    }

    while (1)
    {
        unsigned int client_len;
        struct sockaddr_in client_addr;
        int conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (conn_sock == -1)
        {
            perror("server: accept()");
            close(listen_sock);
            exit(1);
        }

        char buf[256] = {0};
        result = recv(conn_sock, buf, sizeof(buf) - 1, 0);
        if (result == -1)
        {
            perror("server: recv()");
            close(conn_sock);
            close(listen_sock);
            exit(1);
        }

        result = send(conn_sock, buf, sizeof(buf), 0);
        if (result == -1)
        {
            perror("server: send()");
            close(conn_sock);
            close(listen_sock);
            exit(1);
        }

        close(conn_sock);
    }

    close(listen_sock);

    return 0;
}
