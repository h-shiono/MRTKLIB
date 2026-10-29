/*------------------------------------------------------------------------------
 * utest_stream_bytes.c : 64-bit stream byte counters (#368)
 *
 * The byte counters of stream_t were uint32_t and strsum() / strsvrstat()
 * returned them as int, so a relay went negative after 2 GiB and wrapped at
 * 4 GiB. Sets counters beyond 2^32 and checks that strsum() returns them
 * exactly; the rates stay int.
 *
 * Explicit CHECK macro (not assert) so it is robust under -DNDEBUG.
 *-----------------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "mrtklib/rtklib.h"

static int fails = 0;

#define CHECK(cond, msg)                 \
    do {                                 \
        if (!(cond)) {                   \
            printf("FAIL: %s\n", (msg)); \
            fails++;                     \
        }                                \
    } while (0)

int main(void) {
    stream_t* s = (stream_t*)calloc(1, sizeof(stream_t));
    const uint64_t in = 5000000000ULL, out = 4294967296ULL + 123; /* > 2^32 */
    uint64_t inb = 0, outb = 0;
    int inr = -1, outr = -1;

    if (!s) {
        printf("FAIL: alloc\n");
        return 1;
    }
    strinit(s);
    s->inb = in;
    s->outb = out;
    s->inr = 110110;
    s->outr = 2400;
    strsum(s, &inb, &inr, &outb, &outr);
    printf("strsum: inb=%" PRIu64 " inr=%d outb=%" PRIu64 " outr=%d\n", inb, inr, outb, outr);
    CHECK(inb == in, "input bytes beyond 2^32 returned exactly");
    CHECK(outb == out, "output bytes beyond 2^32 returned exactly");
    CHECK(inr == 110110 && outr == 2400, "rates unchanged");
    strsum(s, NULL, NULL, NULL, NULL); /* NULL outputs are allowed */
    free(s);

    printf("%s\n", fails ? "FAILED" : "all checks passed");
    return fails ? 1 : 0;
}
