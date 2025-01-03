#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int sock = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in client_addr;
    client_addr.sin_family = PF_INET;
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client_addr.sin_port = htons(8080);
    connect(sock, (struct sockaddr *)&client_addr, sizeof(client_addr));

    char buf[256];
    strcpy(buf, "hello");

    send(sock, buf, sizeof(buf), 0);

    close(sock);

    return 0;
}
