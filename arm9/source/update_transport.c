/* HTTPS transport. Platform scheduling and Wi-Fi ownership remain in main.c. */
#ifndef HOST_PREVIEW
#include "update_transport.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

/* A single bounded DNS A query avoids libc's blocking DNS retry loop. TLS still
 * authenticates the hostname; no address returned by DNS is trusted on its own. */
static bool dns_name(const unsigned char *data, size_t size, size_t *offset)
{
    for (unsigned labels = 0; labels < 128 && *offset < size; labels++) {
        unsigned n = data[(*offset)++];
        if (!n)
            return true;
        if ((n & 0xc0) == 0xc0) {
            if (*offset >= size)
                return false;
            unsigned pointer = ((n & 63) << 8) | data[(*offset)++];
            return pointer < size;
        }
        if (n > 63 || n > size - *offset)
            return false;
        *offset += n;
    }
    return false;
}
static unsigned word(const unsigned char *p)
{
    return p[0] * 256u + p[1];
}
static uint32_t resolve(uint32_t dns, bool (*wait)(void))
{
    unsigned char query[] = {0x47, 0x4c, 1,   0,   0,   1, 0,   0,   0,   0,   0,
                             0,    3,    'a', 'p', 'i', 6, 'g', 'i', 't', 'h', 'u',
                             'b',  3,    'c', 'o', 'm', 0, 0,   1,   0,   1};
    uint32_t address = 0;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return 0;
    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(53)};
    server.sin_addr.s_addr = dns;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        connect(fd, (struct sockaddr *)&server, sizeof server) < 0)
        goto done;
    unsigned char data[512];
    unsigned tick = 0;
    while (wait()) {
        if (tick++ % 120 == 0)
            send(fd, query, sizeof query, 0);
        int n = recv(fd, data, sizeof data, 0);
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            continue;
        if (n < 12 || word(data) != 0x474c || (word(data + 2) & 0x820f) != 0x8000 ||
            word(data + 4) != 1)
            break;
        size_t offset = 12;
        if (n < (int)sizeof query || memcmp(data + 12, query + 12, sizeof query - 12))
            break;
        if (!dns_name(data, n, &offset) || offset + 4 > (size_t)n)
            break;
        offset += 4;
        for (unsigned i = 0; i < word(data + 6); i++) {
            if (!dns_name(data, n, &offset) || offset + 10 > (size_t)n)
                break;
            unsigned type = word(data + offset), klass = word(data + offset + 2);
            unsigned length = word(data + offset + 8);
            offset += 10;
            if (offset + length > (size_t)n)
                break;
            if (type == 1 && klass == 1 && length == 4) {
                memcpy(&address, data + offset, 4);
                break;
            }
            offset += length;
        }
        break;
    }
done:
    close(fd);
    return address;
}
static bool pending(int result)
{
    return result == MBEDTLS_ERR_SSL_WANT_READ || result == MBEDTLS_ERR_SSL_WANT_WRITE;
}
bool update_fetch(const char *ca_file, uint32_t dns, bool (*wait)(void), char tag[UPDATE_TAG_CAP])
{
    bool success = false;
    tag[0] = 0;
    if (!update_trust_available(ca_file))
        return false;
    char *response = malloc(UPDATE_RESPONSE_CAP);
    if (!response)
        return false;
    mbedtls_net_context net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt ca;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_net_init(&net);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&ca);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&random);
    if (!wait() || mbedtls_x509_crt_parse_file(&ca, ca_file) != 0)
        goto done;
    uint32_t address = resolve(dns, wait);
    if (!address)
        goto done;
    if (!wait() || mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy,
                                         (const unsigned char *)"Gloam updates", 13))
        goto done;
    net.fd = socket(AF_INET, SOCK_STREAM, 0);
    if (net.fd < 0 || fcntl(net.fd, F_SETFL, O_NONBLOCK) < 0)
        goto done;
    struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(443)};
    server.sin_addr.s_addr = address;
    int result = connect(net.fd, (struct sockaddr *)&server, sizeof server);
    if (result < 0 && errno != EINPROGRESS)
        goto done;
    while (result < 0) {
        if (!wait())
            goto done;
        fd_set writes;
        FD_ZERO(&writes);
        FD_SET(net.fd, &writes);
        struct timeval timeout = {0};
        result = select(net.fd + 1, NULL, &writes, NULL, &timeout);
        if (result < 0)
            goto done;
        if (!result) {
            result = -1;
            continue;
        }
        int error = 0;
        socklen_t size = sizeof error;
        if (getsockopt(net.fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error)
            goto done;
    }
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT))
        goto done;
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf, &ca, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &random);
    /* TLS 1.2 avoids unnecessary TLS 1.3 negotiation work on this small client. */
    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    if (mbedtls_ssl_setup(&ssl, &conf) || mbedtls_ssl_set_hostname(&ssl, "api.github.com"))
        goto done;
    mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, NULL);
    while (!mbedtls_ssl_is_handshake_over(&ssl)) {
        if (!wait())
            goto done;
        result = mbedtls_ssl_handshake_step(&ssl);
        if (result && !pending(result))
            goto done;
    }
    if (mbedtls_ssl_get_verify_result(&ssl))
        goto done;
    static const unsigned char request[] =
        "GET /repos/nextcode4u/gloam-shell-ndx/releases?per_page=1 HTTP/1.0\r\n"
        "Host: api.github.com\r\nUser-Agent: Gloam-Shell-NDX\r\n"
        "Accept: application/vnd.github+json\r\nAccept-Encoding: identity\r\n"
        "Connection: close\r\n\r\n";
    size_t sent = 0;
    while (sent < sizeof request - 1) {
        if (!wait())
            goto done;
        result = mbedtls_ssl_write(&ssl, request + sent, sizeof request - 1 - sent);
        if (result > 0)
            sent += result;
        else if (!pending(result))
            goto done;
    }
    size_t received = 0;
    while (received < UPDATE_RESPONSE_CAP - 1) {
        if (!wait())
            goto done;
        size_t room = UPDATE_RESPONSE_CAP - 1 - received;
        if (room > 512)
            room = 512;
        result = mbedtls_ssl_read(&ssl, (unsigned char *)response + received, room);
        if (result > 0)
            received += result;
        else if (!result || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            success = update_response_tag(response, received, tag);
            break;
        } else if (!pending(result))
            goto done;
    }
done:
    mbedtls_net_free(&net);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&ca);
    mbedtls_ctr_drbg_free(&random);
    mbedtls_entropy_free(&entropy);
    free(response);
    if (!success)
        tag[0] = 0;
    return success;
}
#endif
