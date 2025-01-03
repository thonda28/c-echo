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

    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = PF_INET;
    receiver_addr.sin_addr.s_addr = INADDR_ANY;
    receiver_addr.sin_port = htons(8080); // host to network short
    result = bind(listen_sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));
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

    for (;;)
    {
        int sender_len;
        struct sockaddr_in sender_addr;
        int conn_sock = accept(listen_sock, (struct sockaddr *)&sender_addr, &sender_len);
        if (conn_sock == -1)
        {
            perror("server: accept()");
            close(listen_sock);
            exit(1);
        }

        char buf[256];
        result = recv(conn_sock, buf, sizeof(buf) - 1, 0);
        if (result == -1)
        {
            perror("server: recv()");
            close(conn_sock);
            close(listen_sock);
            exit(1);
        }

        printf("%s\n", buf);

        close(conn_sock);
    }

    close(listen_sock);

    return 0;
}
