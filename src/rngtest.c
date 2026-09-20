/*
 * How does cc65's rand() behave after srand()?
 *
 * The encounter rolls were coming up empty far more often than 8 percent
 * should allow. This counts, for four different seeds, how many of 200
 * rolls land under 8, 25 and 50. Expected: about 16, 50 and 100.
 */

#include <conio.h>
#include <stdlib.h>

#include <q4k/probe.h>

static const unsigned int seeds[4] = { 0, 3160, 13859, 40000 };

int main(void)
{
    unsigned char s, i, a, b, c, r;

    for (s = 0; s < 4; ++s) {
        srand(seeds[s]);
        a = b = c = 0;
        for (i = 0; i < 200; ++i) {
            r = (unsigned char)(rand() % 100);
            if (r < 8)  ++a;
            if (r < 25) ++b;
            if (r < 50) ++c;
        }
        PROBE_SCRATCH[s * 3 + 0] = a;
        PROBE_SCRATCH[s * 3 + 1] = b;
        PROBE_SCRATCH[s * 3 + 2] = c;
    }

    probe_stage(1);
    cgetc();
    return 0;
}
