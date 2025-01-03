#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    // TODO: Refactor redundant error handling
    int result = 0;

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("client: socket()");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(8080);
    result = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (result == -1)
    {
        perror("client: connect()");
        close(sock);
        exit(1);
    }

    char buf[256];
    fgets(buf, sizeof(buf) - 1, stdin);

    result = send(sock, buf, sizeof(buf), 0);
    if (result == -1)
    {
        perror("client: connect()");
        close(sock);
        exit(1);
    }

    close(sock);

    return 0;
}
