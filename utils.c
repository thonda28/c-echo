#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

int parse_port(const char *port_str)
{
    char *endptr;
    errno = 0;
    long port = strtol(port_str, &endptr, 10);
    if (endptr == port_str || *endptr != '\0' || errno == ERANGE || port <= 0 || port > 65535)
    {
        return -1;
    }
    return (int)port;
}
