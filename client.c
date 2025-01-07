#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int sock;
    if ((sock = socket(PF_INET6, SOCK_STREAM, 0)) == -1)
    {
        perror("client: socket()");
        exit(1);
    }

    struct sockaddr_in6 server_addr6;
    memset(&server_addr6, 0, sizeof(server_addr6));
    server_addr6.sin6_family = PF_INET6;
    server_addr6.sin6_port = htons(8080);
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

    char buf[256];
    fgets(buf, sizeof(buf), stdin); // Null-terminate the string

    size_t len = strlen(buf);
    if (send(sock, buf, len, 0) == -1)
    {
        perror("client: send()");
        close(sock);
        exit(1);
    }

    if (recv(sock, buf, sizeof(buf) - 1, 0) == -1)
    {
        perror("client: recv()");
        close(sock);
        exit(1);
    }

    printf("%s", buf);

    close(sock);

    return 0;
}
