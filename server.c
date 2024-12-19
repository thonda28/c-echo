#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int sender_len;
    struct sockaddr_in receiver_addr, sender_addr;

    int sock = socket(PF_INET, SOCK_STREAM, 0);

    receiver_addr.sin_family = PF_INET;
    receiver_addr.sin_addr.s_addr = INADDR_ANY;
    receiver_addr.sin_port = htons(8080);
    bind(sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));

    listen(sock, 1);

    int sock2 = accept(sock, (struct sockaddr *)&sender_addr, &sender_len);

    char buf[256];
    recv(sock2, buf, sizeof(buf), 0);
    printf("%s\n", buf);

    close(sock);
    close(sock2);

    return 0;
}
