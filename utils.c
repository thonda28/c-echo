#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

long parse_port(const char *port_str)
{
    char *endptr;
    long port = strtol(port_str, &endptr, 10);
    if (endptr == port_str || *endptr != '\0')
    {
        printf("server: cannot convert %s to a port number\n", port_str);
        exit(1);
    }
    else if (errno == ERANGE || port <= 0 || port > 65535)
    {
        printf("server: port number [%s] out of range\n", port_str);
        exit(1);
    }
    return port;
}
