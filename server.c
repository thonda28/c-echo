#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int listen_sock = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in receiver_addr;
    receiver_addr.sin_family = PF_INET;
    receiver_addr.sin_addr.s_addr = INADDR_ANY;
    receiver_addr.sin_port = htons(8080); // host to network short
    bind(listen_sock, (struct sockaddr *)&receiver_addr, sizeof(receiver_addr));

    listen(listen_sock, 0);

    int sender_len;
    struct sockaddr_in sender_addr;
    int conn_sock = accept(listen_sock, (struct sockaddr *)&sender_addr, &sender_len);

    char buf[256];
    recv(conn_sock, buf, sizeof(buf) - 1, 0);
    printf("%s\n", buf);

    close(listen_sock);
    close(conn_sock);

    return 0;
}
