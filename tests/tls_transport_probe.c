/* Exercise the actual HTTPS transport with loopback-only DNS/TLS endpoints. */
#include "update_transport.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned dns_port, tls_port, remaining;
int __real_connect(int, const struct sockaddr *, socklen_t);
int __wrap_connect(int fd, const struct sockaddr *address, socklen_t size)
{
    assert(size == sizeof(struct sockaddr_in) && address->sa_family == AF_INET);
    struct sockaddr_in local = *(const struct sockaddr_in *)address;
    unsigned port = ntohs(local.sin_port);
    assert(port == 53 || port == 443);
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    local.sin_port = htons(port == 53 ? dns_port : tls_port);
    return __real_connect(fd, (const struct sockaddr *)&local, sizeof local);
}
static bool wait_step(void)
{
    if (!remaining)
        return false;
    remaining--;
    struct timespec pause = {.tv_nsec = 1000000};
    nanosleep(&pause, NULL);
    return true;
}
int main(int argc, char **argv)
{
    assert(argc == 6);
    dns_port = (unsigned)strtoul(argv[1], NULL, 10);
    tls_port = (unsigned)strtoul(argv[2], NULL, 10);
    remaining = (unsigned)strtoul(argv[4], NULL, 10);
    unsigned char ca[16384];
    FILE *file = fopen(argv[3], "rb");
    assert(file);
    size_t bytes = fread(ca, 1, sizeof ca - 1, file);
    assert(!ferror(file) && feof(file) && !fclose(file));
    ca[bytes] = 0;
    char tag[UPDATE_TAG_CAP];
    bool success = update_fetch(ca, bytes + 1, htonl(INADDR_LOOPBACK), wait_step, tag);
    bool expected = atoi(argv[5]) != 0;
    assert(success == expected);
    assert(success ? !strcmp(tag, "v1.2.1") : !*tag);
    return 0;
}
