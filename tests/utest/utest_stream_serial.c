/*------------------------------------------------------------------------------
 * utest_stream_serial.c : serial stream write byte count (#343)
 *
 * A pseudo-terminal pair stands in for the serial port: the slave side is
 * opened as an STR_SERIAL stream, the master side counts what arrives.
 * strwrite() must return the bytes written (writeserial() used to return 0
 * for every write), and strsum() / strstat() must see them as output.
 *
 * Busy port: with the master side not read, writes continue until the pty
 * buffer is full. The first write that does not fit is short, later ones get
 * EAGAIN. Neither may mark the port as failed (strstat() -1), and the
 * returned counts must add up to exactly what the master then drains.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#define _XOPEN_SOURCE 600 /* posix_openpt, grantpt, unlockpt, ptsname */
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mrtklib/rtklib.h"

#define NWRITE 10
#define NBYTE 100
#define NBUSY 4096   /* bytes per write while filling the pty buffer */
#define MAXBUSY 4096 /* write attempts: far beyond any pty buffer */

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

/* drain the master side; returns the bytes read */
static long drain(int master) {
    uint8_t rbuf[8192];
    struct pollfd pfd;
    long got = 0;
    int ret;

    pfd.fd = master;
    pfd.events = POLLIN;
    while (poll(&pfd, 1, 500) > 0) {
        ret = (int)read(master, rbuf, sizeof(rbuf));
        if (ret <= 0) break;
        got += ret;
    }
    return got;
}

/* busy port: fill the pty buffer, then check counts and state */
static void test_busy(stream_t* s, int master, uint64_t outb0) {
    uint8_t buf[NBUSY];
    char msg[256], smsg[MAXSTRMSG] = "";
    long sum = 0, got;
    uint64_t inb, outb;
    int i, ret, nshort = 0, nzero = 0, inr, outr, stat;

    memset(buf, 'B', sizeof(buf));
    for (i = 0; i < MAXBUSY && nzero < 3; i++) {
        ret = strwrite(s, buf, NBUSY);
        if (ret < 0) break;
        if (ret == 0) nzero++;
        if (ret > 0 && ret < NBUSY) nshort++;
        sum += ret;
    }
    stat = strstat(s, smsg);
    strsum(s, &inb, &inr, &outb, &outr);
    got = drain(master);

    printf("busy: written=%ld short=%d eagain=%d strstat=%d outb=%" PRIu64 " received=%ld\n", sum, nshort, nzero, stat,
           outb, got);
    CHECK(nzero > 0, "busy: the pty buffer filled up (EAGAIN reached)");
    snprintf(msg, sizeof(msg), "busy: strwrite returned %ld bytes, master received %ld", sum, got);
    CHECK(sum == got, msg);
    snprintf(msg, sizeof(msg), "busy: strsum outb=%" PRIu64 ", expected %" PRIu64, outb, outb0 + (uint64_t)sum);
    CHECK(outb == outb0 + (uint64_t)sum, msg);
    snprintf(msg, sizeof(msg), "busy: strstat=%d, a full buffer must not mark the port failed", stat);
    CHECK(stat >= 0, msg);

    /* the port keeps working once drained */
    ret = strwrite(s, buf, NBYTE);
    stat = strstat(s, smsg); /* "active" expires shortly after the last write */
    got = drain(master);
    snprintf(msg, sizeof(msg), "busy: write after drain returned %d, strstat=%d, received %ld", ret, stat, got);
    CHECK(ret == NBYTE && stat == 3 && got == NBYTE, msg);
}

int main(void) {
    stream_t* s = (stream_t*)calloc(1, sizeof(stream_t));
    uint8_t buf[NBYTE];
    char path[256], msg[256], smsg[MAXSTRMSG] = "";
    const char* name;
    uint64_t inb, outb;
    int master, i, ret, sum = 0, got = 0, inr, outr, stat;

    if (!s || (master = posix_openpt(O_RDWR | O_NOCTTY)) < 0 || grantpt(master) || unlockpt(master) ||
        !(name = ptsname(master)) || strncmp(name, "/dev/", 5)) {
        printf("FAIL: pty setup\n");
        return 1;
    }
    snprintf(path, sizeof(path), "%s:115200:8:n:1:off", name + 5); /* stropen adds /dev/ */

    strinit(s);
    if (!stropen(s, STR_SERIAL, STR_MODE_RW, path)) {
        printf("FAIL: stropen %s: %s\n", path, s->msg);
        return 1;
    }
    memset(buf, 'A', sizeof(buf));
    for (i = 0; i < NWRITE; i++) {
        ret = strwrite(s, buf, NBYTE);
        sum += ret;
    }
    strsum(s, &inb, &inr, &outb, &outr);
    stat = strstat(s, smsg);
    got = (int)drain(master);

    test_busy(s, master, outb);
    strclose(s);
    close(master);
    free(s);

    printf("strwrite sum=%d strsum outb=%" PRIu64 " strstat=%d received=%d\n", sum, outb, stat, got);
    snprintf(msg, sizeof(msg), "pty received %d bytes, expected %d", got, NWRITE * NBYTE);
    CHECK(got == NWRITE * NBYTE, msg);
    snprintf(msg, sizeof(msg), "strwrite returned %d bytes in total, expected %d", sum, got);
    CHECK(sum == got, msg);
    snprintf(msg, sizeof(msg), "strsum outb=%" PRIu64 ", expected %d", outb, got);
    CHECK(outb == (uint64_t)got, msg);
    snprintf(msg, sizeof(msg), "strstat=%d after writes, expected 3 (active)", stat);
    CHECK(stat == 3, msg);

    printf("%s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
