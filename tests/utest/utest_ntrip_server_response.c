/* Test the private handshake parser without opening a network socket. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/stream/mrtk_stream.c"

static int check(const char* response, int accepted, int version) {
    ntrip_t* ntrip = calloc(1, sizeof(*ntrip));
    tcpcli_t* tcp = calloc(1, sizeof(*tcp));
    char message[MAXSTRMSG] = {0};
    int failed;
    if (!ntrip || !tcp) exit(2);
    tcp->svr.sock = (socket_t)-1;
    ntrip->tcp = tcp;
    ntrip->state = 1;
    ntrip->type = 0;
    ntrip->ver = NTRIP_VER_AUTO;
    ntrip->nb = (int)strlen(response);
    memcpy(ntrip->buff, response, ntrip->nb);
    failed = rspntrip_s(ntrip, message) != accepted || (accepted && ntrip->ver_neg != version);
    if (failed) fprintf(stderr, "Unexpected classification: %s", response);
    free(tcp);
    free(ntrip);
    return failed;
}

int main(void) {
    int failures = 0;
    failures += check("OK\r\n", 1, NTRIP_VER_1);
    failures += check("ICY 200 OK\r\n", 1, NTRIP_VER_1);
    failures += check("HTTP/1.1 200 OK\r\nServer: fixture\r\n\r\n", 1, NTRIP_VER_2);
    failures += check("HTTP/1.1 200 OK\r\n", 0, 0);
    failures += check("HTTP/1.1 200 Success\r\nX-Test: OK\r\n\r\n", 1, NTRIP_VER_2);
    failures += check("HTTP/1.1 401 Not OK\r\n\r\n", 0, 0);
    printf("NTRIP server response failures: %d\n", failures);
    return failures != 0;
}
